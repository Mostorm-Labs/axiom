#include "session.hpp"

#include <algorithm>
#include <canvas/control/build_info.hpp>
#include <canvas/semantic/codec.hpp>
#include <cmath>
#include <cstring>
#include <map>
#include <stdexcept>
#include <thread>
#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#define EMSCRIPTEN_KEEPALIVE
#endif
using namespace canvas;
namespace {
struct Lifecycle {
    runtime::RuntimeComposition* owner = nullptr;
    runtime::RuntimeComposition& composition() {
        return *owner;
    }
    runtime::RuntimeFacade& facade() {
        return owner->controlFacade();
    }
};
struct TextInstance {
    Lifecycle lifecycle;
    std::uint64_t request = 0, lastTextIntent = 0;
};
thread_local TextInstance* activeText = nullptr;
TextInstance* instance(std::uintptr_t h) {
    return h == 1 && activeText && activeText->lifecycle.owner ? activeText : nullptr;
}
} // namespace
#include "text_bridge.inl"
namespace canvas::sdk {
void validateTransportEnums(const Json& j) {
    const auto bounded = [](const Json& value, uint64_t max) {
        if (!value.is_number_integer() || value.get<uint64_t>() > max)
            throw std::invalid_argument("integer range");
    };
    const auto command = j.at("command").get<std::string>();
    if (command == "control") {
        const auto kind = j.at("kind").get<std::string>();
        if (kind == "tool")
            bounded(j.at("value"), 4);
        else if (kind == "shape")
            bounded(j.at("value"), 5);
        else if (kind == "selectionMode")
            bounded(j.at("value"), 2);
        else if (kind == "fit")
            bounded(j.value("target", Json(0)), 2);
        else if (kind == "preset")
            bounded(j.at("revision"), 0xffffffffULL);
    } else if (command == "multi") {
        const auto action = j.at("action").get<std::string>();
        if (action == "permission")
            bounded(j.at("mode"), 2);
        else if (action != "read" && action != "values" && action != "set" && action != "width")
            throw std::invalid_argument("multi action");
    } else if (command == "text" && j.value("action", std::string{}) == "intent")
        bounded(j.at("intent").at("kind"), 15);
    else if (command == "textEdit")
        bounded(j.at("kind"), 6);
    else if (command == "section")
        bounded(j.at("action"), 11);
    else if (command == "pointer")
        bounded(j.at("phase"), 3);
}
class Session::Impl {
  public:
    using J = Json;
    std::unique_ptr<runtime::RuntimeComposition> owner;
    std::unique_ptr<SessionPlatform> adapter;
    std::map<std::pair<std::uint64_t, std::uint64_t>, input::PointerKey> contacts;
    std::map<std::pair<std::uint64_t, std::uint64_t>, std::uint64_t> contactTimes;
    std::uint64_t sequence = 0;
    bool lost = false;
    std::vector<std::uint8_t> outputBytes;
    TextInstance textInstance;
    SessionCallbacks callbacks;
    const std::thread::id thread = std::this_thread::get_id();
    void notifyOperation(const std::uint8_t* data, std::size_t size) noexcept {
        if (callbacks.operation)
            callbacks.operation(callbacks.context, data, size);
    }
    void notifyDeliveryFailure() noexcept {
        if (callbacks.deliveryError)
            callbacks.deliveryError(callbacks.context);
    }
    static void localOperation(void* context,
                               const semantic::Operation& op,
                               const semantic::CanonicalCommitRecord&) noexcept {
        auto& self = *static_cast<Impl*>(context);
        try {
            auto encoded = semantic::SemanticCodec::encodeProtobufOperation(op);
            if (!encoded.ok()) {
                self.notifyDeliveryFailure();
                return;
            }
            self.notifyOperation(encoded.bytes.data(), encoded.bytes.size());
        } catch (...) {
            self.notifyDeliveryFailure();
        }
    }
    static J error(const char* code) {
        return {{"ok", false}, {"code", code}};
    }
    static std::uint32_t unsignedValue(const J& j, std::uint32_t max) {
        if (!j.is_number_integer())
            throw std::invalid_argument("integer required");
        const auto n = j.get<std::uint64_t>();
        if (n > max)
            throw std::invalid_argument("integer range");
        return static_cast<std::uint32_t>(n);
    }
    static std::vector<std::uint8_t> byteArray(const J& j) {
        if (!j.is_array() || j.size() > 1048576)
            throw std::invalid_argument("byte array");
        std::vector<std::uint8_t> result;
        result.reserve(j.size());
        for (const auto& v : j)
            result.push_back(static_cast<std::uint8_t>(unsignedValue(v, 255)));
        return result;
    }
    static J ids(const std::vector<foundation::ObjectId>& values) {
        J out = J::array();
        for (auto id : values)
            out.push_back(hexId(id));
        return out;
    }
    static std::vector<foundation::ObjectId> parseIds(const J& values) {
        std::vector<foundation::ObjectId> out;
        for (const auto& id : values)
            out.push_back(rawId(id));
        return out;
    }
    static J stampJson(const runtime::MultiSelectionStamp& s) {
        return {std::to_string(s.runtimeInstance),
                std::to_string(s.lifetimeGeneration),
                std::to_string(s.attachmentEpoch),
                std::to_string(s.selectionRevision),
                std::to_string(s.semanticGeneration),
                std::to_string(s.sceneGeneration),
                std::to_string(s.sceneRevision),
                std::to_string(s.permissionRevision)};
    }
    static runtime::MultiSelectionStamp parseStamp(const J& j) {
        if (!j.is_array() || j.size() != 8)
            throw std::invalid_argument("stamp");
        return {decimal(j[0]),
                decimal(j[1]),
                decimal(j[2]),
                decimal(j[3]),
                decimal(j[4]),
                decimal(j[5]),
                decimal(j[6]),
                decimal(j[7])};
    }
    static semantic::ColorValue color(const J& j) {
        auto c = j.get<std::array<float, 4>>();
        return {c[0], c[1], c[2], c[3]};
    }
    static J colorJson(semantic::ColorValue c) {
        return {c.r, c.g, c.b, c.a};
    }
    static J propertyJson(const semantic::PropertyValue& value) {
        return std::visit(
            [](const auto& v) -> J {
                using T = std::decay_t<decltype(v)>;
                if constexpr (std::is_same_v<T, bool>)
                    return {{"type", "boolean"}, {"value", v}};
                else if constexpr (std::is_same_v<T, float>)
                    return {{"type", "float"}, {"value", v}};
                else if constexpr (std::is_same_v<T, semantic::ColorValue>)
                    return {{"type", "color"}, {"value", colorJson(v)}};
                else if constexpr (std::is_same_v<T, semantic::FillStyleValue>) {
                    const auto* fill = std::get_if<semantic::SolidFill>(&v);
                    return {{"type", "fill"},
                            {"value", fill ? colorJson(fill->color) : J(nullptr)}};
                } else if constexpr (std::is_same_v<T, semantic::StrokeStyleValue>) {
                    const auto* s = std::get_if<semantic::SolidStroke>(&v);
                    if (!s)
                        return {{"type", "stroke"}, {"value", nullptr}};
                    J j = {{"color", colorJson(s->color)}, {"width", s->width}, {"cap", s->cap}};
                    if (const auto* m = std::get_if<semantic::MiterJoin>(&s->join))
                        j["join"] = {{"kind", "miter"}, {"limit", m->limit}};
                    else
                        j["join"] = {{"kind",
                                      std::holds_alternative<semantic::RoundJoin>(s->join)
                                          ? "round"
                                          : "bevel"}};
                    if (const auto* d = std::get_if<semantic::DashPattern>(&s->dash))
                        j["dash"] = {{"segments", d->segments}, {"offset", d->offset}};
                    else
                        j["dash"] = nullptr;
                    return {{"type", "stroke"}, {"value", j}};
                } else
                    return {{"type",
                             std::is_same_v<T, semantic::BlendModeValue> ? "blend" : "decoration"},
                            {"value", v}};
            },
            value);
    }
    static semantic::PropertyValue parseProperty(const J& j) {
        const auto type = j.at("type").get<std::string>();
        const auto& v = j.at("value");
        if (type == "boolean")
            return v.get<bool>();
        if (type == "float")
            return v.get<float>();
        if (type == "color")
            return color(v);
        if (type == "fill") {
            if (v.is_null())
                return semantic::FillStyleValue{semantic::NoFill{}};
            return semantic::FillStyleValue{semantic::SolidFill{color(v)}};
        }
        if (type == "blend")
            return static_cast<semantic::BlendModeValue>(unsignedValue(v, 1));
        if (type == "decoration")
            return static_cast<semantic::ConnectorDecorationValue>(unsignedValue(v, 2));
        if (type == "stroke") {
            if (v.is_null())
                return semantic::StrokeStyleValue{semantic::NoStroke{}};
            semantic::SolidStroke s;
            s.color = color(v.at("color"));
            s.width = v.at("width");
            s.cap = static_cast<semantic::StrokeCap>(unsignedValue(v.at("cap"), 3));
            const auto& join = v.at("join");
            const auto kind = join.at("kind").get<std::string>();
            if (kind == "miter")
                s.join = semantic::MiterJoin{join.at("limit")};
            else if (kind == "round")
                s.join = semantic::RoundJoin{};
            else if (kind == "bevel")
                s.join = semantic::BevelJoin{};
            else
                throw std::invalid_argument("join");
            if (!v.at("dash").is_null())
                s.dash =
                    semantic::DashPattern{v.at("dash").at("segments").get<std::vector<double>>(),
                                          v.at("dash").at("offset")};
            return semantic::StrokeStyleValue{s};
        }
        throw std::invalid_argument("property type");
    }
    static J tokenJson(const runtime::ResourceBindingToken& t) {
        return {{"resourceId", hexId(t.resourceId)},
                {"documentBinding", std::to_string(t.documentBinding)},
                {"providerEpoch", std::to_string(t.providerEpoch)},
                {"resourceVersion", std::to_string(t.resourceVersion)},
                {"expectedSha256", t.expectedSha256},
                {"requestId", std::to_string(t.requestId)},
                {"lifetimeGeneration", std::to_string(t.lifetimeGeneration)}};
    }
    static runtime::ResourceBindingToken parseResourceToken(const J& j) {
        return {rawId(j.at("resourceId")),
                decimal(j.at("documentBinding")),
                decimal(j.at("providerEpoch")),
                decimal(j.at("resourceVersion")),
                j.at("expectedSha256"),
                decimal(j.at("requestId")),
                decimal(j.at("lifetimeGeneration"))};
    }
    static semantic::ConnectorEndpoint endpoint(const J& j) {
        semantic::ConnectorEndpoint e;
        if (j.contains("point")) {
            auto p = j.at("point").get<std::array<double, 2>>();
            e.value = semantic::FreePointEndpoint{{p[0], p[1]}};
        } else {
            semantic::AttachedEndpoint a;
            a.target_object_id = rawId(j.at("objectId"));
            if (j.contains("port"))
                a.anchor = semantic::StablePortAnchor{unsignedValue(j.at("port"), 4)};
            else
                a.anchor = semantic::AutoPerimeterAnchor{};
            e.value = a;
        }
        return e;
    }
    static J endpointJson(const semantic::ConnectorEndpoint& e) {
        if (const auto* f = std::get_if<semantic::FreePointEndpoint>(&e.value))
            return {{"point", {f->point.x, f->point.y}}};
        const auto& a = std::get<semantic::AttachedEndpoint>(e.value);
        J j = {{"objectId", hexId(a.target_object_id)}};
        if (const auto* p = std::get_if<semantic::StablePortAnchor>(&a.anchor))
            j["port"] = p->port_id;
        return j;
    }
    static J imageContent(const semantic::ImageContent& c) {
        const auto r = c.source_rect.value_or(semantic::NormalizedRect{0, 0, 1, 1});
        return {{"resourceId", hexId(c.resource_id.value)},
                {"width", c.width},
                {"height", c.height},
                {"intrinsicWidth", c.intrinsic_width},
                {"intrinsicHeight", c.intrinsic_height},
                {"sourceRect", {r.x, r.y, r.width, r.height}}};
    }
    runtime::RuntimeFacade& facade() {
        if (!owner)
            throw std::runtime_error("CLOSED");
        return owner->controlFacade();
    }
    runtime::CanvasTargetKey target() {
        const auto t = facade().mountedCanvasTargets();
        if (t.empty())
            throw std::runtime_error("NOT_READY");
        return t.front();
    }
    J receipt(const runtime::CanvasControlRequest& r) {
        auto& f = facade();
        const auto queued = f.submitCanvasControl(r);
        if (queued.state != runtime::ProductControlState::kQueued || !owner->ownerPump())
            return error("ADMISSION_REJECTED");
        const auto done = f.canvasControlReceipt({r.clientId, r.requestId});
        if (!done)
            return error("RECEIPT_MISSING");
        J result = {{"ok",
                     done->state == runtime::CanvasControlReceiptState::kApplied ||
                         done->state == runtime::CanvasControlReceiptState::kDuplicate},
                    {"code", "COMMAND_REJECTED"},
                    {"state", done->state},
                    {"error", done->error},
                    {"requestId", std::to_string(r.requestId)},
                    {"revision", std::to_string(done->appliedRevision)}};
        if (done->section) {
            const auto& s = *done->section;
            result["section"] = {{"objectId", hexId(s.objectId)},
                                 {"operationId", hexId(s.operationId)},
                                 {"generation", std::to_string(s.semanticGeneration)},
                                 {"canonical", s.canonical},
                                 {"projection", s.projection},
                                 {"failure", s.failure}};
        }
        return result;
    }
    runtime::CanvasControlRequest request() {
        runtime::CanvasControlRequest r;
        r.target = target();
        r.clientId = 0x4A53;
        r.requestId = ++sequence;
        return r;
    }
    J state() {
        const auto s = facade().readCanvasControlSnapshot(target());
        const auto selected = facade().readSelectionSummary();
        const auto m = owner->surfaceMetrics();
        const auto o = owner->hostObservation();
        return {
            {"documentId", hexId(*owner->documentId())},
            {"target", jsonTarget(s.target)},
            {"generation", std::to_string(s.semanticGeneration)},
            {"selectionRevision", std::to_string(s.selectionRevision)},
            {"tool", s.tool},
            {"selectionMode", s.selectionAcquisitionMode},
            {"creationMode", s.creationMode},
            {"shape", s.creationPreset},
            {"eraserMode", s.eraserMode},
            {"objects", o ? o->objectCount : 0},
            {"activeContacts", contacts.size() + adapter->activeContacts()},
            {"selected", ids(selected.roots)},
            {"primary", hexId(selected.primary)},
            {"canUndo", s.canUndo},
            {"canRedo", s.canRedo},
            {"camera",
             {{"scale", s.cameraScale}, {"x", s.cameraTranslationX}, {"y", s.cameraTranslationY}}},
            {"surface",
             {{"available", m && m->available},
              {"width", m ? m->logicalWidth : 0},
              {"height", m ? m->logicalHeight : 0}}}};
    }
    J dispatch(const std::string& command, const J& j, const std::vector<std::uint8_t>& bytes) {
        using K = runtime::CanvasControlPayloadKind;
        if (command == "version") {
            const auto b = runtime::readRuntimeBuildInfo();
            return {{"ok", true},
                    {"apiVersion", 1},
                    {"sdkVersion", AXIOM_SDK_VERSION},
                    {"sourceRevision", b.sourceRevision},
                    {"dirty", b.dirty},
                    {"platform", b.platform}};
        }
        if (command == "open") {
            if (owner)
                return error("ALREADY_OPEN");
            owner = std::make_unique<runtime::RuntimeComposition>();
            bool opened = false;
            if (j.contains("restore")) {
                const auto& v = j.at("restore");
                runtime::DocumentRestoreRequest r;
                r.bytes = bytes;
                r.expectedDocumentId = rawId(j.at("documentId"));
                r.documentRevision = decimal(v.at("documentRevision"));
                r.recoveryFrontier = byteArray(v.at("recoveryFrontier"));
                r.checkpointDigest = byteArray(v.at("checkpointDigest"));
                r.resourceBinding = decimal(v.at("resourceBinding"));
                opened =
                    owner->openDocumentFromSnapshot(r) == runtime::DocumentRestoreStatus::kRestored;
            } else
                opened = owner->openDocument(rawId(j.at("documentId")));
            if (!opened) {
                owner.reset();
                return error("OPEN_REJECTED");
            }
            textInstance.lifecycle.owner = owner.get();
            auto bound = adapter->open(*owner, j);
            if (!bound.value("ok", false))
                return bound;
            if (!j.contains("restore") && j.value("builtinFonts", true) && !adapter->loadFonts())
                return error("FONT_LOAD_FAILED");
            if (j.contains("initialResources")) {
                if (j.contains("restore") || j.value("builtinFonts", true))
                    return error("RESOURCE_STARTUP_CONFLICT");
                const auto& spec = j.at("initialResources");
                semantic::ResourceManifest manifest;
                manifest.manifest_revision = decimal(spec.at("revision"));
                for (const auto& e : spec.at("entries")) {
                    semantic::ResourceManifestEntry entry;
                    entry.resource_id = semantic::ResourceId{rawId(e.at("resourceId"))};
                    entry.resource_kind =
                        static_cast<semantic::ResourceKind>(unsignedValue(e.at("kind"), 3));
                    entry.resource_revision = decimal(e.at("revision"));
                    entry.content_hash = e.at("sha256");
                    entry.byte_length = decimal(e.at("byteLength"));
                    entry.decode_version = unsignedValue(e.at("decodeVersion"), 0xFFFFFFFFU);
                    entry.layout_version = unsignedValue(e.at("layoutVersion"), 0xFFFFFFFFU);
                    entry.metadata = byteArray(e.at("metadata"));
                    manifest.entries.push_back(std::move(entry));
                }
                if (!owner->bindInitialResourceManifest(manifest, decimal(spec.at("binding"))))
                    return error("RESOURCE_MANIFEST_REJECTED");
            }
            for (unsigned source = 1; source <= 3; ++source)
                if (!owner->registerSource(source))
                    return error("INPUT_SOURCE_FAILED");
            runtime::ConnectorRoutingRequest routing;
            routing.routing = semantic::ConnectorRouting::kOrthogonal;
            if (facade().setConnectorRouting(routing).state !=
                runtime::ConnectorCommandState::kApplied)
                return error("ROUTING_FAILED");
            if (!owner->setLocalOperationCallback(localOperation, this))
                return error("CALLBACK_FAILED");
            return {{"ok", true}, {"apiVersion", 1}};
        }
        if (!owner)
            return error("CLOSED");
        if (command == "close") {
            if ((!contacts.empty() || adapter->activeContacts() != 0) ||
                facade().readTextEditor().phase != runtime::TextPhase::kClosed)
                return error("BUSY");
            if (adapter && !adapter->shutdown())
                return error("SHUTDOWN_FAILED");
            if (!owner->close())
                return error("CLOSE_FAILED");
            adapter->release();
            owner.reset();
            textInstance.lifecycle.owner = nullptr;
            return {{"ok", true}};
        }
        if (command == "resize") {
            if (!contacts.empty() || adapter->activeContacts() != 0)
                return error("BUSY");
            const auto w = j.at("width").get<float>(), h = j.at("height").get<float>(),
                       d = j.at("dpr").get<float>();
            if (!std::isfinite(w) || !std::isfinite(h) || !std::isfinite(d) || w <= 0 || h <= 0 ||
                d <= 0 || w * d > 16384 || h * d > 16384)
                return error("INVALID_SIZE");
            const auto pw = static_cast<std::uint32_t>(std::round(w * d)),
                       ph = static_cast<std::uint32_t>(std::round(h * d));
            if (!pw || !ph || !adapter->resize(w, h, d))
                return error("SURFACE_REJECTED");
            lost = false;
            return {{"ok", true}};
        }
        if (command == "lose") {
            if (lost)
                return {{"ok", true}};
            for (const auto& [id, key] : contacts) {
                ++sequence;
                const auto time = std::max(contactTimes.at(id) + 1, adapter->nowNs());
                if (!owner->cancelContact(key, sequence, time))
                    return error("CANCEL_REJECTED");
            }
            contacts.clear();
            contactTimes.clear();
            lost = true;
            return {{"ok", adapter->lose()}, {"code", "SURFACE_REJECTED"}};
        }
        if (command == "frame") {
            if (lost)
                return {{"ok", true}, {"pending", false}, {"lost", true}};
            return adapter->frame(j.value("force", false));
        }
        if (command == "state")
            return {{"ok", true}, {"value", state()}};
        if (command == "text") {
            const auto action = j.at("action").get<std::string>();
            if (action == "snapshot")
                return {{"ok", true}, {"value", J::parse(axiom_web_text_snapshot(1))}};
            if (action == "fonts")
                return {{"ok", true}, {"value", J::parse(axiom_web_font_catalog(1))}};
            if (action == "fontStatus")
                return {
                    {"ok", true},
                    {"value",
                     J::parse(axiom_web_font_status(1, j.at("id").get<std::string>().c_str()))}};
            if (action == "intent") {
                const auto value = j.at("intent");
                const auto queued = axiom_web_text_intent_json(1, value.dump().c_str());
                if (queued != static_cast<int>(runtime::TextStatus::kQueued))
                    return {{"ok", false}, {"code", "TEXT_REJECTED"}, {"status", queued}};
                if (!owner->ownerPump())
                    return error("PUMP_FAILED");
                const auto done = J::parse(axiom_web_text_intent_receipt(
                    1,
                    value.at("clientId").get<std::string>().c_str(),
                    value.at("requestId").get<std::string>().c_str()));
                if (done.is_null())
                    return error("TEXT_RECEIPT_MISSING");
                const auto s = done.at("status").get<int>();
                return {{"ok",
                         s == static_cast<int>(runtime::TextStatus::kApplied) ||
                             s == static_cast<int>(runtime::TextStatus::kObserved)},
                        {"code", "TEXT_REJECTED"},
                        {"receipt", done}};
            }
            return error("UNSUPPORTED");
        }
        if (command == "textEdit") {
            auto& f = facade();
            const auto s = f.readTextEditor();
            if (s.composition)
                return error("BUSY");
            runtime::TextInputEvent e;
            e.token = s.token;
            e.sequence = s.nextInputSequence;
            if (j.contains("token"))
                e.token = parseToken(j.at("token").dump().c_str());
            const auto kind = unsignedValue(j.at("kind"), 6);
            if (kind != 4 && kind != 5)
                return error("UNSUPPORTED");
            e.kind = static_cast<runtime::TextInputKind>(kind);
            const auto& range = j.at("range");
            if (!range.is_array() || range.size() != 2)
                throw std::invalid_argument("text range");
            e.range = {unsignedValue(range[0], 65536), unsignedValue(range[1], 65536)};
            if (j.contains("units")) {
                const auto units = j.at("units").get<std::vector<std::uint16_t>>();
                if (units.size() > 65536)
                    return error("TEXT_LIMIT");
                e.text.assign(units.begin(), units.end());
            }
            if (f.submitTextInput(e).status != runtime::TextStatus::kQueued || !owner->ownerPump())
                return error("TEXT_REJECTED");
            const auto done = f.textInputReceipt(e.token, e.sequence);
            if (!done)
                return error("TEXT_RECEIPT_MISSING");
            const auto ok = done->status == runtime::TextStatus::kApplied ||
                            done->status == runtime::TextStatus::kObserved;
            if (ok)
                (void)f.acknowledgeTextInput(e.token, e.sequence);
            return {{"ok", ok}, {"code", "TEXT_REJECTED"}, {"status", done->status}};
        }
        if (command == "selection") {
            const auto s = facade().readSelectionSummary();
            J actions = J::array();
            for (const auto& a : s.actions)
                actions.push_back(
                    {{"available", a.available()}, {"state", a.state}, {"reason", a.reason}});
            J bounds = nullptr;
            if (s.bounds.world) {
                const auto& b = *s.bounds.world;
                bounds = {b.left, b.top, b.right, b.bottom};
            }
            const auto c = facade().readCanvasControlSnapshot(target());
            J appearance = {{"fill",
                             {c.selectedAppearance.fill.r,
                              c.selectedAppearance.fill.g,
                              c.selectedAppearance.fill.b,
                              c.selectedAppearance.fill.a}},
                            {"stroke",
                             {c.selectedAppearance.stroke.r,
                              c.selectedAppearance.stroke.g,
                              c.selectedAppearance.stroke.b,
                              c.selectedAppearance.stroke.a}},
                            {"strokeWidth", c.selectedAppearance.strokeWidth},
                            {"opacity", c.selectedAppearance.opacity},
                            {"fillEnabled", c.selectedAppearance.fillEnabled},
                            {"strokeEnabled", c.selectedAppearance.strokeEnabled}};
            return {{"ok", true},
                    {"value",
                     {{"count", s.count},
                      {"roots", ids(s.roots)},
                      {"primary", hexId(s.primary)},
                      {"status", s.status},
                      {"reason", s.reason},
                      {"groupScope", s.groupScope ? J(hexId(*s.groupScope)) : J(nullptr)},
                      {"bounds", bounds},
                      {"actions", actions},
                      {"generation", std::to_string(s.semanticGeneration)},
                      {"selectionRevision", std::to_string(s.selectionRevision)},
                      {"kind", c.selectedKind},
                      {"width", c.selectedWidth},
                      {"height", c.selectedHeight},
                      {"transform", s.count == 1 ? J(c.selectedTransform) : J(nullptr)},
                      {"appearance", s.count == 1 ? appearance : J(nullptr)}}}};
        }
        if (command == "presets") {
            J entries = J::array();
            for (const auto& p : facade().brushPresets())
                entries.push_back({{"profileId", p.profileId}, {"revision", p.revision}});
            return {{"ok", true}, {"value", entries}};
        }
        if (command == "readSection") {
            const auto s = facade().readSection(j.contains("objectId") ? rawId(j.at("objectId"))
                                                                       : foundation::ObjectId{});
            return {{"ok", true},
                    {"value",
                     {{"available", s.available},
                      {"objectId", hexId(s.objectId)},
                      {"children", ids(s.directChildren)},
                      {"roots", ids(s.selectedRoots)},
                      {"groupScope", s.groupScope ? J(hexId(*s.groupScope)) : J(nullptr)},
                      {"title", s.content.title},
                      {"boundary",
                       {s.content.boundary.x,
                        s.content.boundary.y,
                        s.content.boundary.width,
                        s.content.boundary.height}},
                      {"generation", std::to_string(s.semanticGeneration)}}}};
        }
        if (command == "connector") {
            const auto action = j.at("action").get<std::string>();
            auto& f = facade();
            if (action == "read") {
                const auto s = j.contains("objectId")
                                   ? f.readConnectorState(rawId(j.at("objectId")))
                                   : f.readSelectedConnector();
                J points = J::array();
                for (auto p : s.route)
                    points.push_back({p.x, p.y});
                return {{"ok", true},
                        {"value",
                         {{"available", s.available},
                          {"objectId", hexId(s.objectId)},
                          {"start", endpointJson(s.content.start)},
                          {"end", endpointJson(s.content.end)},
                          {"routing", s.content.routing},
                          {"routeStatus", s.routeStatus},
                          {"route", points},
                          {"generation", std::to_string(s.semanticGeneration)}}}};
            }
            runtime::ConnectorCommandReceipt result;
            if (action == "routing") {
                runtime::ConnectorRoutingRequest r;
                r.routing =
                    static_cast<semantic::ConnectorRouting>(unsignedValue(j.at("routing"), 2));
                if (j.contains("objectId"))
                    r.objectId = rawId(j.at("objectId"));
                if (j.contains("generation"))
                    r.expectedSemanticGeneration = decimal(j.at("generation"));
                result = f.setConnectorRouting(r);
            } else if (action == "create") {
                runtime::ConnectorCreateRequest r;
                r.objectId = rawId(j.at("objectId"));
                r.content.start = endpoint(j.at("start"));
                r.content.end = endpoint(j.at("end"));
                r.content.routing = static_cast<semantic::ConnectorRouting>(
                    unsignedValue(j.value("routing", J(2)), 2));
                result = f.createConnector(r);
            } else
                return error("UNSUPPORTED");
            return {{"ok", result.state == runtime::ConnectorCommandState::kApplied},
                    {"code", "CONNECTOR_REJECTED"},
                    {"reason", result.reason},
                    {"objectId", hexId(result.objectId)}};
        }
        if (command == "sticky") {
            if (j.at("action") == "read") {
                const auto s = facade().readSelectedSticky();
                return {{"ok", true},
                        {"value",
                         {{"available", s.available},
                          {"objectId", hexId(s.objectId)},
                          {"width", s.content.width},
                          {"height", s.content.height}}}};
            }
            runtime::StickyCreateRequest r;
            r.objectId = rawId(j.at("objectId"));
            r.content = {j.at("size"), j.at("size")};
            r.transform.tx = j.value("x", 0.0);
            r.transform.ty = j.value("y", 0.0);
            const auto result = facade().createSticky(r);
            return {{"ok", result.state == runtime::ConnectorCommandState::kApplied},
                    {"code", "STICKY_REJECTED"},
                    {"objectId", hexId(result.objectId)}};
        }
        if (command == "multi") {
            auto& f = facade();
            const auto action = j.at("action").get<std::string>();
            if (action == "permission")
                return {{"ok",
                         f.setMultiSelectionPermission(
                             static_cast<runtime::MultiSelectionPermissionMode>(
                                 unsignedValue(j.at("mode"), 2)))},
                        {"code", "PERMISSION_REJECTED"}};
            const auto projection = f.readMultiSelection();
            if (action == "read") {
                J properties = J::object();
                for (const auto& [field, p] : projection.properties) {
                    J excluded = J::array();
                    for (const auto& e : p.excluded)
                        excluded.push_back({{"objectId", hexId(e.object)}, {"reason", e.reason}});
                    properties[std::to_string(field)] = {
                        {"total", p.total},
                        {"eligible", ids(p.eligible)},
                        {"excluded", excluded},
                        {"mixed", p.mixed},
                        {"partial", p.partial},
                        {"available", p.editAvailable},
                        {"availability", p.availability},
                        {"strokeWidthAvailable", p.strokeWidthAvailable},
                        {"uniformValue",
                         p.uniformValue ? propertyJson(*p.uniformValue) : J(nullptr)}};
                }
                return {{"ok", true},
                        {"value",
                         {{"available", projection.available},
                          {"stamp", stampJson(projection.stamp)},
                          {"roots", ids(projection.roots)},
                          {"properties", properties}}}};
            }
            const auto stamp = parseStamp(j.at("stamp"));
            const auto eligible = parseIds(j.at("eligible"));
            if (action == "values") {
                const auto values = f.readMultiSelectionPropertyValues(
                    unsignedValue(j.at("field"), 0xFFFFFFFFU), stamp, eligible);
                if (!values)
                    return error("STALE_OR_UNAVAILABLE");
                J out = J::array();
                for (const auto& v : *values)
                    out.push_back(
                        {{"objectId", hexId(v.object)}, {"value", propertyJson(v.value)}});
                return {{"ok", true}, {"value", out}};
            }
            const auto id = ++sequence;
            const auto queued =
                action == "width"
                    ? f.submitMultiSelectionStrokeWidth(j.at("width"), stamp, eligible, 0x4A53, id)
                    : f.submitMultiSelectionProperty(unsignedValue(j.at("field"), 0xFFFFFFFFU),
                                                     parseProperty(j.at("value")),
                                                     stamp,
                                                     eligible,
                                                     0x4A53,
                                                     id);
            if (!queued.queued)
                return {{"ok", false},
                        {"code", "MULTI_ADMISSION_REJECTED"},
                        {"state", queued.state},
                        {"error", queued.error}};
            if (!owner->ownerPump())
                return error("PUMP_REJECTED");
            const auto result = f.multiSelectionReceipt(0x4A53, id);
            if (!result)
                return error("RECEIPT_MISSING");
            return {{"ok", result->applied},
                    {"code", "MULTI_REJECTED"},
                    {"state", result->state},
                    {"error", result->error},
                    {"changed", ids(result->changed)},
                    {"unchanged", ids(result->unchanged)}};
        }
        if (command == "resource") {
            const auto action = j.at("action").get<std::string>();
            if (action == "token") {
                const auto t = owner->resourceToken(rawId(j.at("objectId")));
                return {{"ok", true}, {"value", t ? tokenJson(*t) : J(nullptr)}};
            }
            if (action == "publish") {
                auto token = parseResourceToken(j.at("token"));
                if (!owner->publishResourceBytes(token, bytes) || !owner->ownerPump())
                    return error("RESOURCE_REJECTED");
                const auto r = owner->resourceReceipt(token.resourceId);
                if (!r || r->token != token)
                    return error("RESOURCE_RECEIPT_MISSING");
                return {{"ok",
                         r->state == runtime::ResourceBindingState::kBytesValidated ||
                             r->state == runtime::ResourceBindingState::kDecodeReady ||
                             r->state == runtime::ResourceBindingState::kLayoutReady},
                        {"code", "RESOURCE_REJECTED"},
                        {"state", r->state}};
            }
            if (action == "receipt") {
                const auto r = owner->resourceReceipt(rawId(j.at("objectId")));
                return {{"ok", true},
                        {"value",
                         r ? J{{"token", tokenJson(r->token)},
                               {"state", r->state},
                               {"byteLength", r->byteLength},
                               {"changed", r->changed}}
                           : J(nullptr)}};
            }
            return error("UNSUPPORTED");
        }
        if (command == "pointer") {
            input::PlatformPointerSample p;
            p.source = j.at("source");
            p.pointer = j.at("pointer");
            p.sequence = ++sequence;
            p.timestampNs = j.at("timestampNs");
            p.x = j.at("x");
            p.y = j.at("y");
            const auto phase = unsignedValue(j.at("phase"), 3);
            if (p.source < 1 || p.source > 3 || phase > 3)
                return error("INVALID_INPUT");
            p.phase = phase == 0   ? input::PointerPhase::kDown
                      : phase == 1 ? input::PointerPhase::kMove
                      : phase == 2 ? input::PointerPhase::kUp
                                   : input::PointerPhase::kCancel;
            p.pressure = j.value("pressure", 0.0F);
            p.capabilities.pressure = j.value("pressureAvailable", false);
            p.capabilities.tool = p.source == 3   ? input::PointerTool::kFinger
                                  : p.source == 2 ? input::PointerTool::kPen
                                                  : input::PointerTool::kUnknown;
            p.contact = {j.value("width", 0.0F),
                         j.value("height", 0.0F),
                         0,
                         j.value("geometryAvailable", false)};
            p.capabilities.contactGeometry = p.contact.available;
            p.capabilities.coalescedHistory = j.value("historical", false);
            p.provenance = p.capabilities.coalescedHistory
                               ? input::SampleProvenance::kCoalescedHistory
                               : input::SampleProvenance::kConfirmedCurrent;
            p.modifiers = j.value("shift", false) ? input::PointerModifiers::kShift
                                                  : input::PointerModifiers::kNone;
            input::PlatformPointerBatch batch;
            batch.samples = {p};
            const auto id = std::pair{p.source, p.pointer};
            bool ok = false;
            if (phase == 0) {
                if (contacts.contains(id))
                    return error("DUPLICATE_CONTACT");
                if (auto key = owner->beginContact(batch)) {
                    contacts[id] = *key;
                    contactTimes[id] = p.timestampNs;
                    ok = true;
                }
            } else if (auto it = contacts.find(id); it != contacts.end()) {
                ok = owner->submitContact(it->second, batch);
                if (ok)
                    contactTimes[id] = p.timestampNs;
                if (phase >= 2 && ok) {
                    contacts.erase(it);
                    contactTimes.erase(id);
                }
            }
            return {{"ok", ok}, {"code", "INPUT_REJECTED"}};
        }
        if (command == "hover")
            return {{"ok", true},
                    {"changed", owner->hoverSelectionAtViewPoint(j.at("x"), j.at("y"))}};
        if (command == "control") {
            auto r = request();
            auto& p = r.payload;
            const auto kind = j.at("kind").get<std::string>();
            if (j.contains("expectedGeneration")) {
                const auto s = facade().readCanvasControlSnapshot(r.target);
                if (s.semanticGeneration != decimal(j.at("expectedGeneration")) ||
                    s.selectionRevision != decimal(j.at("expectedSelectionRevision")))
                    return error("STALE_TARGET");
            }
            if (kind == "tool") {
                p.kind = K::kSelectTool;
                p.tool = static_cast<runtime::CanvasToolKind>(unsignedValue(j.at("value"), 4));
            } else if (kind == "preset") {
                p.kind = K::kSelectPreset;
                p.preset = {j.at("profileId"), unsignedValue(j.at("revision"), 0xffffffffU)};
            } else if (kind == "shape") {
                p.kind = K::kSelectCreationPreset;
                p.creationPreset =
                    static_cast<runtime::CanvasCreationPreset>(unsignedValue(j.at("value"), 5));
            } else if (kind == "selectionMode") {
                p.kind = K::kSelectionAcquisitionMode;
                p.selectionAcquisitionMode =
                    static_cast<runtime::CanvasSelectionMode>(unsignedValue(j.at("value"), 2));
            } else if (kind == "ink") {
                p.kind = K::kInkOptions;
                if (j.contains("size"))
                    p.ink.size = runtime::OptionPatch<float>::set(j.at("size"));
                if (j.contains("opacity"))
                    p.ink.opacity = runtime::OptionPatch<float>::set(j.at("opacity"));
                if (j.contains("color")) {
                    auto c = j.at("color").get<std::array<float, 4>>();
                    p.ink.color =
                        runtime::OptionPatch<runtime::RgbaColor>::set({c[0], c[1], c[2], c[3]});
                }
            } else if (kind == "eraser") {
                p.kind = K::kEraserOptions;
                p.eraser.mode =
                    runtime::OptionPatch<std::uint32_t>::set(unsignedValue(j.at("mode"), 2));
                if (j.contains("diameter"))
                    p.eraser.diameterLogicalPx = runtime::OptionPatch<float>::set(j.at("diameter"));
            } else if (kind == "undo")
                p.kind = K::kUndo;
            else if (kind == "redo")
                p.kind = K::kRedo;
            else if (kind == "pan") {
                p.kind = K::kPanBy;
                p.x = j.at("x");
                p.y = j.at("y");
            } else if (kind == "zoom") {
                p.kind = K::kZoomAt;
                p.x = j.at("x");
                p.y = j.at("y");
                p.hasAnchor = true;
                p.scale = j.at("scale");
            } else if (kind == "fit") {
                p.kind = K::kFit;
                p.fitTarget = static_cast<runtime::CanvasFitTarget>(
                    unsignedValue(j.value("target", J(0)), 2));
                if (j.contains("objectId"))
                    p.objectId = rawId(j.at("objectId"));
            } else if (kind == "size") {
                p.kind = K::kSelectedSize;
                p.width = j.at("width");
                p.height = j.at("height");
            } else if (kind == "transform") {
                p.kind = K::kSelectedTransform;
                p.transform = j.at("matrix").get<std::array<double, 6>>();
            } else if (kind == "delete")
                p.kind = K::kDeleteSelected;
            else if (kind == "lock") {
                p.kind = K::kSelectedLock;
                p.flag = j.at("value");
            } else if (kind == "visible") {
                p.kind = K::kSelectedVisible;
                p.flag = j.at("value");
            } else if (kind == "unlock")
                p.kind = K::kUnlockSelected;
            else if (kind == "order") {
                p.kind = K::kSelectedOrder;
                p.flag = j.at("front");
            } else if (kind == "appearance") {
                const bool selected = j.value("selected", true);
                p.kind = selected ? K::kSelectedAppearance : K::kFutureAppearance;
                const auto s = facade().readCanvasControlSnapshot(r.target);
                p.appearance = selected ? s.selectedAppearance : s.futureAppearance;
                if (j.contains("fill")) {
                    auto c = j.at("fill").get<std::array<float, 4>>();
                    p.appearance.fill = {c[0], c[1], c[2], c[3]};
                }
                if (j.contains("stroke")) {
                    auto c = j.at("stroke").get<std::array<float, 4>>();
                    p.appearance.stroke = {c[0], c[1], c[2], c[3]};
                }
                if (j.contains("fillEnabled"))
                    p.appearance.fillEnabled = j.at("fillEnabled");
                if (j.contains("strokeEnabled"))
                    p.appearance.strokeEnabled = j.at("strokeEnabled");
                if (j.contains("strokeWidth"))
                    p.appearance.strokeWidth = j.at("strokeWidth");
                if (j.contains("opacity"))
                    p.appearance.opacity = j.at("opacity");
            } else
                return error("UNSUPPORTED");
            return receipt(r);
        }
        if (command == "section") {
            auto r = request();
            r.payload.kind = K::kSection;
            auto& p = r.payload.section;
            const auto action = unsignedValue(j.at("action"), 11);
            if (action > 11)
                return error("INVALID_VALUE");
            p.action = static_cast<runtime::SectionAction>(action);
            if (j.contains("objectId"))
                p.objectId = rawId(j.at("objectId"));
            if (j.contains("objects"))
                for (const auto& id : j.at("objects"))
                    p.objects.push_back(rawId(id));
            if (j.contains("title"))
                p.content.title = j.at("title");
            if (j.contains("boundary")) {
                auto b = j.at("boundary").get<std::array<double, 4>>();
                p.content.boundary = {b[0], b[1], b[2], b[3]};
            }
            if (j.contains("parentId") && !j.at("parentId").is_null())
                p.parentId = rawId(j.at("parentId"));
            if (j.contains("generation"))
                p.expectedSemanticGeneration = decimal(j.at("generation"));
            if (j.contains("selectionRevision"))
                p.selectionRevision = decimal(j.at("selectionRevision"));
            return receipt(r);
        }
        if (command == "image") {
            runtime::ImageCommandReceipt result;
            const auto action = j.at("action").get<std::string>();
            auto& f = facade();
            if (action == "read") {
                const auto s = f.readSelectedImage();
                return {{"ok", true},
                        {"value",
                         {{"available", s.available},
                          {"editable", s.editable},
                          {"objectId", hexId(s.objectId)},
                          {"content", imageContent(s.content)}}}};
            }
            if (action == "readCrop") {
                const auto s = f.readImageCrop();
                return {{"ok", true},
                        {"value",
                         {{"active", s.active},
                          {"token", std::to_string(s.cropToken)},
                          {"objectId", hexId(s.objectId)},
                          {"scale", s.contentScale},
                          {"content", imageContent(s.content)}}}};
            }
            if (action == "import") {
                runtime::ImageImportRequest r;
                r.bytes = bytes;
                r.mimeType = j.at("mime");
                result = f.importImage(r);
            } else if (action == "replace") {
                const auto s = f.readSelectedImage();
                runtime::ImageReplaceRequest r;
                r.objectId = s.objectId;
                r.content = s.content;
                r.bytes = bytes;
                r.mimeType = j.at("mime");
                result = f.replaceImage(r);
            } else if (action == "resize") {
                result = f.resizeImage({rawId(j.at("objectId")),
                                        j.at("width"),
                                        j.at("height"),
                                        j.value("freeStretch", false)
                                            ? runtime::ImageResizePolicy::kFreeStretch
                                            : runtime::ImageResizePolicy::kPreserveAspect});
            } else if (action == "crop") {
                result = f.beginImageCrop({rawId(j.at("objectId"))});
            } else if (action == "done")
                result = f.commitImageCrop(decimal(j.at("token")));
            else if (action == "cancel")
                result = f.cancelImageCrop(decimal(j.at("token")));
            else if (action == "update") {
                runtime::ImageCropUpdateRequest r;
                r.cropToken = decimal(j.at("token"));
                auto b = j.at("sourceRect").get<std::array<double, 4>>();
                r.sourceRect = {b[0], b[1], b[2], b[3]};
                r.width = j.at("width");
                r.height = j.at("height");
                if (j.contains("scale"))
                    r.contentScale = j.at("scale");
                result = f.updateImageCrop(r);
            } else
                return error("UNSUPPORTED");
            return {{"ok", result.state == runtime::ImageCommandState::kApplied},
                    {"code", "IMAGE_REJECTED"},
                    {"reason", result.reason},
                    {"objectId", hexId(result.objectId)},
                    {"cropToken", std::to_string(result.cropToken)}};
        }
        if (command == "snapshot") {
            runtime::DocumentSnapshotRequest r;
            r.expectedSemanticGeneration = decimal(j.at("generation"));
            r.documentRevision = decimal(j.at("documentRevision"));
            r.recoveryFrontier = byteArray(j.at("recoveryFrontier"));
            auto saved = owner->captureDocumentSnapshot(r);
            outputBytes = std::move(saved.bytes);
            J result = {{"ok", saved.status == runtime::DocumentSnapshotStatus::kCaptured},
                        {"code", "SNAPSHOT_REJECTED"},
                        {"status", saved.status},
                        {"generation", std::to_string(saved.semanticGeneration)}};
            if (saved.status == runtime::DocumentSnapshotStatus::kCaptured) {
                auto decoded = semantic::SnapshotCodec::decode(
                    outputBytes,
                    semantic::SnapshotCapability::
                        kShape2TriangleRichTextFormattingConnectorAndSectionV1);
                if (!decoded.ok())
                    return error("SNAPSHOT_DECODE_FAILED");
                result["checkpointDigest"] = decoded.snapshot->checkpoint_digest;
            }
            return result;
        }
        if (command == "remote") {
            const auto op = semantic::SemanticCodec::decodeProtobufOperation(bytes);
            if (!op.ok())
                return error("INVALID_OPERATION");
            auto result = owner->applyRemoteOperation(op.operation);
            return {{"ok",
                     result.disposition == semantic::ApplyDisposition::kApplied ||
                         result.disposition == semantic::ApplyDisposition::kAlreadyApplied},
                    {"code", "REMOTE_REJECTED"},
                    {"disposition", result.disposition}};
        }
        return error("UNSUPPORTED");
    }
};
Session::Session(std::unique_ptr<SessionPlatform> p, SessionCallbacks c)
    : impl_(std::make_unique<Impl>()) {
    impl_->adapter = std::move(p);
    impl_->callbacks = c;
}
Session::~Session() {
    if (activeText == &impl_->textInstance)
        activeText = nullptr;
}
void Session::activateTextAccess() noexcept {
    activeText = &impl_->textInstance;
}
void Session::clearCallbacks() noexcept {
    impl_->callbacks = {};
}
runtime::RuntimeComposition* Session::composition() noexcept {
    return impl_->owner.get();
}
SessionPlatform& Session::platform() noexcept {
    return *impl_->adapter;
}
std::span<const std::uint8_t> Session::bytes() const noexcept {
    return impl_->outputBytes;
}
std::string Session::call(std::string_view input, std::span<const std::uint8_t> data) {
    auto* previous = activeText;
    activeText = &impl_->textInstance;
    struct Restore {
        TextInstance* previous;
        ~Restore() {
            activeText = previous;
        }
    } restore{previous};
    try {
        if (std::this_thread::get_id() != impl_->thread)
            return Impl::error("WRONG_THREAD").dump();
        if (input.empty() || input.size() > 1048576 || data.size() > 134217728)
            throw std::invalid_argument("request length");
        auto j = Json::parse(input);
        validateTransportEnums(j);
        std::vector<std::uint8_t> bytes(data.begin(), data.end());
        impl_->outputBytes.clear();
        return impl_->dispatch(j.at("command"), j, bytes).dump();
    } catch (const std::exception& e) {
        return Json{{"ok", false}, {"code", "INVALID_REQUEST"}, {"message", e.what()}}.dump();
    } catch (...) {
        return "{\"ok\":false,\"code\":\"INTERNAL_FAILURE\"}";
    }
}
} // namespace canvas::sdk

#include "canvas/input/platform_input_contract.hpp"
#include "canvas/render/webgl_surface_backend.hpp"
#include "web_surface_host.hpp"

#include <GLES3/gl3.h>
#include "host_font_resources.hpp"
#include "nlohmann/json.hpp"
#include <cstring>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#if AXIOM_WEB_DEBUG_UI
#include "web_debug_ui.inl"
#endif

EM_JS(void, axiom_web_preview_visibility, (int visible), {
    const c = document.getElementById('axiom-preview');
    if (c)
        c.style.visibility = visible ? 'visible' : 'hidden';
});

namespace {
using namespace canvas;
struct Fence final {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
    GLsync sync = nullptr;
    void clear() {
        if (sync && emscripten_webgl_make_context_current(context) == EMSCRIPTEN_RESULT_SUCCESS)
            glDeleteSync(sync);
        sync = nullptr;
    }
    bool insert(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE value) {
        context = value;
        if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS)
            return false;
        sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        return sync != nullptr;
    }
    int poll() const {
        if (!sync)
            return 1;
        if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS)
            return 0;
        const auto r = glClientWaitSync(sync, 0, 0);
        return r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED ? 1
               : r == GL_TIMEOUT_EXPIRED                               ? 2
                                                                       : 0;
    }
};
struct WebInstance final {
    web_demo::WebSurfaceHost lifecycle;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE canonicalContext = 0, previewContext = 0;
    Fence canonicalFence, previewFence;
    std::optional<runtime::RuntimeFrameOpportunity> pending;
    std::unordered_map<std::uint64_t, input::PointerKey> pointers;
    std::unordered_map<std::uint64_t, std::uint64_t> timestamps;
    input::PointerKey lastPointer{};
    std::uint64_t pointerSamples = 0, sequence = 0, request = 0, presentedFrames = 0, retries = 0,
                  rejectedInputs = 0;
    std::uint64_t lastTextIntent=0;
    bool forceCanonical = true;
    runtime::CanvasControlReceiptState lastControl =
        runtime::CanvasControlReceiptState::kUnsupported;
    std::string error;
#if AXIOM_WEB_DEBUG_UI
    std::unique_ptr<WebDebugUi> debug;
#endif
    explicit WebInstance(const render::SurfaceMetrics& m)
        : lifecycle(
              {render::ViewId{301}, render::SurfaceGeneration{1}, render::MetricsGeneration{1}, m},
              true) {}
    void discardFrame() {
        canonicalFence.clear();
        previewFence.clear();
        pending.reset();
    }
    void cancel() {
        // Retire the exact ingress generations before forgetting host capture.
        // Clearing only this map strands an active Runtime generation after
        // blur/tool cancellation, so a later Down cannot reuse the pointer.
        const auto now=static_cast<std::uint64_t>(std::max(1.0,emscripten_get_now()*1000000.0));
        for (const auto& [id,key]:pointers) {
            (void)id;
            (void)lifecycle.composition().cancelPlatformPointer(key,++sequence,now);
        }
        (void)lifecycle.composition().cancelAllPointers();
        pointers.clear();
    }
    ~WebInstance() {
        discardFrame();
        cancel();
        (void)lifecycle.composition().close();
    }
};
WebInstance* instance(std::uintptr_t handle) {
    return reinterpret_cast<WebInstance*>(handle);
}
std::uint64_t identity(std::uint32_t source, std::uint32_t pointer) {
    return (static_cast<std::uint64_t>(source) << 32U) | pointer;
}
bool bind(WebInstance& v, const char* selector, const render::SurfaceMetrics& m) {
    if (!selector || !std::isfinite(m.logicalWidth) || !std::isfinite(m.logicalHeight) ||
        !std::isfinite(m.devicePixelRatio) || m.logicalWidth <= 0 || m.logicalHeight <= 0 ||
        m.devicePixelRatio <= 0 || !m.physicalWidth || !m.physicalHeight)
        return false;
    auto& comp = v.lifecycle.composition();
    v.cancel();
    v.discardFrame();
    const auto before = comp.surfaceSnapshot();
    if (before.available && before.width == m.physicalWidth && before.height == m.physicalHeight) {
        if (v.lifecycle.bindMetrics(m) != web_demo::WebSurfaceDisposition::kRebound)
            return false;
    } else {
        if (comp.state() == runtime::RuntimeCompositionState::kReady &&
            v.lifecycle.markLost() != render::SurfaceLifecycleDisposition::kLost)
            return false;
        // One provider, not a legacy backend plus another Ganesh wrapper of the same context.
        auto canonical = std::make_unique<render::WebGlSurfaceProvider>(
            render::WebGlSurfaceConfig{selector, m.physicalWidth, m.physicalHeight});
        if (!canonical->ready()) {
            v.error = canonical->error();
            return false;
        }
        v.canonicalContext = emscripten_webgl_get_current_context();
        auto preview = std::make_unique<render::WebGlSurfaceProvider>(render::WebGlSurfaceConfig{
            "#axiom-preview", m.physicalWidth, m.physicalHeight, 0, [](bool visible) {
                axiom_web_preview_visibility(visible);
            }});
        if (!preview->ready()) {
            v.error = preview->error();
            return false;
        }
        v.previewContext = emscripten_webgl_get_current_context();
        if (!comp.registerSurfaceProvider("webgl-window", std::move(canonical)) ||
            !comp.registerPreviewSurfaceProvider("webgl-preview", std::move(preview)) ||
            v.lifecycle.bindMetrics(m) != web_demo::WebSurfaceDisposition::kRebound)
            return false;
    }
    (void)comp.setPlatformPresentationDeferred(true);
    v.error.clear();
    v.forceCanonical = true;
    return true;
}
} // namespace

extern "C" {
EMSCRIPTEN_KEEPALIVE std::uintptr_t axiom_web_create(
    const char* selector, float lw, float lh, std::uint32_t pw, std::uint32_t ph, float dpr) {
    auto v = std::make_unique<WebInstance>(render::SurfaceMetrics{lw, lh, pw, ph, dpr, 1});
    if (!bind(*v, selector, {lw, lh, pw, ph, dpr, 1}))
        return 0;
    if(!minimal_runtime_host::bindHostTextFonts(v->lifecycle.composition(),"/axiom-font.otf","/fonts"))return 0;
    for (std::uint32_t s = 1; s <= 3; ++s)
        if (!v->lifecycle.composition().registerSource(s))
            return 0;
#if AXIOM_WEB_DEBUG_UI
    v->debug = std::make_unique<WebDebugUi>(v->lifecycle.composition());
    if (!v->debug->initialize())
        return 0;
#endif
    return reinterpret_cast<std::uintptr_t>(v.release());
}
EMSCRIPTEN_KEEPALIVE void axiom_web_destroy(std::uintptr_t handle) {
    delete instance(handle);
}
EMSCRIPTEN_KEEPALIVE int axiom_web_resize(std::uintptr_t h,
                                          const char* s,
                                          float lw,
                                          float lh,
                                          std::uint32_t pw,
                                          std::uint32_t ph,
                                          float dpr) {
    auto* v = instance(h);
    return v && bind(*v, s, {lw, lh, pw, ph, dpr, 1});
}
EMSCRIPTEN_KEEPALIVE int axiom_web_backend_ready(std::uintptr_t h) {
    auto* v = instance(h);
    return v && v->lifecycle.composition().canonicalProviderAvailable() &&
           v->lifecycle.composition().state() == runtime::RuntimeCompositionState::kReady;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_lose_surface(std::uintptr_t h) {
    auto* v = instance(h);
    if (!v)
        return 0;
    v->cancel();
    v->discardFrame();
    return v->lifecycle.markLost() == render::SurfaceLifecycleDisposition::kLost;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_cancel_all(std::uintptr_t h) {
    auto* v = instance(h);
    if (!v)
        return 0;
    v->cancel();
    return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_hover(std::uintptr_t h, float x, float y) {
    auto* v = instance(h);
    if (!v || !std::isfinite(x) || !std::isfinite(y)) return 0;
    return v->lifecycle.composition().hoverSelectionAtViewPoint(x, y) ? 1 : 0;
}
EMSCRIPTEN_KEEPALIVE int
axiom_web_cancel_pointer(std::uintptr_t h, std::uint32_t s, std::uint32_t p, std::uint64_t g) {
    auto* v = instance(h);
    if (!v)
        return 0;
    const auto id = identity(s, p);
    const auto it = v->pointers.find(id);
    if (it == v->pointers.end() || it->second.generation != g)
        return 0;
    const auto now = static_cast<std::uint64_t>(emscripten_get_now() * 1000000.0);
    const bool ok = v->lifecycle.composition().cancelPlatformPointer(
        it->second, ++v->sequence, std::max(now, v->timestamps[id] + 1));
    if (ok)
        v->pointers.erase(it);
    return ok;
}
EMSCRIPTEN_KEEPALIVE std::uint64_t
axiom_web_pointer_generation(std::uintptr_t h, std::uint32_t s, std::uint32_t p) {
    auto* v = instance(h);
    if (!v)
        return 0;
    const auto it = v->pointers.find(identity(s, p));
    return it == v->pointers.end() ? 0 : it->second.generation;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_pointer(std::uintptr_t h,
                                           std::uint32_t s,
                                           std::uint32_t p,
                                           std::uint64_t g,
                                           std::uint64_t time,
                                           int phase,
                                           float x,
                                           float y,
                                           float pressure,
                                           int pressureAvailable,
                                           int tool,
                                           float width,
                                           float height,
                                           int geometryAvailable,
                                           int history, int modifiers=0) {
    auto* v = instance(h);
    if (!v)
        return 0;
    const auto reject = [&]() {
        ++v->rejectedInputs;
        return 0;
    };
    if (!s || s > 3 || !p || !time || phase < 0 || phase > 3 || !std::isfinite(x) ||
        !std::isfinite(y) || !std::isfinite(pressure) || pressure < 0 || pressure > 1)
        return reject();
    const auto id = identity(s, p);
    auto& comp = v->lifecycle.composition();
    const auto active = v->pointers.find(id);
    if (time <= v->timestamps[id] ||
        (phase == 0 ? g != 0 || active != v->pointers.end()
                    : active == v->pointers.end() || active->second.generation != g))
        return reject();
    input::PointerCapabilities caps{};
    caps.tool = tool == 1   ? input::PointerTool::kFinger
                : tool == 2 ? input::PointerTool::kPen
                            : input::PointerTool::kUnknown;
    caps.pressure = pressureAvailable != 0;
    caps.coalescedHistory = history != 0;
    caps.contactGeometry = geometryAvailable != 0 && width > 0 && height > 0;
    input::PlatformPointerBatch batch;
    batch.samples.push_back({s,
                             p,
                             ++v->sequence,
                             time,
                             x,
                             y,
                             pressure,
                             0,
                             0,
                             {width, height, 0, caps.contactGeometry},
                             caps,
                             history ? input::SampleProvenance::kCoalescedHistory
                                     : input::SampleProvenance::kConfirmedCurrent,
                             static_cast<input::PointerPhase>(phase)});
    batch.samples.back().modifiers=(modifiers&1)?input::PointerModifiers::kShift:input::PointerModifiers::kNone;
    if (!comp.submit(batch)) {
        if (const auto key = comp.platformKey(s, p))
            (void)comp.cancelPlatformPointer(*key, ++v->sequence, time);
        v->pointers.erase(id);
        return reject();
    }
    if (phase == 0) {
        const auto key = comp.platformKey(s, p);
        if (!key)
            return reject();
        v->pointers[id] = *key;
        v->lastPointer = *key;
    } else
        v->lastPointer = active->second;
    if (phase == 2 || phase == 3)
        v->pointers.erase(id);
    v->timestamps[id] = time;
    ++v->pointerSamples;
    return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_begin_frame(std::uintptr_t h) {
    auto* v = instance(h);
    if (!v || !axiom_web_backend_ready(h))
        return 0;
    if (v->pending)
        return 2;
    runtime::RuntimeFrameOpportunity work{};
    if (!v->lifecycle.composition().serviceFrameOpportunity(v->forceCanonical, work)) {
        v->error = "Runtime frame opportunity rejected";
        return 0;
    }
    v->forceCanonical = false;
    // A historical submitted identity is not newly rendered work.
    if (!work.canonicalRequested)
        work.canonicalWork.reset();
    if (!work.canonicalWork && !work.previewWork) {
        const auto& comp = v->lifecycle.composition();
        if (comp.canonicalPresentationDirty() || (comp.previewActive() && comp.previewRenderState().dirty)) {
            ++v->retries;
            return 3;
        }
        return 1;
    }
    if ((work.canonicalWork && !v->canonicalFence.insert(v->canonicalContext)) ||
        (work.previewWork && !v->previewFence.insert(v->previewContext))) {
        v->error = "WebGL completion fence creation failed";
        return 0;
    }
    v->pending = work;
    return 2;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_complete_frame(std::uintptr_t h) {
    auto* v = instance(h);
    if (!v)
        return 0;
    if (!v->pending)
        return 1;
    const auto a = v->canonicalFence.poll(), b = v->previewFence.poll();
    if (!a || !b) {
        v->error = "WebGL completion fence failed";
        return 0;
    }
    if (a == 2 || b == 2) {
        ++v->retries;
        return 2;
    }
    auto& comp = v->lifecycle.composition();
    const auto& work = *v->pending;
    if (work.previewWork && comp.previewActive() &&
        comp.previewRenderState().contentRevision == work.previewWork->contentRevision &&
        !comp.finishFrameOpportunity({work.previewWork, true})) {
        v->error = "Preview completion rejected";
        return 0;
    }
    // Called only after browser yield and actual GPU fence completion. No scanout timestamp is
    // invented.
    if (work.canonicalWork) {
        if (!comp.receivePlatformPresented({*work.canonicalWork,
                                            render::PresentOutcome::kPresented,
                                            render::PresentEvidenceKind::kPlatformQualified,
                                            std::nullopt})) {
            v->error = "Canonical completion rejected";
            return 0;
        }
        ++v->presentedFrames;
    }
    v->discardFrame();
    return 1;
}
EMSCRIPTEN_KEEPALIVE int axiom_web_control(
    std::uintptr_t h, int kind, float a, float b, float c, float d, float e, float f) {
    auto* v = instance(h);
    if (!v || kind < 0 ||
        (kind > static_cast<int>(runtime::CanvasControlPayloadKind::kUnlockSelected)&&
         kind != static_cast<int>(runtime::CanvasControlPayloadKind::kSelectionAcquisitionMode)))
        return 0;
    auto& facade = v->lifecycle.facade();
    const auto targets = facade.mountedCanvasTargets();
    if (targets.empty())
        return 0;
    runtime::CanvasControlRequest r{};
    r.clientId = 0x574542U;
    r.requestId = ++v->request;
    r.deadlineSequence = facade.controlSequence() + 32;
    r.target = targets.front();
    auto& payload = r.payload;
    payload.kind = static_cast<runtime::CanvasControlPayloadKind>(kind);
    using K = runtime::CanvasControlPayloadKind;
    switch (payload.kind) {
    case K::kSelectionAcquisitionMode:
        if (!std::isfinite(a)||a<0||a>2||a!=std::floor(a)) return 0;
        payload.selectionAcquisitionMode=static_cast<runtime::CanvasSelectionMode>(static_cast<int>(a));break;
    case K::kSelectTool:
        if (a < 0 || a > 3)
            return 0;
        payload.tool = static_cast<runtime::CanvasToolKind>(static_cast<int>(a));
        break;
    case K::kSelectPreset: {
        const auto presets = facade.brushPresets();
        if (a < 0 || static_cast<std::size_t>(a) >= presets.size())
            return 0;
        payload.preset = presets[static_cast<std::size_t>(a)];
        break;
    }
    case K::kInkOptions:
        if (a >= 0)
            payload.ink.size = runtime::OptionPatch<float>::set(a);
        if (b >= 0)
            payload.ink.opacity = runtime::OptionPatch<float>::set(b);
        if (c >= 0)
            payload.ink.color = runtime::OptionPatch<runtime::RgbaColor>::set({c, d, e, f});
        break;
    case K::kEraserOptions:
        payload.eraser.mode =
            runtime::OptionPatch<std::uint32_t>::set(static_cast<std::uint32_t>(a));
        if (b > 0)
            payload.eraser.diameterLogicalPx = runtime::OptionPatch<float>::set(b);
        break;
    case K::kSelectCreationPreset:
        if (a < 0 || a > 5)
            return 0;
        payload.creationPreset = static_cast<runtime::CanvasCreationPreset>(static_cast<int>(a));
        break;
    case K::kSelectedTransform:
        payload.transform = {a, b, c, d, e, f};
        break;
    default:
        payload.x = a;
        payload.y = b;
        payload.scale = c;
        break;
    }
    v->cancel();
    const auto receipt = facade.submitCanvasControl(r);
    v->lastControl =
        receipt.control ? receipt.control->state : runtime::CanvasControlReceiptState::kFailed;
    return receipt.state == runtime::ProductControlState::kQueued ||
           receipt.state == runtime::ProductControlState::kApplied;
}
EMSCRIPTEN_KEEPALIVE const char* axiom_web_snapshot(std::uintptr_t h) {
    static std::string json;
    auto* v = instance(h);
    if (!v)
        return "{}";
    auto& comp = v->lifecycle.composition();
    auto& facade = v->lifecycle.facade();
    const auto state = facade.readRuntimeState();
    const auto targets = facade.mountedCanvasTargets();
    const auto control = targets.empty() ? runtime::CanvasControlSnapshot{}
                                         : facade.readCanvasControlSnapshot(targets.front());
    if (v->request) {
        if (const auto receipt = facade.canvasControlReceipt({0x574542U, v->request}))
            v->lastControl = receipt->state;
    }
    const auto s = comp.surfaceSnapshot();
    const auto& summary=control.selectionSummary;
    const auto identifier=[](const canvas::foundation::ObjectId& id) {
        std::ostringstream value;value<<std::hex<<std::setfill('0');
        for(const auto b:id.bytes)value<<std::setw(2)<<static_cast<unsigned>(b);
        return value.str();
    };
    nlohmann::json projection{{"status",static_cast<unsigned>(summary.status)},{"reason",static_cast<unsigned>(summary.reason)},
        {"count",summary.count},{"primary",identifier(summary.primary)},{"groupScope",nullptr},{"roots",nlohmann::json::array()},
        {"kindCounts",nlohmann::json::array()},{"selectionRevision",std::to_string(summary.selectionRevision)},
        {"semanticGeneration",std::to_string(summary.semanticGeneration)},{"sceneGeneration",std::to_string(summary.sceneGeneration)},
        {"sceneRevision",std::to_string(summary.sceneRevision)},{"bounds",{{"status",static_cast<unsigned>(summary.bounds.status)},
            {"reason",static_cast<unsigned>(summary.bounds.reason)},{"world",nullptr}}},
        {"properties",nlohmann::json::array()},{"actions",nlohmann::json::array()}};
    for(const auto id:summary.roots)projection["roots"].push_back(identifier(id));
    if(summary.groupScope)projection["groupScope"]=identifier(*summary.groupScope);
    for(const auto& count:summary.kindCounts)projection["kindCounts"].push_back({{"kind",count.kind},{"count",count.count}});
    if(summary.bounds.world) {
        const auto& b=*summary.bounds.world;projection["bounds"]["world"]={b.left,b.top,b.right,b.bottom};
    }
    for(const auto& property:summary.properties) {
        nlohmann::json row{{"fieldId",property.fieldId},{"state",static_cast<unsigned>(property.state)},
            {"reason",static_cast<unsigned>(property.reason)},{"applicableRoots",property.applicableRoots},
            {"knownRoots",property.knownRoots},{"applicableValuesMixed",property.applicableValuesMixed},{"value",nullptr}};
        if(property.value) {
            if(const auto* boolean=std::get_if<bool>(&*property.value))row["value"]=*boolean;
            else if(const auto* scalar=std::get_if<float>(&*property.value))row["value"]=*scalar;
        }
        projection["properties"].push_back(std::move(row));
    }
    for(std::size_t i=0;i<summary.actions.size();++i)projection["actions"].push_back({{"action",i},
        {"state",static_cast<unsigned>(summary.actions[i].state)},{"reason",static_cast<unsigned>(summary.actions[i].reason)}});
    std::ostringstream out;
    out << std::boolalpha << "{\"owner\":\"RuntimeComposition\",\"revision\":\""
        << AXIOM_BUILD_SOURCE_REVISION << "\",\"dirty\":" << AXIOM_BUILD_DIRTY
        << ",\"debugUiEnabled\":" << (AXIOM_WEB_DEBUG_UI != 0)
        << ",\"state\":" << static_cast<int>(comp.state())
        << ",\"objects\":" << comp.semanticObjectCount()
        << ",\"operations\":" << state.control.appliedCommitOrdinal
        << ",\"pointerSamples\":" << v->pointerSamples
        << ",\"rejectedInputs\":" << v->rejectedInputs
        << ",\"activePointers\":" << v->pointers.size()
        << ",\"presentedFrames\":" << v->presentedFrames << ",\"retries\":" << v->retries
        << ",\"pendingFrame\":" << v->pending.has_value()
        << ",\"previewActive\":" << comp.previewActive()
        << ",\"surfaceGeneration\":" << s.surfaceGeneration
        << ",\"metricsGeneration\":" << s.metricsGeneration << ",\"width\":" << s.width
        << ",\"height\":" << s.height << ",\"logicalWidth\":" << s.logicalWidth
        << ",\"logicalHeight\":" << s.logicalHeight << ",\"dpr\":" << s.devicePixelRatio
        << ",\"controlState\":" << static_cast<int>(v->lastControl)
        << ",\"tool\":" << static_cast<int>(control.tool)
        << ",\"selectedCount\":" << control.selectedObjectCount
        << ",\"selectionMode\":" << static_cast<int>(control.selectionAcquisitionMode)
        << ",\"regionActive\":" << control.regionSelectionActive
        << ",\"regionCandidates\":" << control.regionCandidateCount
        << ",\"regionQuery\":" << static_cast<int>(control.selectionQueryStatus)
        << ",\"creationMode\":" << control.creationMode
        << ",\"creationPreset\":" << static_cast<int>(control.creationPreset)
        << ",\"size\":" << control.size << ",\"opacity\":" << control.opacity
        << ",\"camera\":{\"scale\":" << state.camera.scale << ",\"x\":" << state.camera.translationX
        << ",\"y\":" << state.camera.translationY
        << "},\"lastPointer\":{\"source\":" << v->lastPointer.source
        << ",\"pointer\":" << v->lastPointer.pointer
        << ",\"generation\":" << v->lastPointer.generation
        << "},\"error\":" << std::quoted(v->error) << ",\"selectionSummary\":" << projection.dump() << "}";
    json = out.str();
    return json.c_str();
}
EMSCRIPTEN_KEEPALIVE int
axiom_web_debug(std::uintptr_t h, int phase, float x, float y, int button) {
#if AXIOM_WEB_DEBUG_UI
    auto* v = instance(h);
    return v && v->debug && v->debug->input(phase, x, y, button);
#else
    (void)h;
    (void)phase;
    (void)x;
    (void)y;
    (void)button;
    return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int axiom_web_debug_text(std::uintptr_t h, const char* value) {
#if AXIOM_WEB_DEBUG_UI
    auto* v = instance(h);
    return v && v->debug && v->debug->text(value);
#else
    (void)h;
    (void)value;
    return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE std::uint64_t axiom_web_debug_image_request(std::uintptr_t h) {
#if AXIOM_WEB_DEBUG_UI
    auto* v = instance(h);
    return v && v->debug ? v->debug->beginImageFileRequest() : 0;
#else
    (void)h;
    return 0;
#endif
}
EMSCRIPTEN_KEEPALIVE int axiom_web_debug_image_result(
    std::uintptr_t h, std::uint64_t token, const std::uint8_t* bytes, std::size_t size,
    const char* mime, const char* error, int cancelled) {
#if AXIOM_WEB_DEBUG_UI
    auto* v = instance(h);
    if (!v || !v->debug) return 0;
    canvas::debug_ui::ImageFileResult result;
    result.cancelled = cancelled != 0;
    result.mimeType = mime ? mime : "";
    result.error = error ? error : "";
    if (size > 128U * 1024U * 1024U || (size && !bytes))
        result.error = "Image file buffer is invalid or exceeds 128 MiB";
    else if (size)
        result.bytes.assign(bytes, bytes + size);
    return v->debug->completeImageFileRequest(token, result);
#else
    (void)h; (void)token; (void)bytes; (void)size; (void)mime; (void)error; (void)cancelled;
    return 0;
#endif
}
}

#include "text_bridge.inl"
#include "section_bridge.inl"

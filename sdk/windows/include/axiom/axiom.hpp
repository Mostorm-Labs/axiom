#pragma once
#include "api.h"
#include "detail/json.hpp"

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace axiom {
using Json = nlohmann::json;
class Error : public std::runtime_error {
  public:
    int status;
    Json details;
    Error(int s, std::string message, Json d = {})
        : std::runtime_error(std::move(message)), status(s), details(std::move(d)) {};
};
inline void check(int status) {
    if (status)
        throw Error(status, "Axiom ABI status " + std::to_string(status));
}
class Buffer final {
    axiom_buffer id_;

  public:
    explicit Buffer(axiom_buffer id) : id_(id) {}
    ~Buffer() {
        if (id_)
            axiom_buffer_release(id_);
    }
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    std::vector<uint8_t> bytes() const {
        if (!id_)
            return {};
        size_t n = 0;
        check(axiom_buffer_size(id_, &n));
        std::vector<uint8_t> b(n);
        check(axiom_buffer_copy(id_, b.data(), b.size(), nullptr));
        return b;
    }
    std::string text() const {
        auto b = bytes();
        return {b.begin(), b.end()};
    }
};
struct Options {
    void* parentWindow = nullptr;
    std::wstring assetRoot;
    Json open = Json::object();
    std::vector<uint8_t> snapshot;
    std::function<void(std::span<const uint8_t>)> onOperation;
    std::function<void(uint32_t)> onError;
};
struct Response {
    Json value;
    std::vector<uint8_t> bytes;
};
// Caller-side facade. This header and all STL values compile inside the app's CRT.
// Runtime, its callbacks, and its HWND must be closed/destroyed on their owner thread.
class Runtime final {
    Options options_;
    axiom_runtime id_ = 0;
    uint64_t textRequest_ = 0;
    struct Listener {
        std::function<void(uint32_t, const Json&, std::span<const uint8_t>)> call;
    };
    std::map<uint64_t, std::unique_ptr<Listener>> listeners_;
    static int event(void* p,
                     uint32_t kind,
                     const char* json,
                     size_t n,
                     const uint8_t* bytes,
                     size_t size) noexcept {
        try {
            static_cast<Listener*>(p)->call(kind, Json::parse(json, json + n), {bytes, size});
            return AXIOM_OK;
        } catch (...) {
            return AXIOM_INTERNAL_ERROR;
        }
    }
    static void operation(void* p, const uint8_t* b, size_t n) noexcept {
        auto& self = *static_cast<Runtime*>(p);
        try {
            if (self.options_.onOperation)
                self.options_.onOperation({b, n});
        } catch (...) {
            failure(p, AXIOM_INTERNAL_ERROR);
        }
    }
    static void failure(void* p, uint32_t e) noexcept {
        try {
            auto& self = *static_cast<Runtime*>(p);
            if (self.options_.onError)
                self.options_.onError(e);
        } catch (...) {
        }
    }

  public:
    explicit Runtime(Options options) : options_(std::move(options)) {
        auto json = options_.open.dump();
        axiom_options o{};
        o.size = sizeof(o);
        o.abi_version = AXIOM_ABI_VERSION;
        o.parent_window = options_.parentWindow;
        o.asset_root = options_.assetRoot.empty() ? nullptr : options_.assetRoot.c_str();
        o.open_json = json.c_str();
        o.snapshot = options_.snapshot.data();
        o.snapshot_size = options_.snapshot.size();
        o.callback_context = this;
        o.on_operation = operation;
        o.on_error = failure;
        axiom_buffer err = 0;
        auto s = axiom_create(&o, &id_, &err);
        Buffer error(err);
        if (s)
            throw Error(s, error.text());
    }
    ~Runtime() {
        if (id_ && axiom_abandon(id_) != AXIOM_OK)
            std::terminate();
    }
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
    axiom_runtime handle() const noexcept {
        return id_;
    }
    void* window() const {
        void* w = nullptr;
        check(axiom_window(id_, &w));
        return w;
    }
    Response
    invoke(std::string command, Json args = Json::object(), std::span<const uint8_t> bytes = {}) {
        args["command"] = command;
        const auto text = args.dump();
        axiom_buffer out = 0, raw = 0;
        auto s = axiom_call(id_, text.data(), text.size(), bytes.data(), bytes.size(), &out, &raw);
        Buffer response(out), binary(raw);
        check(s);
        auto j = Json::parse(response.text());
        if (!j.value("ok", false))
            throw Error(AXIOM_REJECTED, j.value("code", std::string{"REJECTED"}), j);
        return {std::move(j), binary.bytes()};
    }
    bool close() {
        if (!id_)
            return true;
        auto s = axiom_close(id_);
        if (s == AXIOM_BUSY)
            return false;
        check(s);
        id_ = 0;
        return true;
    }
    void abandon() {
        if (id_) {
            check(axiom_abandon(id_));
            id_ = 0;
        }
    }
    Json state() {
        return invoke("state").value.at("value");
    }
    Json frame(bool force = false) {
        auto result = invoke("frame", {{"force", force}}).value;
        dispatchEvents();
        return result;
    }
    uint64_t subscribe(uint32_t mask,
                       std::function<void(uint32_t, const Json&, std::span<const uint8_t>)> fn) {
        auto listener = std::make_unique<Listener>();
        listener->call = std::move(fn);
        uint64_t token = 0;
        check(axiom_subscribe(id_, mask, event, listener.get(), &token));
        try {
            listeners_.emplace(token, std::move(listener));
        } catch (...) {
            (void)axiom_unsubscribe(id_, token);
            throw;
        }
        return token;
    }
    void unsubscribe(uint64_t token) {
        check(axiom_unsubscribe(id_, token));
        listeners_.erase(token);
    }
    void dispatchEvents() {
        check(axiom_dispatch_events(id_));
    }
    Json attachData(Json options = Json::object()) {
        options["action"] = "attach";
        return invoke("data", options).value.at("value");
    }
    Json dataStatus() {
        return invoke("data", {{"action", "status"}}).value.at("value");
    }
    Response nextOperation(Json after = nullptr) {
        return invoke("data", {{"action", "next"}, {"after", after}});
    }
    void releaseOperations(Json packet) {
        (void)invoke("data", {{"action", "release"}, {"packet", packet}});
    }
    void reportOperationStatus(Json packet, unsigned localDurable, unsigned cloudSynced) {
        (void)invoke("data",
                     {{"action", "report"},
                      {"packet", packet},
                      {"localDurable", localDurable},
                      {"cloudSynced", cloudSynced}});
    }
    Json applyRemoteBatch(Json packets) {
        return invoke("remoteBatch", {{"packets", packets}}).value.at("value");
    }
    Json applyReplay(std::span<const uint8_t> bytes) { return invoke("replay",Json::object(),bytes).value; }
    Json applyReplayBatch(Json packets) { return invoke("replayBatch",{{"packets",packets}}).value.at("value"); }
    void resize(float width, float height, float dpr) {
        (void)invoke("resize", {{"width", width}, {"height", height}, {"dpr", dpr}});
    }
    void loseSurface() {
        (void)invoke("lose");
    }
    Json control(std::string kind, Json args = Json::object()) {
        args["kind"] = kind;
        return invoke("control", args).value;
    }
    void ink() {
        control("tool", {{"value", 0}});
    }
    void select() {
        control("tool", {{"value", 1}});
    }
    void erase() {
        control("tool", {{"value", 2}});
    }
    void text() {
        control("tool", {{"value", 4}});
    }
    void rectangle() {
        control("shape", {{"value", 0}});
    }
    void ellipse() {
        control("shape", {{"value", 1}});
    }
    void triangle() {
        control("shape", {{"value", 2}});
    }
    void sticky() {
        control("shape", {{"value", 5}});
    }
    void line() {
        control("shape", {{"value", 3}});
    }
    void arrow() {
        control("shape", {{"value", 4}});
    }
    void undo() {
        control("undo");
    }
    void redo() {
        control("redo");
    }
    void remove() {
        control("delete");
    }
    void fit() {
        control("fit");
    }
    void selectionMode(unsigned mode) {
        control("selectionMode", {{"value", mode}});
    }
    Json buildInfo() {
        return invoke("version").value;
    }
    void hand() {
        control("tool", {{"value", 3}});
    }
    void objectEraser() {
        erase();
        control("eraser", {{"mode", 1}});
    }
    void partialEraser() {
        erase();
        control("eraser", {{"mode", 2}});
    }
    Json presets() {
        return invoke("presets").value.at("value");
    }
    void brush(std::string profile, uint32_t revision) {
        control("preset", {{"profileId", profile}, {"revision", revision}});
    }
    void inkOptions(Json options) {
        control("ink", options);
    }
    void eraserOptions(Json options) {
        control("eraser", options);
    }
    void appearance(Json options) {
        const auto s = state();
        options["expectedGeneration"] = s.at("generation");
        options["expectedSelectionRevision"] = s.at("selectionRevision");
        control("appearance", options);
    }
    void pan(float x, float y) {
        control("pan", {{"x", x}, {"y", y}});
    }
    void zoom(float x, float y, double scale) {
        control("zoom", {{"x", x}, {"y", y}, {"scale", scale}});
    }
    void fitSelection() {
        control("fit", {{"target", 1}});
    }
    void fitObject(std::string id) {
        control("fit", {{"target", 2}, {"objectId", id}});
    }
    void size(double width, double height) {
        control("size", {{"width", width}, {"height", height}});
    }
    void transform(std::array<double, 6> matrix) {
        control("transform", {{"matrix", matrix}});
    }
    void lock(bool value) {
        control("lock", {{"value", value}});
    }
    void visible(bool value) {
        control("visible", {{"value", value}});
    }
    void unlock() {
        control("unlock");
    }
    void order(bool front) {
        control("order", {{"front", front}});
    }
    Json selection() {
        return invoke("selection").value;
    }
    Json section(Json args) {
        return invoke("section", std::move(args)).value;
    }
    Json createSection(Json args) {
        args["action"] = 0;
        return section(args);
    }
    Json wrapInSection(Json args) {
        args["action"] = 1;
        return section(args);
    }
    Json releaseSection(std::string id) {
        return section({{"action", 2}, {"objectId", id}});
    }
    Json reparent(Json args) {
        args["action"] = 3;
        return section(args);
    }
    Json resizeSection(std::string id, std::array<double, 4> boundary) {
        return section({{"action", 4}, {"objectId", id}, {"boundary", boundary}});
    }
    Json renameSection(std::string id, std::string title) {
        return section({{"action", 5}, {"objectId", id}, {"title", title}});
    }
    Json selectSection(std::string id) {
        return section({{"action", 6}, {"objectId", id}});
    }
    Json selectSectionChildren(std::string id) {
        return section({{"action", 7}, {"objectId", id}});
    }
    Json enterGroup(std::string id) {
        return section({{"action", 8}, {"objectId", id}});
    }
    Json leaveGroup() {
        return section({{"action", 9}});
    }
    Json group() {
        auto s = state();
        return section({{"action", 10},
                        {"objects", s.at("selected")},
                        {"generation", s.at("generation")},
                        {"selectionRevision", s.at("selectionRevision")}});
    }
    Json ungroup(std::string id) {
        auto s = state();
        return section({{"action", 11},
                        {"objectId", id},
                        {"generation", s.at("generation")},
                        {"selectionRevision", s.at("selectionRevision")}});
    }
    Json readSection(std::string id) {
        return invoke("readSection", {{"objectId", id}}).value.at("value");
    }
    Json connector(Json args) {
        return invoke("connector", std::move(args)).value;
    }
    Json createConnector(Json args) {
        args["action"] = "create";
        return connector(args);
    }
    Json connectorRouting(Json args) {
        args["action"] = "routing";
        return connector(args);
    }
    Json readConnector(std::string id) {
        return connector({{"action", "read"}, {"objectId", id}}).at("value");
    }
    Json stickyOptions(Json args) {
        return invoke("sticky", std::move(args)).value;
    }
    Json createSticky(Json args) {
        args["action"] = "create";
        return stickyOptions(args);
    }
    Json readSticky() {
        return stickyOptions({{"action", "read"}}).at("value");
    }
    Json multi(Json args) {
        return invoke("multi", std::move(args)).value;
    }
    Json readMulti() {
        return multi({{"action", "read"}}).at("value");
    }
    void permission(unsigned mode) {
        multi({{"action", "permission"}, {"mode", mode}});
    }
    Json multiValues(unsigned field, Json stamp, Json eligible) {
        return multi({{"action", "values"},
                      {"field", field},
                      {"stamp", stamp},
                      {"eligible", eligible}})
            .at("value");
    }
    Json multiSet(unsigned field, Json value, Json stamp, Json eligible) {
        return multi({{"action", "set"},
                      {"field", field},
                      {"value", value},
                      {"stamp", stamp},
                      {"eligible", eligible}});
    }
    Json multiStrokeWidth(float width, Json stamp, Json eligible) {
        return multi(
            {{"action", "width"}, {"width", width}, {"stamp", stamp}, {"eligible", eligible}});
    }
    Json image(Json args, std::span<const uint8_t> bytes = {}) {
        return invoke("image", std::move(args), bytes).value;
    }
    Json importImage(std::span<const uint8_t> b, std::string mime) {
        return image({{"action", "import"}, {"mime", mime}}, b);
    }
    Json replaceImage(std::span<const uint8_t> b, std::string mime) {
        return image({{"action", "replace"}, {"mime", mime}}, b);
    }
    Json readImage() {
        return image({{"action", "read"}}).at("value");
    }
    Json readCrop() {
        return image({{"action", "readCrop"}}).at("value");
    }
    Json resizeImage(std::string id, double width, double height, bool freeStretch = false) {
        return image({{"action", "resize"},
                      {"objectId", id},
                      {"width", width},
                      {"height", height},
                      {"freeStretch", freeStretch}});
    }
    Json beginCrop(std::string id) {
        return image({{"action", "crop"}, {"objectId", id}});
    }
    Json updateCrop(Json args) {
        args["action"] = "update";
        return image(args);
    }
    Json commitCrop(std::string token) {
        return image({{"action", "done"}, {"token", token}});
    }
    Json cancelCrop(std::string token) {
        return image({{"action", "cancel"}, {"token", token}});
    }
    Json resource(Json args, std::span<const uint8_t> bytes = {}) {
        return invoke("resource", std::move(args), bytes).value;
    }
    Json resourceToken(std::string id) {
        return resource({{"action", "token"}, {"objectId", id}}).at("value");
    }
    Json resourceManifest() {
        return resource({{"action", "list"}}).at("value");
    }
    std::vector<uint8_t> readResourceBytes(Json token) {
        return invoke("resource", {{"action", "bytes"}, {"token", token}}).bytes;
    }
    Json resourceReceipt(std::string id) {
        return resource({{"action", "receipt"}, {"objectId", id}}).at("value");
    }
    Json publishResource(Json token, std::span<const uint8_t> bytes) {
        return resource({{"action", "publish"}, {"token", token}}, bytes);
    }
    Response snapshot(Json args) {
        return invoke("snapshot", std::move(args));
    }
    Json applyRemote(std::span<const uint8_t> bytes) {
        return invoke("remote", Json::object(), bytes).value;
    }
    Json textSnapshot() {
        return invoke("text", {{"action", "snapshot"}}).value.at("value");
    }
    Json fonts() {
        return invoke("text", {{"action", "fonts"}}).value.at("value");
    }
    Json textIntent(unsigned kind, Json args = Json::object()) {
        const auto s = textSnapshot();
        args["kind"] = kind;
        args["clientId"] = "1096304961";
        args["requestId"] = std::to_string(++textRequest_);
        args["target"] = s.at("target");
        args["token"] = s.at("token");
        return invoke("text", {{"action", "intent"}, {"intent", args}}).value;
    }
    Json textFormat(Json patch) {
        return textIntent(15, {{"format", patch}});
    }
    Json createText(float x, float y, double width = 320) {
        return textIntent(0, {{"x", x}, {"y", y}, {"width", width}});
    }
    Json editText(std::string id) {
        return textIntent(1, {{"object", id}});
    }
    Json editTextAt(float x, float y) {
        return textIntent(13, {{"x", x}, {"y", y}});
    }
    Json finishText() {
        return textIntent(2);
    }
    Json completeComposition() {
        return textIntent(14);
    }
    Json selectText(uint32_t start, uint32_t length) {
        auto s = textSnapshot();
        return invoke("textEdit",
                      {{"kind", 5}, {"token", s.at("token")}, {"range", {start, length}}})
            .value;
    }
    Json replaceText(std::u16string_view text) {
        auto s = textSnapshot();
        return invoke("textEdit",
                      {{"kind", 4},
                       {"token", s.at("token")},
                       {"range", s.at("selection")},
                       {"units", std::vector<uint16_t>(text.begin(), text.end())}})
            .value;
    }
    void registerSource(uint64_t source) {
        check(axiom_input_source(id_, source, 1));
    }
    void unregisterSource(uint64_t source) {
        check(axiom_input_source(id_, source, 0));
    }
    axiom_contact begin(std::span<const axiom_sample> b) {
        axiom_contact key{};
        check(axiom_input_begin(id_, b.data(), b.size(), &key));
        return key;
    }
    void submit(const axiom_contact& key, std::span<const axiom_sample> b) {
        check(axiom_input_submit(id_, &key, b.data(), b.size()));
    }
    void cancel(const axiom_contact& key, uint64_t sequence, uint64_t timestamp) {
        check(axiom_input_cancel(id_, &key, sequence, timestamp));
    }
};
} // namespace axiom

#include "session.hpp"

#include <canvas/platform/web_runtime_adapter.hpp>
#include <cmath>
#include <cstring>
#include <emscripten.h>
#include <emscripten/html5.h>
#include <stdexcept>

EM_JS(void, notifyOperation, (const std::uint8_t* bytes, std::size_t size), {
    Module.onLocalOperation ?.(HEAPU8.slice(bytes, bytes + size));
});
EM_JS(void, notifyDeliveryFailure, (), { Module.onDeliveryError ?.(); });
EM_JS(void, notifyPreview, (int visible), { Module.onPreview ?.(!!visible); });
namespace {
using namespace canvas;
class WebPlatform final : public sdk::SessionPlatform {
    std::unique_ptr<platform::WebRuntimeAdapter> adapter_;
    int canonical_ = 0, preview_ = 0;

  public:
    ~WebPlatform() override {
        release();
    }
    sdk::Json open(runtime::RuntimeComposition& owner, const sdk::Json& j) override {
        EmscriptenWebGLContextAttributes attr;
        emscripten_webgl_init_context_attributes(&attr);
        attr.majorVersion = 2;
        attr.stencil = true;
        attr.alpha = true;
        attr.antialias = false;
        attr.preserveDrawingBuffer = true;
        canonical_ =
            emscripten_webgl_create_context(j.at("canonical").get<std::string>().c_str(), &attr);
        preview_ =
            emscripten_webgl_create_context(j.at("preview").get<std::string>().c_str(), &attr);
        if (canonical_ <= 0 || preview_ <= 0)
            return {{"ok", false}, {"code", "WEBGL_UNAVAILABLE"}};
        adapter_ = std::make_unique<platform::WebRuntimeAdapter>(
            owner, [](void*, bool visible) { notifyPreview(visible); });
        const bool bound = resize(j.at("width"), j.at("height"), j.at("dpr"));
        return {{"ok", bound}, {"code", "SURFACE_REJECTED"}};
    }
    bool loadFonts() override {
        return adapter_ && adapter_->loadHostFonts("/axiom-font.otf", "/fonts");
    }
    bool resize(float w, float h, float d) override {
        if (!std::isfinite(w) || !std::isfinite(h) || !std::isfinite(d) || w <= 0 || h <= 0 ||
            d <= 0 || w * d > 16384 || h * d > 16384)
            return false;
        return adapter_ && adapter_->bind(canonical_,
                                          preview_,
                                          w,
                                          h,
                                          static_cast<std::uint32_t>(std::round(w * d)),
                                          static_cast<std::uint32_t>(std::round(h * d)),
                                          d);
    }
    bool lose() override {
        return adapter_ && adapter_->loseSurface();
    }
    sdk::Json frame(bool force) override {
        const auto result =
            adapter_->frameStatus().pending ? adapter_->pollFrame() : adapter_->prepareFrame(force);
        return {{"ok", result != platform::WebFrameResult::kRejected},
                {"code", "FRAME_REJECTED"},
                {"pending", adapter_->frameStatus().pending},
                {"retry", result == platform::WebFrameResult::kRetry},
                {"presented", std::to_string(adapter_->presentedCount())}};
    }
    bool shutdown() override {
        return !adapter_ || adapter_->shutdown();
    }
    void release() noexcept override {
        if (adapter_) {
            (void)adapter_->shutdown();
            adapter_.reset();
        }
        if (preview_ > 0)
            emscripten_webgl_destroy_context(preview_);
        if (canonical_ > 0)
            emscripten_webgl_destroy_context(canonical_);
        preview_ = canonical_ = 0;
    }
    std::uint64_t nowNs() const noexcept override {
        return static_cast<std::uint64_t>(emscripten_get_now() * 1000000.0);
    }
};
sdk::Session session(std::make_unique<WebPlatform>(),
                     {nullptr,
                      [](void*, const std::uint8_t* bytes, std::size_t size) noexcept {
                          notifyOperation(bytes, size);
                      },
                      [](void*) noexcept { notifyDeliveryFailure(); }});
} // namespace
extern "C" {
EMSCRIPTEN_KEEPALIVE const std::uint8_t* axiom_js_bytes() {
    return session.bytes().data();
}
EMSCRIPTEN_KEEPALIVE std::size_t axiom_js_size() {
    return session.bytes().size();
}
EMSCRIPTEN_KEEPALIVE const char*
axiom_js_call(const char* input, const std::uint8_t* data, std::size_t size) {
    static std::string output;
    if (!input || std::strlen(input) > 1048576 || size > 134217728 || (size && !data))
        return "{\"ok\":false,\"code\":\"INVALID_REQUEST\"}";
    try {
        session.activateTextAccess();
        output = session.call(input, {data, size});
        return output.c_str();
    } catch (const std::bad_alloc&) {
        return "{\"ok\":false,\"code\":\"OUT_OF_MEMORY\"}";
    } catch (...) {
        return "{\"ok\":false,\"code\":\"INTERNAL_FAILURE\"}";
    }
}
}

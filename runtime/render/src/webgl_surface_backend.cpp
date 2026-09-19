#include "canvas/render/webgl_surface_backend.hpp"

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"

#include <GLES3/gl3.h>
#include <emscripten/html5.h>

class SkCanvas;
namespace canvas::render::internal {
BackendSubmissionResult drawReferencePlanToSkCanvas(SkCanvas&, const FramePlan&);
}

namespace canvas::render {

struct WebGlSurfaceBackend::Impl final {
    WebGlSurfaceConfig config;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl = 0;
    sk_sp<GrDirectContext> context;
    sk_sp<SkSurface> surface;
    std::string error;

    ~Impl() {
        surface.reset();
        if (context) { context->abandonContext(); context.reset(); }
        if (webgl > 0) emscripten_webgl_destroy_context(webgl);
    }
};

WebGlSurfaceBackend::WebGlSurfaceBackend(WebGlSurfaceConfig config)
    : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
    if (impl_->config.canvasSelector.empty() || impl_->config.physicalWidth == 0U ||
        impl_->config.physicalHeight == 0U) {
        impl_->error = "invalid WebGL2 surface configuration";
        return;
    }
    if (emscripten_set_canvas_element_size(impl_->config.canvasSelector.c_str(),
                                           static_cast<int>(impl_->config.physicalWidth),
                                           static_cast<int>(impl_->config.physicalHeight)) != EMSCRIPTEN_RESULT_SUCCESS) {
        impl_->error = "canvas selector or dimensions rejected";
        return;
    }
    EmscriptenWebGLContextAttributes attributes;
    emscripten_webgl_init_context_attributes(&attributes);
    attributes.alpha = true;
    attributes.antialias = false;
    attributes.depth = false;
    attributes.stencil = true;
    attributes.premultipliedAlpha = true;
    attributes.preserveDrawingBuffer = true;
    attributes.majorVersion = 2;
    attributes.minorVersion = 0;
    attributes.enableExtensionsByDefault = false;
    impl_->webgl = emscripten_webgl_create_context(impl_->config.canvasSelector.c_str(), &attributes);
    if (impl_->webgl <= 0 || emscripten_webgl_make_context_current(impl_->webgl) != EMSCRIPTEN_RESULT_SUCCESS) {
        impl_->error = "WebGL2 context creation failed";
        return;
    }
    impl_->context = GrDirectContexts::MakeGL(GrGLMakeNativeInterface());
    if (!impl_->context) { impl_->error = "Skia WebGL2 context creation failed"; return; }
    GLint framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    GrGLFramebufferInfo info{static_cast<GrGLuint>(framebuffer), static_cast<GrGLenum>(GL_RGBA8)};
    const auto target = GrBackendRenderTargets::MakeGL(
        static_cast<int>(impl_->config.physicalWidth),
        static_cast<int>(impl_->config.physicalHeight), 1, 8, info);
    impl_->surface = SkSurfaces::WrapBackendRenderTarget(
        impl_->context.get(), target, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!impl_->surface) impl_->error = "Skia WebGL2 surface wrapping failed";
}

WebGlSurfaceBackend::~WebGlSurfaceBackend() = default;
bool WebGlSurfaceBackend::ready() const noexcept { return impl_ && impl_->surface != nullptr; }
const std::string& WebGlSurfaceBackend::error() const noexcept { return impl_->error; }

BackendSubmissionResult WebGlSurfaceBackend::submit(const FramePlan& plan) {
    if (!ready()) return BackendSubmissionResult::rejected(impl_->error);
    if (plan.frame.metrics.physicalWidth != impl_->config.physicalWidth ||
        plan.frame.metrics.physicalHeight != impl_->config.physicalHeight) {
        return BackendSubmissionResult::rejected("frame metrics do not match WebGL2 target");
    }
    const auto result = internal::drawReferencePlanToSkCanvas(*impl_->surface->getCanvas(), plan);
    if (result.code != BackendSubmissionCode::kAccepted) return result;
    impl_->context->flushAndSubmit(impl_->surface.get(), GrSyncCpu::kNo);
    return BackendSubmissionResult::accepted();
}

} // namespace canvas::render

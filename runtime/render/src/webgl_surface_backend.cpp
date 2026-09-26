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
#include <limits>
#include <utility>

namespace canvas::render {
struct WebGlSurfaceProvider::Impl final {
    WebGlSurfaceConfig config;
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl = 0;
    sk_sp<GrDirectContext> context;
    sk_sp<SkSurface> surface;
    std::string error;
    std::uint64_t presents = 0;
    std::uint64_t generation = 1;
    bool acquired = false;
    bool lost = false;
    bool ownsContext = false;
    ~Impl() { surface.reset(); if (context) { context->abandonContext(); context.reset(); } if (ownsContext && webgl > 0) emscripten_webgl_destroy_context(webgl); }
};
WebGlSurfaceProvider::WebGlSurfaceProvider(WebGlSurfaceConfig config) : impl_(std::make_unique<Impl>()) {
    impl_->config = std::move(config);
    if ((impl_->config.canvasSelector.empty() && impl_->config.appOwnedContext <= 0) ||
        impl_->config.physicalWidth == 0U || impl_->config.physicalHeight == 0U) { impl_->error = "invalid WebGL2 surface configuration"; return; }
    EmscriptenWebGLContextAttributes attributes; emscripten_webgl_init_context_attributes(&attributes);
    attributes.alpha = true; attributes.antialias = false; attributes.depth = false; attributes.stencil = true; attributes.premultipliedAlpha = true; attributes.majorVersion = 2; attributes.minorVersion = 0; attributes.enableExtensionsByDefault = false; attributes.preserveDrawingBuffer = true;
    if (impl_->config.appOwnedContext > 0) {
      impl_->webgl = static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(impl_->config.appOwnedContext);
    } else {
      if (emscripten_set_canvas_element_size(impl_->config.canvasSelector.c_str(), static_cast<int>(impl_->config.physicalWidth), static_cast<int>(impl_->config.physicalHeight)) != EMSCRIPTEN_RESULT_SUCCESS) { impl_->error = "canvas selector or dimensions rejected"; return; }
      impl_->webgl = emscripten_webgl_create_context(impl_->config.canvasSelector.c_str(), &attributes);
      impl_->ownsContext = true;
    }
    if (impl_->webgl <= 0 || emscripten_webgl_make_context_current(impl_->webgl) != EMSCRIPTEN_RESULT_SUCCESS) { impl_->error = "WebGL2 context creation failed"; return; }
    impl_->context = GrDirectContexts::MakeGL(GrGLMakeNativeInterface());
    if (!impl_->context) { impl_->error = "Skia WebGL2 context creation failed"; return; }
    GLint framebuffer = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer); GrGLFramebufferInfo info{static_cast<GrGLuint>(framebuffer), static_cast<GrGLenum>(GL_RGBA8)};
    const auto target = GrBackendRenderTargets::MakeGL(static_cast<int>(impl_->config.physicalWidth), static_cast<int>(impl_->config.physicalHeight), 1, 8, info);
    impl_->surface = SkSurfaces::WrapBackendRenderTarget(impl_->context.get(), target, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!impl_->surface) impl_->error = "Skia WebGL2 surface wrapping failed";
}
std::unique_ptr<WebGlSurfaceProvider> WebGlSurfaceProvider::fromCurrentContext(
    std::uint32_t physicalWidth, std::uint32_t physicalHeight) {
  const auto current = emscripten_webgl_get_current_context();
  if (current <= 0) return nullptr;
  return std::make_unique<WebGlSurfaceProvider>(WebGlSurfaceConfig{
      {}, physicalWidth, physicalHeight, static_cast<std::int32_t>(current)});
}
WebGlSurfaceProvider::~WebGlSurfaceProvider() = default;
bool WebGlSurfaceProvider::ready() const noexcept { return impl_ && impl_->surface != nullptr; }
const std::string& WebGlSurfaceProvider::error() const noexcept { return impl_->error; }
RenderTargetInfo WebGlSurfaceProvider::describe() const noexcept { return {"webgl-window", RenderTargetKind::kGpuWindow, RenderTargetBackend::kWebGL2, RenderTargetFormat::kRgba8888, {static_cast<float>(impl_->config.physicalWidth), static_cast<float>(impl_->config.physicalHeight), impl_->config.physicalWidth, impl_->config.physicalHeight, 1.0F, 1.0F}, {true, false, false, false, true, true}}; }
SkiaSurfaceAcquireResult WebGlSurfaceProvider::acquire() noexcept { if (!ready() || impl_->lost) return SkiaSurfaceAcquireResult::rejected(SkiaSurfaceAcquireCode::kLost, impl_->lost ? "WebGL surface is lost" : impl_->error); impl_->acquired = true; return SkiaSurfaceAcquireResult::acquired({impl_->surface.get(), impl_->generation}); }
void WebGlSurfaceProvider::release() noexcept { if (impl_) impl_->acquired = false; }
BackendSubmissionResult WebGlSurfaceProvider::lose() noexcept { if (!impl_) return BackendSubmissionResult::rejected("WebGL surface is not initialized"); impl_->acquired = false; impl_->lost = true; return BackendSubmissionResult::accepted(); }
BackendSubmissionResult WebGlSurfaceProvider::present() noexcept { if (!ready()) return BackendSubmissionResult::rejected("WebGL surface is not ready"); impl_->context->flushAndSubmit(impl_->surface.get(), GrSyncCpu::kNo); ++impl_->presents; return BackendSubmissionResult::accepted(); }
BackendSubmissionResult WebGlSurfaceProvider::resize(std::uint32_t, std::uint32_t) noexcept { return BackendSubmissionResult::rejected("WebGL surface resize requires provider recreation"); }
BackendSubmissionResult WebGlSurfaceProvider::readbackRgba(std::span<std::uint8_t>) noexcept { return BackendSubmissionResult::rejected("WebGL provider does not expose readback"); }
std::uint64_t WebGlSurfaceProvider::presentCount() const noexcept { return impl_ == nullptr ? 0 : impl_->presents; }
std::uint64_t WebGlSurfaceProvider::generation() const noexcept { return impl_ == nullptr ? 0 : impl_->generation; }
BackendSubmissionResult WebGlSurfaceProvider::advanceGeneration() noexcept {
  if (!ready() || impl_->generation == std::numeric_limits<std::uint64_t>::max()) return BackendSubmissionResult::rejected("WebGL surface generation unavailable");
  ++impl_->generation;
  impl_->acquired = false;
  impl_->lost = false;
  return BackendSubmissionResult::accepted();
}
WebGlSurfaceBackend::WebGlSurfaceBackend(WebGlSurfaceConfig config) : provider_(std::move(config)) {}
WebGlSurfaceBackend::~WebGlSurfaceBackend() = default;
bool WebGlSurfaceBackend::ready() const noexcept { return provider_.ready(); }
const std::string& WebGlSurfaceBackend::error() const noexcept { return provider_.error(); }
BackendSubmissionResult WebGlSurfaceBackend::submit(const FramePlan& plan) { if (!ready()) return BackendSubmissionResult::rejected(error()); const auto info = provider_.describe(); if (plan.frame.metrics.physicalWidth != info.metrics.physicalWidth || plan.frame.metrics.physicalHeight != info.metrics.physicalHeight) return BackendSubmissionResult::rejected("frame metrics do not match WebGL2 target"); auto acquired = provider_.acquire(); if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) return BackendSubmissionResult::rejected(acquired.message); provider_.release(); return provider_.present(); }
BackendSubmissionResult WebGlSurfaceBackend::submitBrushPrimitives(std::span<const canvas::ink::BrushPrimitive> primitives) { const auto result = renderer_.renderPrimitives(provider_, primitives); if (result.code != BackendSubmissionCode::kAccepted) return result; auto acquired = provider_.acquire(); if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) return BackendSubmissionResult::rejected(acquired.message); provider_.release(); return provider_.present(); }
std::uint64_t WebGlSurfaceBackend::submissionCount() const noexcept { return renderer_.submissionCount(); }
std::uint64_t WebGlSurfaceBackend::flushCount() const noexcept { return provider_.presentCount(); }
BackendSubmissionResult WebGlSurfaceBackend::submitBrushPoints(std::span<const BrushRenderPoint> points) { return submitBrushPoints(points, CanonicalViewportTransform{}); }
BackendSubmissionResult WebGlSurfaceBackend::submitBrushPoints(std::span<const BrushRenderPoint> points, CanonicalViewportTransform viewport) { const auto result = renderer_.renderBrushPoints(provider_, points, viewport.scale, viewport.translationX, viewport.translationY); if (result.code != BackendSubmissionCode::kAccepted) return result; auto acquired = provider_.acquire(); if (acquired.code != SkiaSurfaceAcquireCode::kAcquired) return BackendSubmissionResult::rejected(acquired.message); provider_.release(); return provider_.present(); }
} // namespace canvas::render

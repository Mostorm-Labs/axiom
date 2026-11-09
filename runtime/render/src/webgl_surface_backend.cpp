#include "canvas/render/webgl_surface_backend.hpp"
#include "webgl_context_liveness.hpp"

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
#include <cstdio>

namespace canvas::render {
struct WebGlSurfaceProvider::Impl final : GpuCompletionService {
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
    bool overlayVisible = true;
    bool completionCapable=false,retainedContents=false;
    std::shared_ptr<GpuCompletionState> completion=std::make_shared<GpuCompletionState>();
    void observe([[maybe_unused]] const char* event)const noexcept {
#if defined(AXIOM_ENABLE_FRAME_TIMING)
      std::fprintf(stderr,"AXIOM_GPU_CONTEXT {\"event\":\"%s\",\"lifetime\":\"%llu\",\"native_handle\":\"%llu\",\"role\":\"%s\",\"generation\":\"%llu\",\"abandoned\":%s,\"physical_loss\":%s}\n",event,static_cast<unsigned long long>(completion->lifetime),static_cast<unsigned long long>(webgl),config.overlayVisibility?"preview":"canonical",static_cast<unsigned long long>(generation),completion->abandoned.load()?"true":"false",completion->physicalLost.load()?"true":"false");
#endif
    }
    std::shared_ptr<const GpuCompletionState> state()const noexcept override{return completion;}
    void service()noexcept override {
      if(!context||completion->abandoned.load())return;
      const auto previous=emscripten_webgl_get_current_context();
      const auto liveness=internal::probeWebGlContext(webgl,
        [](int handle,int* width,int* height){
          return emscripten_webgl_get_drawing_buffer_size(handle,width,height)==EMSCRIPTEN_RESULT_SUCCESS;
        },[](int handle){return emscripten_is_webgl_context_lost(handle);});
      if(liveness!=internal::WebGlContextLiveness::kAlive){
        const bool physical=liveness==internal::WebGlContextLiveness::kPhysicalLost;
        if(physical)completion->physicalLost=true;
        completion->abandoned=true;observe(physical?"physical-loss":"unknown-abandon");context->abandonContext();return;
      }
      if(emscripten_webgl_make_context_current(webgl)!=EMSCRIPTEN_RESULT_SUCCESS)return;
      context->checkAsyncWorkCompletion();
      if(previous!=webgl)(void)emscripten_webgl_make_context_current(previous);
    }
    ~Impl() { completion->abandoned=true;observe("owner-close");surface.reset(); if (context) { context->abandonContext(); context.reset(); } if (ownsContext && webgl > 0) emscripten_webgl_destroy_context(webgl); }
};
WebGlSurfaceProvider::WebGlSurfaceProvider(WebGlSurfaceConfig config) : impl_(std::make_shared<Impl>()) {
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
    EmscriptenWebGLContextAttributes actual;
    impl_->completionCapable=emscripten_webgl_get_context_attributes(impl_->webgl,&actual)==EMSCRIPTEN_RESULT_SUCCESS&&actual.majorVersion>=2;
    impl_->retainedContents=impl_->completionCapable&&actual.preserveDrawingBuffer;
    impl_->context = GrDirectContexts::MakeGL(GrGLMakeNativeInterface());
    if (!impl_->context) { impl_->error = "Skia WebGL2 context creation failed"; return; }
    GLint framebuffer = 0; glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer); GrGLFramebufferInfo info{static_cast<GrGLuint>(framebuffer), static_cast<GrGLenum>(GL_RGBA8)};
    const auto target = GrBackendRenderTargets::MakeGL(static_cast<int>(impl_->config.physicalWidth), static_cast<int>(impl_->config.physicalHeight), 1, 8, info);
    impl_->surface = SkSurfaces::WrapBackendRenderTarget(impl_->context.get(), target, kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!impl_->surface) impl_->error = "Skia WebGL2 surface wrapping failed";
    impl_->observe("created");
}
std::unique_ptr<WebGlSurfaceProvider> WebGlSurfaceProvider::fromCurrentContext(
    std::uint32_t physicalWidth, std::uint32_t physicalHeight) {
  const auto current = emscripten_webgl_get_current_context();
  if (current <= 0) return nullptr;
  return std::make_unique<WebGlSurfaceProvider>(WebGlSurfaceConfig{
      {}, physicalWidth, physicalHeight, static_cast<std::int32_t>(current), {}});
}
WebGlSurfaceProvider::~WebGlSurfaceProvider() = default;
std::shared_ptr<GpuCompletionService> WebGlSurfaceProvider::gpuCompletionService()const noexcept {
  return impl_&&impl_->completionCapable?impl_:nullptr;
}
bool WebGlSurfaceProvider::ready() const noexcept { return impl_ && impl_->surface != nullptr && !impl_->completion->abandoned.load(); }
const std::string& WebGlSurfaceProvider::error() const noexcept { return impl_->error; }
RenderTargetInfo WebGlSurfaceProvider::describe() const noexcept { return {"webgl-window", RenderTargetKind::kGpuWindow, RenderTargetBackend::kWebGL2, RenderTargetFormat::kRgba8888, {static_cast<float>(impl_->config.physicalWidth), static_cast<float>(impl_->config.physicalHeight), impl_->config.physicalWidth, impl_->config.physicalHeight, 1.0F, 1.0F}, {true, false, false, false, true, impl_->retainedContents}}; }
SkiaSurfaceAcquireResult WebGlSurfaceProvider::acquire() noexcept {
  if (!ready() || impl_->lost) {
    return SkiaSurfaceAcquireResult::rejected(
        SkiaSurfaceAcquireCode::kLost,
        impl_->lost ? "WebGL surface is lost" : impl_->error);
  }
  if (emscripten_webgl_make_context_current(impl_->webgl) != EMSCRIPTEN_RESULT_SUCCESS) {
    return SkiaSurfaceAcquireResult::rejected(
        SkiaSurfaceAcquireCode::kUnavailable, "WebGL context activation failed");
  }
  impl_->acquired = true;
  return SkiaSurfaceAcquireResult::acquired({impl_->surface.get(), impl_->generation});
}
void WebGlSurfaceProvider::release() noexcept { if (impl_) impl_->acquired = false; }
BackendSubmissionResult WebGlSurfaceProvider::lose() noexcept { if (!impl_) return BackendSubmissionResult::rejected("WebGL surface is not initialized"); impl_->service(); impl_->acquired = false; impl_->lost = true; return BackendSubmissionResult::accepted(); }
BackendSubmissionResult WebGlSurfaceProvider::present() noexcept {
  if (!ready() || impl_->lost) return BackendSubmissionResult::rejected("WebGL surface is not ready");
  impl_->service();
  if (!ready()) return BackendSubmissionResult::rejected("WebGL context unavailable or lost");
  if (emscripten_webgl_make_context_current(impl_->webgl) != EMSCRIPTEN_RESULT_SUCCESS) {
    return BackendSubmissionResult::rejected("WebGL context activation failed");
  }
  impl_->context->flush(impl_->surface.get());
  if (!impl_->context->submit(GrSyncCpu::kNo)) return BackendSubmissionResult::rejected("WebGL GPU submit failed");
  ++impl_->presents;
  return BackendSubmissionResult::accepted();
}
BackendSubmissionResult WebGlSurfaceProvider::resize(
    std::uint32_t width, std::uint32_t height) noexcept {
  if (!impl_ || width == 0U || height == 0U) {
    return BackendSubmissionResult::rejected("WebGL surface dimensions are invalid");
  }
  if (width == impl_->config.physicalWidth && height == impl_->config.physicalHeight) {
    return BackendSubmissionResult::accepted();
  }
  if (impl_->webgl <= 0 || !impl_->context ||
      emscripten_webgl_make_context_current(impl_->webgl) != EMSCRIPTEN_RESULT_SUCCESS) {
    return BackendSubmissionResult::rejected("WebGL context activation failed");
  }
  if (impl_->acquired) {
    return BackendSubmissionResult::rejected("WebGL surface is acquired");
  }

  // The Web App has already resized the drawing buffer. Rewrap that same
  // framebuffer in a new SkSurface while retaining the context and provider
  // identity. The registry will advance the provider generation atomically.
  impl_->surface.reset();
  GLint framebuffer = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  const auto target = GrBackendRenderTargets::MakeGL(
      static_cast<int>(width), static_cast<int>(height), 1, 8,
      GrGLFramebufferInfo{static_cast<GrGLuint>(framebuffer), static_cast<GrGLenum>(GL_RGBA8)});
  impl_->surface = SkSurfaces::WrapBackendRenderTarget(
      impl_->context.get(), target, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
  if (!impl_->surface) {
    impl_->lost = true;
    return BackendSubmissionResult::rejected("Skia WebGL2 surface resize failed");
  }
  impl_->config.physicalWidth = width;
  impl_->config.physicalHeight = height;
  impl_->lost = false;
  return BackendSubmissionResult::accepted();
}
BackendSubmissionResult WebGlSurfaceProvider::readbackRgba(std::span<std::uint8_t>) noexcept { return BackendSubmissionResult::rejected("WebGL provider does not expose readback"); }
std::uint64_t WebGlSurfaceProvider::presentCount() const noexcept { return impl_ == nullptr ? 0 : impl_->presents; }
std::uint64_t WebGlSurfaceProvider::generation() const noexcept { return impl_ == nullptr ? 0 : impl_->generation; }
void WebGlSurfaceProvider::setOverlayVisible(bool visible) noexcept {
  if (impl_ == nullptr) return;
  impl_->overlayVisible = visible;
  if (impl_->config.overlayVisibility) impl_->config.overlayVisibility(visible);
}
bool WebGlSurfaceProvider::overlayVisible() const noexcept {
  return impl_ != nullptr && impl_->overlayVisible;
}
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

#include "android_egl_surface_provider.hpp"

#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"

#include <algorithm>
#include <limits>

namespace canvas::ink_playground {

struct AndroidEglSkiaSurfaceProvider::Impl final {
  ANativeWindow* window = nullptr;
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLConfig config = nullptr;
  EGLContext context = EGL_NO_CONTEXT;
  EGLSurface surface = EGL_NO_SURFACE;
  sk_sp<GrDirectContext> grContext;
  sk_sp<SkSurface> skSurface;
  bool lost = false;
};

AndroidEglSkiaSurfaceProvider::~AndroidEglSkiaSurfaceProvider() {
  detach();
  delete impl_;
}

bool AndroidEglSkiaSurfaceProvider::attach(ANativeWindow* window,
                                           std::uint32_t width,
                                           std::uint32_t height) noexcept {
  if (window == nullptr || width == 0U || height == 0U) return false;
  if (impl_ == nullptr) impl_ = new Impl{};
  detach();
  impl_->window = window;
  impl_->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (impl_->display == EGL_NO_DISPLAY || !eglInitialize(impl_->display, nullptr, nullptr)) {
    detach(); return false;
  }
  const EGLint configAttributes[] = {
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
      EGL_NONE};
  EGLint count = 0;
  if (!eglChooseConfig(impl_->display, configAttributes, &impl_->config, 1, &count) || count != 1) {
    detach(); return false;
  }
  const EGLint contextAttributes[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  impl_->context = eglCreateContext(impl_->display, impl_->config, EGL_NO_CONTEXT,
                                    contextAttributes);
  impl_->surface = eglCreateWindowSurface(impl_->display, impl_->config, impl_->window, nullptr);
  if (impl_->context == EGL_NO_CONTEXT || impl_->surface == EGL_NO_SURFACE ||
      !eglMakeCurrent(impl_->display, impl_->surface, impl_->surface, impl_->context)) {
    detach(); return false;
  }
  impl_->grContext = GrDirectContexts::MakeGL();
  if (!impl_->grContext) { detach(); return false; }
  GLint framebuffer = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
  const GrGLFramebufferInfo framebufferInfo{
      static_cast<GrGLuint>(framebuffer), static_cast<GrGLenum>(GL_RGBA8)};
  const auto target = GrBackendRenderTargets::MakeGL(
      static_cast<int>(width), static_cast<int>(height), 0, 8, framebufferInfo);
  impl_->skSurface = SkSurfaces::WrapBackendRenderTarget(
      impl_->grContext.get(), target, kBottomLeft_GrSurfaceOrigin,
      kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
  if (!impl_->skSurface) { detach(); return false; }
  width_ = width;
  height_ = height;
  ++generation_;
  if (generation_ == 0U) generation_ = 1U;
  attached_ = true;
  acquired_ = false;
  impl_->lost = false;
  return true;
}

void AndroidEglSkiaSurfaceProvider::detach() noexcept {
  if (impl_ == nullptr) return;
  acquired_ = false;
  impl_->skSurface.reset();
  if (impl_->grContext) { impl_->grContext->abandonContext(); impl_->grContext.reset(); }
  if (impl_->display != EGL_NO_DISPLAY) {
    eglMakeCurrent(impl_->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (impl_->surface != EGL_NO_SURFACE) eglDestroySurface(impl_->display, impl_->surface);
    if (impl_->context != EGL_NO_CONTEXT) eglDestroyContext(impl_->display, impl_->context);
    eglTerminate(impl_->display);
  }
  impl_->surface = EGL_NO_SURFACE;
  impl_->context = EGL_NO_CONTEXT;
  impl_->display = EGL_NO_DISPLAY;
  impl_->config = nullptr;
  if (impl_->window != nullptr) ANativeWindow_release(impl_->window);
  impl_->window = nullptr;
  attached_ = false;
  impl_->lost = true;
}

render::RenderTargetInfo AndroidEglSkiaSurfaceProvider::describe() const noexcept {
  return {"android-gles", render::RenderTargetKind::kGpuWindow,
          render::RenderTargetBackend::kOpenGL, render::RenderTargetFormat::kRgba8888,
          {static_cast<float>(width_), static_cast<float>(height_), width_, height_, 1.0F, 1.0F},
          {true, false, false, false, true, false}};
}

render::SkiaSurfaceAcquireResult AndroidEglSkiaSurfaceProvider::acquire() noexcept {
  if (!attached_ || impl_ == nullptr || impl_->skSurface == nullptr || impl_->lost) {
    return render::SkiaSurfaceAcquireResult::rejected(
        render::SkiaSurfaceAcquireCode::kLost, "Android EGL surface is unavailable");
  }
  if (!eglMakeCurrent(impl_->display, impl_->surface, impl_->surface, impl_->context)) {
    impl_->lost = true;
    return render::SkiaSurfaceAcquireResult::rejected(
        render::SkiaSurfaceAcquireCode::kLost, "eglMakeCurrent failed");
  }
  acquired_ = true;
  return render::SkiaSurfaceAcquireResult::acquired({impl_->skSurface.get(), generation_});
}

void AndroidEglSkiaSurfaceProvider::release() noexcept { acquired_ = false; }

render::BackendSubmissionResult AndroidEglSkiaSurfaceProvider::lose() noexcept {
  if (impl_ == nullptr) return render::BackendSubmissionResult::rejected("Android EGL provider is not initialized");
  impl_->lost = true;
  acquired_ = false;
  return render::BackendSubmissionResult::accepted();
}

render::BackendSubmissionResult AndroidEglSkiaSurfaceProvider::present() noexcept {
  if (!attached_ || impl_ == nullptr || impl_->grContext == nullptr || impl_->surface == EGL_NO_SURFACE) {
    return render::BackendSubmissionResult::rejected("Android EGL surface is unavailable");
  }
  impl_->grContext->flushAndSubmit(impl_->skSurface.get(), GrSyncCpu::kNo);
  if (!eglSwapBuffers(impl_->display, impl_->surface)) {
    impl_->lost = true;
    return render::BackendSubmissionResult::rejected("eglSwapBuffers failed");
  }
  ++presents_;
  return render::BackendSubmissionResult::accepted();
}

render::BackendSubmissionResult AndroidEglSkiaSurfaceProvider::resize(
    std::uint32_t width, std::uint32_t height) noexcept {
  if (width == 0U || height == 0U || !attached_) {
    return render::BackendSubmissionResult::rejected("Android EGL surface is not attached");
  }
  if (width == width_ && height == height_) return render::BackendSubmissionResult::accepted();
  // SurfaceView recreation supplies a new native window. Keep the old target
  // invalid until attach() establishes the new EGL surface.
  width_ = width;
  height_ = height;
  ++generation_;
  return render::BackendSubmissionResult::accepted();
}

render::BackendSubmissionResult AndroidEglSkiaSurfaceProvider::readbackRgba(
    std::span<std::uint8_t>) noexcept {
  return render::BackendSubmissionResult::rejected("Android GLES provider does not expose readback");
}

render::BackendSubmissionResult AndroidEglSkiaSurfaceProvider::advanceGeneration() noexcept {
  if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
    return render::BackendSubmissionResult::rejected("Android EGL generation exhausted");
  }
  ++generation_;
  return render::BackendSubmissionResult::accepted();
}

}  // namespace canvas::ink_playground

#include "canvas/platform/web_runtime_adapter.hpp"
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include "canvas/render/webgl_surface_backend.hpp"
#include "host_font_resources.hpp"

#include <GLES3/gl3.h>
#include <emscripten/html5.h>
#include <cmath>
#include <limits>
#include <optional>
#include <thread>
#include <utility>

namespace canvas::platform {
namespace {
struct Fence final {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context = 0;
  GLsync sync = nullptr;
  void clear() noexcept {
    if (sync && emscripten_webgl_make_context_current(context) == EMSCRIPTEN_RESULT_SUCCESS)
      glDeleteSync(sync);
    sync = nullptr;
  }
  bool insert(EMSCRIPTEN_WEBGL_CONTEXT_HANDLE value) noexcept {
    context = value;
    if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS) return false;
    sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();
    return sync != nullptr;
  }
  int poll() const noexcept {
    if (!sync) return 1;
    if (emscripten_webgl_make_context_current(context) != EMSCRIPTEN_RESULT_SUCCESS) return 0;
    const auto result = glClientWaitSync(sync, 0, 0);
    return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED ? 1
         : result == GL_TIMEOUT_EXPIRED ? 2 : 0;
  }
};
}  // namespace

class WebRuntimeAdapter::Impl final {
 public:
  explicit Impl(runtime::RuntimeComposition& owner, PreviewVisibilityCallback callback, void* context)
      : core(runtime::RuntimePlatformAccess::core(owner)), visibility(callback), visibilityContext(context) {}
  runtime::RuntimeCompositionCore& core;
  const std::thread::id thread = std::this_thread::get_id();
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE canonical = 0, preview = 0;
  Fence canonicalFence, previewFence;
  std::optional<runtime::RuntimeFrameOpportunity> pending;
  std::uint64_t presented = 0;
  bool bound = false;
  bool hostFontsLoaded = false;
  PreviewVisibilityCallback visibility = nullptr;
  void* visibilityContext = nullptr;
  void discard() noexcept { canonicalFence.clear(); previewFence.clear(); pending.reset(); }
};

WebRuntimeAdapter::WebRuntimeAdapter(runtime::RuntimeComposition& owner,
    PreviewVisibilityCallback visibility, void* context)
    : impl_(std::make_unique<Impl>(owner, visibility, context)) {}
WebRuntimeAdapter::~WebRuntimeAdapter() { (void)shutdown(); }
bool WebRuntimeAdapter::bind(std::int32_t canonical, std::int32_t preview,
    float lw, float lh, std::uint32_t width, std::uint32_t height, float dpr) noexcept {
  if (std::this_thread::get_id() != impl_->thread || canonical <= 0 || preview <= 0 || canonical == preview ||
      !width || !height || width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
      !std::isfinite(lw) || !std::isfinite(lh) || !std::isfinite(dpr) ||
      lw <= 0 || lh <= 0 || dpr <= 0) return false;
  try {
    for (const auto context : {canonical, preview}) {
      int actualWidth = 0, actualHeight = 0;
      if (emscripten_webgl_get_drawing_buffer_size(
              static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(context), &actualWidth, &actualHeight) != EMSCRIPTEN_RESULT_SUCCESS ||
          actualWidth != static_cast<int>(width) || actualHeight != static_cast<int>(height)) return false;
    }
    impl_->discard();
    const render::SurfaceMetrics metrics{lw, lh, width, height, dpr, 1.0F};
    const auto before = impl_->core.surfaceSnapshot();
    if (before.available && before.width == width && before.height == height &&
        impl_->canonical == static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(canonical) &&
        impl_->preview == static_cast<EMSCRIPTEN_WEBGL_CONTEXT_HANDLE>(preview))
      return impl_->core.resizeSurface(metrics);
    if (impl_->core.state() == runtime::RuntimeCompositionState::kReady && !impl_->core.loseSurface()) return false;
    auto first = std::make_unique<render::WebGlSurfaceProvider>(
        render::WebGlSurfaceConfig{{}, width, height, canonical, {}});
    if (!first->ready()) return false;
    const auto visibility = impl_->visibility;
    void* const visibilityContext = impl_->visibilityContext;
    auto second = std::make_unique<render::WebGlSurfaceProvider>(
        render::WebGlSurfaceConfig{{}, width, height, preview, [visibility, visibilityContext](bool visible) {
          if (visibility) visibility(visibilityContext, visible);
        }});
    if (!second->ready()) return false;
    if (!impl_->core.registerSurfaceProvider("webgl-window", std::move(first)) ||
        !impl_->core.registerPreviewSurfaceProvider("webgl-preview", std::move(second))) return false;
    const bool accepted = impl_->core.state() == runtime::RuntimeCompositionState::kBound
        ? impl_->core.bindSurface(metrics) : impl_->core.rebindSurface(metrics);
    if (!accepted || !impl_->core.setPlatformPresentationDeferred(true)) return false;
    impl_->canonical = canonical; impl_->preview = preview; impl_->bound = true;
    return true;
  } catch (...) { return false; }
}
bool WebRuntimeAdapter::loseSurface() noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound) return false;
  impl_->discard();
  return impl_->core.loseSurface();
}
bool WebRuntimeAdapter::loadHostFonts(const char* cjkPath, const char* catalogRoot) noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound || impl_->hostFontsLoaded ||
      !cjkPath || !catalogRoot || impl_->core.semanticObjectCount() != 0) return false;
  try {
    impl_->hostFontsLoaded = minimal_runtime_host::bindHostTextFonts(impl_->core,
        std::filesystem::path(cjkPath), std::filesystem::path(catalogRoot));
    return impl_->hostFontsLoaded;
  } catch (...) { return false; }
}
WebFrameResult WebRuntimeAdapter::prepareFrame(bool force) noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound) return WebFrameResult::kRejected;
  if (impl_->pending) return WebFrameResult::kPending;
  runtime::RuntimeFrameOpportunity work{};
  if (!impl_->core.serviceFrameOpportunity(force, work)) return WebFrameResult::kRejected;
  if (!work.canonicalRequested) work.canonicalWork.reset();
  if (!work.canonicalWork && !work.previewWork)
    return impl_->core.canonicalPresentationDirty() ||
           (impl_->core.previewActive() && impl_->core.previewRenderState().dirty)
        ? WebFrameResult::kRetry : WebFrameResult::kIdle;
  if ((work.canonicalWork && !impl_->canonicalFence.insert(impl_->canonical)) ||
      (work.previewWork && !impl_->previewFence.insert(impl_->preview))) {
    impl_->discard(); return WebFrameResult::kRejected;
  }
  impl_->pending = work;
  return WebFrameResult::kPending;
}
WebFrameResult WebRuntimeAdapter::pollFrame() noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound) return WebFrameResult::kRejected;
  if (!impl_->pending) return WebFrameResult::kIdle;
  const auto canonical = impl_->canonicalFence.poll(), preview = impl_->previewFence.poll();
  if (!canonical || !preview) return WebFrameResult::kRejected;
  if (canonical == 2 || preview == 2) return WebFrameResult::kPending;
  const auto& work = *impl_->pending;
  if (work.previewWork && impl_->core.previewActive() &&
      impl_->core.previewRenderState().contentRevision == work.previewWork->contentRevision &&
      !impl_->core.finishFrameOpportunity({work.previewWork, true})) return WebFrameResult::kRejected;
  if (work.canonicalWork) {
    if (!impl_->core.receivePlatformPresented({*work.canonicalWork, render::PresentOutcome::kPresented,
            render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) return WebFrameResult::kRejected;
    ++impl_->presented;
  }
  impl_->discard();
  return WebFrameResult::kIdle;
}
WebFrameStatus WebRuntimeAdapter::frameStatus() const noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->pending) return {};
  const auto& work = *impl_->pending;
  return {true, work.canonicalWork ? work.canonicalWork->workId : 0U,
      work.previewWork ? work.previewWork->workId : 0U};
}
bool WebRuntimeAdapter::previewActive() const noexcept {
  return std::this_thread::get_id() == impl_->thread && impl_->bound && impl_->core.previewActive();
}
std::uint64_t WebRuntimeAdapter::presentedCount() const noexcept {
  return std::this_thread::get_id() == impl_->thread ? impl_->presented : 0;
}
bool WebRuntimeAdapter::shutdown() noexcept {
  if (std::this_thread::get_id() != impl_->thread) return false;
  impl_->discard();
  if (!impl_->bound) return true;
  impl_->bound = false;
  return impl_->core.state() != runtime::RuntimeCompositionState::kReady || impl_->core.loseSurface();
}
}  // namespace canvas::platform

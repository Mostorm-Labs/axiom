#include "canvas/platform/windows_runtime_adapter.hpp"

#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include "windows_d3d12_surface_provider.hpp"

#include <cmath>
#include <thread>
#include <utility>

namespace canvas::platform {
class WindowsRuntimeAdapter::Impl final {
 public:
  Impl(runtime::RuntimeComposition& owner, HWND window) : core(runtime::RuntimePlatformAccess::core(owner)), hwnd(window) {}
  runtime::RuntimeCompositionCore& core;
  HWND hwnd;
  const std::thread::id thread = std::this_thread::get_id();
  minimal_runtime_host::WindowsD3D12SurfaceProvider* canonical = nullptr;
  minimal_runtime_host::WindowsD3D12SurfaceProvider* preview = nullptr;
  runtime::RuntimeFrameOpportunity frame{};
  std::uint64_t lastPresentedFrame = 0;
  std::uint64_t canonicalConfirmed = 0;
  std::uint64_t previewConfirmed = 0;
  bool bound = false;
  bool prepared = false;
};

WindowsRuntimeAdapter::WindowsRuntimeAdapter(runtime::RuntimeComposition& owner, void* hwnd)
    : impl_(std::make_unique<Impl>(owner, static_cast<HWND>(hwnd))) {}
WindowsRuntimeAdapter::~WindowsRuntimeAdapter() { (void)shutdown(); }
bool WindowsRuntimeAdapter::bind(std::uint32_t width, std::uint32_t height, float scale) noexcept {
  if (std::this_thread::get_id() != impl_->thread || impl_->bound || !impl_->hwnd || !IsWindow(impl_->hwnd) || !width || !height ||
      !std::isfinite(scale) || scale <= 0) return false;
  try {
    auto canonical = std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(impl_->hwnd, true);
    auto* canonicalRaw = canonical.get();
    if (canonical->resize(width, height).code != render::BackendSubmissionCode::kAccepted ||
        !impl_->core.registerSurfaceProvider("windows-d3d12-canonical", std::move(canonical))) return false;
    auto preview = std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(impl_->hwnd, false);
    auto* previewRaw = preview.get();
    preview->setOverlayOffset(0, 0);
    if (preview->resize(width, height).code != render::BackendSubmissionCode::kAccepted ||
        !impl_->core.registerPreviewSurfaceProvider("windows-d3d12-preview", std::move(preview))) return false;
    if (!impl_->core.bindSurface({width / scale, height / scale, width, height, scale, scale})) return false;
    if (!impl_->core.setPlatformPresentationDeferred(true)) return false;
    impl_->canonical = canonicalRaw;
    impl_->preview = previewRaw;
    impl_->bound = true;
    return true;
  } catch (const std::bad_alloc&) { return false; }
}
bool WindowsRuntimeAdapter::resize(std::uint32_t width, std::uint32_t height, float scale) noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound || !width || !height || !std::isfinite(scale) || scale <= 0) return false;
  impl_->prepared = false;
  return impl_->core.resizeSurface({width / scale, height / scale, width, height, scale, scale});
}
bool WindowsRuntimeAdapter::loseSurface() noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound) return false;
  impl_->prepared = false;
  return impl_->core.loseSurface();
}
bool WindowsRuntimeAdapter::recoverSurface(std::uint32_t width, std::uint32_t height, float scale) noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound || !width || !height ||
      !std::isfinite(scale) || scale <= 0) return false;
  impl_->prepared = false;
  return impl_->core.rebindSurface({width / scale, height / scale, width, height, scale, scale});
}
bool WindowsRuntimeAdapter::prepareFrame(bool force) noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound || impl_->prepared) return false;
  impl_->frame = {};
  impl_->prepared = impl_->core.serviceFrameOpportunity(force, impl_->frame);
  return impl_->prepared;
}
WindowsFrameStatus WindowsRuntimeAdapter::frameStatus() const noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->prepared) return {};
  return {impl_->frame.needsPaint, impl_->frame.canonicalRequested, impl_->frame.previewRendered,
          impl_->frame.canonicalWork ? impl_->frame.canonicalWork->workId : 0U,
          impl_->frame.previewWork ? impl_->frame.previewWork->workId : 0U};
}
bool WindowsRuntimeAdapter::present() noexcept {
  if (std::this_thread::get_id() != impl_->thread || !impl_->bound || !impl_->prepared) return false;
  impl_->prepared = false;
  if (const auto& work = impl_->frame.canonicalWork; work && work->frame.frameId.value() != impl_->lastPresentedFrame) {
    const auto count = impl_->canonical->presentCount();
    if (count <= impl_->canonicalConfirmed) return false;
    if (!impl_->core.receivePlatformPresented({*work, render::PresentOutcome::kPresented,
                                                render::PresentEvidenceKind::kPlatformQualified, std::nullopt})) return false;
    impl_->canonicalConfirmed = count;
    impl_->lastPresentedFrame = work->frame.frameId.value();
  }
  const auto previewCount = impl_->preview->presentCount();
  if (impl_->frame.previewWork && previewCount <= impl_->previewConfirmed) return false;
  const bool accepted = impl_->core.finishFrameOpportunity({impl_->frame.previewWork, true});
  if (accepted && impl_->frame.previewWork) impl_->previewConfirmed = previewCount;
  return accepted;
}
void WindowsRuntimeAdapter::reposition() noexcept { if (std::this_thread::get_id() == impl_->thread && impl_->preview) impl_->preview->reposition(); }
bool WindowsRuntimeAdapter::shutdown() noexcept {
  if (std::this_thread::get_id() != impl_->thread) return false;
  if (!impl_->bound) return true;
  impl_->prepared = false;
  impl_->preview = nullptr;
  impl_->canonical = nullptr;
  impl_->bound = false;
  return impl_->core.state() != runtime::RuntimeCompositionState::kReady || impl_->core.loseSurface();
}
}  // namespace canvas::platform

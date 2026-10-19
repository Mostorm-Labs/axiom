#include "minimal_runtime_facade.hpp"

#include "canvas/render/render_target.hpp"

namespace canvas::minimal_runtime_host {

MinimalRuntimeFacade::MinimalRuntimeFacade(MinimalRuntimeHost& host) noexcept
    : host_(host) {}

runtime::RuntimeStateSnapshot MinimalRuntimeFacade::readRuntimeState() const noexcept {
  return host_.composition().controlFacade().readRuntimeState();
}
runtime::ProductControlReceipt MinimalRuntimeFacade::submitProductControl(
    const runtime::ProductControlRequest& request) noexcept {
  return host_.composition().controlFacade().submitProductControl(request);
}
runtime::ProductControlReceipt MinimalRuntimeFacade::submitCanvasControl(
    const runtime::CanvasControlRequest& request) noexcept {
  return host_.composition().controlFacade().submitCanvasControl(request);
}
runtime::CanvasControlSnapshot MinimalRuntimeFacade::readCanvasControlSnapshot(
    const runtime::CanvasTargetKey& target) const noexcept {
  return host_.composition().controlFacade().readCanvasControlSnapshot(target);
}
std::vector<runtime::CanvasTargetKey> MinimalRuntimeFacade::mountedCanvasTargets() const {
  return host_.composition().controlFacade().mountedCanvasTargets();
}
std::vector<runtime::BrushPresetRef> MinimalRuntimeFacade::brushPresets() const {
  return host_.composition().controlFacade().brushPresets();
}
std::uint64_t MinimalRuntimeFacade::controlSequence() const noexcept {
  return host_.composition().controlFacade().controlSequence();
}
std::optional<runtime::CanvasControlReceipt> MinimalRuntimeFacade::canvasControlReceipt(
    runtime::CanvasControlReceiptKey key) const noexcept {
  return host_.composition().controlFacade().canvasControlReceipt(key);
}
const runtime::CanvasTargetKey& MinimalRuntimeFacade::target() const noexcept {
  static thread_local runtime::CanvasTargetKey value{};
  const auto targets = host_.composition().controlFacade().mountedCanvasTargets();
  value = targets.empty() ? runtime::CanvasTargetKey{} : targets.front();
  return value;
}

std::size_t MinimalRuntimeFacade::process() noexcept {
  if (!host_.composition().ownerPump()) return 0U;
  return host_.composition().readback().lastOwnerControlCount;
}

runtime::PlatformDiagnosticsSnapshot
MinimalRuntimeFacade::readPlatformDiagnostics() const noexcept {
  runtime::PlatformDiagnosticsSnapshot out{};
  const auto surface = host_.composition().surfaceSnapshot();
  out.canonicalSurfaceGeneration = surface.surfaceGeneration;
  out.previewSurfaceGeneration = host_.composition().previewSurfaceGeneration();
  out.width = surface.width;
  out.height = surface.height;
  out.devicePixelRatio = surface.devicePixelRatio;
  out.presentCount = host_.composition().canonicalPresentCount();
  out.surfaceAvailable = surface.available;
  out.canonicalSurfaceMode = surface.available &&
      host_.composition().canonicalBackend() == render::RenderTargetBackend::kD3D12
      ? runtime::SurfaceMode::kGpuDefault : runtime::SurfaceMode::kCpuReference;
  return out;
}

runtime::TextReceipt MinimalRuntimeFacade::submitTextIntent(const runtime::TextIntent& r) { return host_.composition().controlFacade().submitTextIntent(r); }
runtime::TextReceipt MinimalRuntimeFacade::submitTextInput(const runtime::TextInputEvent& r) { return host_.composition().controlFacade().submitTextInput(r); }
runtime::TextEditorSnapshot MinimalRuntimeFacade::readTextEditor() const { return host_.composition().controlFacade().readTextEditor(); }
std::optional<runtime::TextReceipt> MinimalRuntimeFacade::textInputReceipt(const runtime::TextSessionToken& t,std::uint64_t s) const { return host_.composition().controlFacade().textInputReceipt(t,s); }
std::optional<runtime::TextReceipt> MinimalRuntimeFacade::textIntentReceipt(std::uint64_t c,std::uint64_t r) const { return host_.composition().controlFacade().textIntentReceipt(c,r); }
bool MinimalRuntimeFacade::acknowledgeTextInput(const runtime::TextSessionToken& t,std::uint64_t s) { return host_.composition().controlFacade().acknowledgeTextInput(t,s); }

}  // namespace canvas::minimal_runtime_host

namespace canvas::minimal_runtime_host { runtime::TextGeometrySnapshot MinimalRuntimeFacade::readTextGeometry(const std::optional<runtime::TextGeometryToken>& expected) const { return host_.composition().controlFacade().readTextGeometry(expected); } }

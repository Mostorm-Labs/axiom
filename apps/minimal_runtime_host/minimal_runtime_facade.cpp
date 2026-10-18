#include "minimal_runtime_facade.hpp"

#include "canvas/render/render_target.hpp"

namespace canvas::minimal_runtime_host {

MinimalRuntimeFacade::MinimalRuntimeFacade(MinimalRuntimeHost& host) noexcept
    : runtime::CanvasRuntimeFacade(host.runtime(), {1U, 1U, 1U, 1U}), host_(host) {
  host.composition().attachControlFacade(this);
}

std::size_t MinimalRuntimeFacade::process() noexcept {
  return runtime::CanvasRuntimeFacade::process(++ownerSequence_);
}

runtime::PlatformDiagnosticsSnapshot
MinimalRuntimeFacade::readPlatformDiagnostics() const noexcept {
  runtime::PlatformDiagnosticsSnapshot out{};
  const auto& editing = host_.runtime();
  out.canonicalSurfaceGeneration = editing.surface().generation;
  out.previewSurfaceGeneration = editing.previewSurfaceGeneration();
  out.width = editing.surface().width;
  out.height = editing.surface().height;
  out.devicePixelRatio = editing.surface().devicePixelRatio;
  out.presentCount = editing.canonicalFrameCount();
  out.surfaceAvailable = editing.surface().available;
  const auto* provider = const_cast<interaction::CanvasEditingRuntime&>(editing)
      .activeSurfaceProvider();
  out.canonicalSurfaceMode = provider != nullptr &&
      provider->describe().backend == render::RenderTargetBackend::kD3D12
      ? runtime::SurfaceMode::kGpuDefault : runtime::SurfaceMode::kCpuReference;
  return out;
}

}  // namespace canvas::minimal_runtime_host

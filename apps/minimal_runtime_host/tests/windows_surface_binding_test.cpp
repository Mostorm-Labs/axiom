#include "windows_d3d12_surface_provider.hpp"
#include "canvas/runtime/runtime_composition.hpp"

#include <cassert>
#include <cstdio>
#include <memory>

namespace {
using namespace canvas;

// Inject only the failure boundary; all resource operations remain D3D12.
class FailingPreview final : public render::SkiaSurfaceProvider {
 public:
  explicit FailingPreview(HWND window) : actual(window, false) {}
  minimal_runtime_host::WindowsD3D12SurfaceProvider actual;
  bool rejectResize = false;
  render::RenderTargetInfo describe() const noexcept override { return actual.describe(); }
  render::SkiaSurfaceAcquireResult acquire() noexcept override { return actual.acquire(); }
  void release() noexcept override { actual.release(); }
  render::BackendSubmissionResult lose() noexcept override { return actual.lose(); }
  render::BackendSubmissionResult present() noexcept override { return actual.present(); }
  render::BackendSubmissionResult resize(std::uint32_t w, std::uint32_t h) noexcept override {
    if (rejectResize) {
      (void)actual.lose();
      return render::BackendSubmissionResult::rejected("injected second-role failure");
    }
    return actual.resize(w, h);
  }
  render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out) noexcept override {
    return actual.readbackRgba(out);
  }
  std::uint64_t generation() const noexcept override { return actual.generation(); }
  render::BackendSubmissionResult advanceGeneration() noexcept override { return actual.advanceGeneration(); }
  render::BackendSubmissionResult activateBinding() noexcept override { return actual.activateBinding(); }
  void deactivateBinding() noexcept override { actual.deactivateBinding(); }
  void setOverlayVisible(bool visible) noexcept override { actual.setOverlayVisible(visible); }
  std::uint64_t readbackCount() const noexcept override { return actual.readbackCount(); }
  std::uint64_t cpuCopyCount() const noexcept override { return actual.cpuCopyCount(); }
  std::uint64_t presentCount() const noexcept override { return actual.presentCount(); }
};
}

int main() {
  WNDCLASSW klass{};
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpfnWndProc = DefWindowProcW;
  klass.lpszClassName = L"AxiomS2SurfaceBindingTest";
  assert(RegisterClassW(&klass) != 0U);
  HWND window = CreateWindowExW(0U, klass.lpszClassName, L"S2 native resource test",
      WS_OVERLAPPEDWINDOW, 0, 0, 400, 400, nullptr, nullptr, klass.hInstance, nullptr);
  assert(window != nullptr);
  {
    runtime::RuntimeComposition composition;
    assert(composition.open());
    auto canonical = std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(window, true);
    auto* canonicalRaw = canonical.get();
    auto preview = std::make_unique<FailingPreview>(window);
    auto* previewRaw = preview.get();
    if (canonical->resize(256U, 256U).code != render::BackendSubmissionCode::kAccepted ||
        preview->resize(256U, 256U).code != render::BackendSubmissionCode::kAccepted) {
      std::fprintf(stderr, "BLOCKED_ENVIRONMENT: native D3D12 preparation unavailable\n");
      return 77;
    }
    assert(composition.registerSurfaceProvider("d3d12-canonical", std::move(canonical)));
    assert(composition.registerPreviewSurfaceProvider("d3d12-preview", std::move(preview)));
    assert(composition.bindSurface({128.0F, 128.0F, 256U, 256U, 2.0F, 1.0F}));
    assert(canonicalRaw->generation() == 1U && previewRaw->generation() == 1U);
    assert(composition.resizeSurface({128.0F, 128.0F, 256U, 256U, 2.0F, 1.25F}));
    assert(canonicalRaw->generation() == 1U && previewRaw->generation() == 1U);
    const auto before = composition.surfaceSnapshot();
    previewRaw->rejectResize = true;
    assert(!composition.resizeSurface(320U, 240U));
    assert(canonicalRaw->describe().metrics.physicalWidth == 320U);
    assert(canonicalRaw->generation() > before.surfaceGeneration);
    assert(composition.state() == runtime::RuntimeCompositionState::kLost);
    assert(!composition.canonicalProviderAvailable() && !composition.previewProviderAvailable());
    assert(canonicalRaw->acquire().code == render::SkiaSurfaceAcquireCode::kLost);
    assert(previewRaw->acquire().code == render::SkiaSurfaceAcquireCode::kLost);
    previewRaw->rejectResize = false;
    assert(composition.rebindSurface({320.0F, 240.0F, 320U, 240U, 1.0F, 1.0F}));
    assert(composition.state() == runtime::RuntimeCompositionState::kReady);
    auto replacement = std::make_unique<minimal_runtime_host::WindowsD3D12SurfaceProvider>(window, true);
    assert(replacement->resize(320U, 240U).code == render::BackendSubmissionCode::kAccepted);
    assert(composition.registerSurfaceProvider("d3d12-replacement", std::move(replacement)));
    assert(composition.resizeSurface(320U, 240U));
    runtime::RuntimeFrameOpportunity frame{};
    assert(composition.serviceFrameOpportunity(true, frame) && frame.canonicalFrame);
    assert(composition.close());
  }
  assert(DestroyWindow(window));
  std::puts("S2 native D3D12 pair: prepared adopt, metrics-only reuse, destructive failure, Lost/recovery, replacement and frame passed");
}

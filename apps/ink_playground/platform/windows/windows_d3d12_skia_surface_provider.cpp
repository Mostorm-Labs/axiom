#include "windows_d3d12_skia_surface_provider.hpp"
#include "windows_input_diagnostics.hpp"

#if defined(_WIN32) && defined(CANVAS_RENDER_HAS_SKIA)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <d3d12.h>
#include <dcomp.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendContext.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSurface.h"
#include "include/gpu/ganesh/d3d/GrD3DDirectContext.h"
#include "include/gpu/ganesh/d3d/GrD3DTypes.h"
#include <array>
#include <limits>

using Microsoft::WRL::ComPtr;

namespace canvas::ink_playground {
namespace {
constexpr wchar_t kOverlayClass[] = L"AxiomD3D12PreviewOverlay";
LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<const CREATESTRUCTW*>(lParam);
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    return TRUE;
  }
  if (message == WM_NCHITTEST) return HTTRANSPARENT;
  if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE ||
      message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED || message == WM_TOUCH ||
      message == WM_LBUTTONDOWN || message == WM_MOUSEMOVE || message == WM_LBUTTONUP) {
    const auto owner = reinterpret_cast<HWND>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (auto* diagnostic = windows_input::diagnosticStream(owner)) {
      windows_input::logInputMessage(*diagnostic, "overlay", window, message, wParam);
    }
  }
  if (message == WM_POINTERDOWN || message == WM_POINTERUPDATE ||
      message == WM_POINTERUP || message == WM_POINTERCAPTURECHANGED) {
    const auto owner = reinterpret_cast<HWND>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (owner != nullptr && IsWindow(owner)) {
      // WM_POINTER carries a pointer id, not a client-coordinate payload.
      // The owner resolves screen position from GetPointerInfo. Windows owns
      // capture per pointer; Win32 SetCapture is mouse-global and would merge
      // otherwise independent touch contacts.
      SendMessageW(owner, message, wParam, lParam);
      return 0;
    }
  }
  if (message == WM_TOUCH) {
    const auto owner = reinterpret_cast<HWND>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (owner != nullptr && IsWindow(owner)) {
      SendMessageW(owner, message, wParam, lParam);
      return 0;
    }
  }
  if (message == WM_LBUTTONDOWN || message == WM_MOUSEMOVE ||
      message == WM_LBUTTONUP) {
    const auto owner = reinterpret_cast<HWND>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (owner != nullptr && IsWindow(owner)) {
      POINT point{static_cast<short>(LOWORD(lParam)),
                  static_cast<short>(HIWORD(lParam))};
      MapWindowPoints(window, owner, &point, 1U);
      const auto ownerPosition = MAKELPARAM(point.x, point.y);
      SendMessageW(owner, message, wParam, ownerPosition);
      return 0;
    }
  }
  return DefWindowProcW(window, message, wParam, lParam);
}
bool registerOverlayClass(HINSTANCE instance) noexcept {
  WNDCLASSW klass{};
  klass.hInstance = instance;
  klass.lpfnWndProc = overlayProc;
  klass.lpszClassName = kOverlayClass;
  return RegisterClassW(&klass) != 0 || GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
}
}  // namespace

struct WindowsD3D12SkiaSurfaceProvider::Impl final {
  ComPtr<IDXGIAdapter1> adapter;
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<IDXGISwapChain3> swapChain;
  ComPtr<IDCompositionDevice> composition;
  ComPtr<IDCompositionTarget> target;
  ComPtr<IDCompositionVisual> visual;
  sk_sp<GrDirectContext> context;
  std::array<ComPtr<ID3D12Resource>, 2> buffers;
  std::array<sk_sp<SkSurface>, 2> surfaces;
};

WindowsD3D12SkiaSurfaceProvider::WindowsD3D12SkiaSurfaceProvider(
    HWND owner, bool attachToOwner) noexcept
    : impl_(std::make_unique<Impl>()), owner_(owner), attachToOwner_(attachToOwner) {}
WindowsD3D12SkiaSurfaceProvider::~WindowsD3D12SkiaSurfaceProvider() {
  destroyGpuSurface();
  destroyOverlay();
}
canvas::render::RenderTargetInfo WindowsD3D12SkiaSurfaceProvider::describe() const noexcept {
  return {"gpu-d3d12", canvas::render::RenderTargetKind::kGpuWindow,
          canvas::render::RenderTargetBackend::kD3D12,
          canvas::render::RenderTargetFormat::kBgra8888,
          {static_cast<float>(width_), static_cast<float>(height_), width_, height_, 1.0F, 1.0F},
          {true, false, false, false, true, true}};
}
canvas::render::SkiaSurfaceAcquireResult WindowsD3D12SkiaSurfaceProvider::acquire() noexcept {
  if (lost_ || !impl_ || !impl_->swapChain || !impl_->context) {
    return canvas::render::SkiaSurfaceAcquireResult::rejected(
        canvas::render::SkiaSurfaceAcquireCode::kLost, "Windows D3D12 surface unavailable");
  }
  const auto index = impl_->swapChain->GetCurrentBackBufferIndex();
  if (index >= impl_->surfaces.size() || !impl_->surfaces[index]) {
    return canvas::render::SkiaSurfaceAcquireResult::rejected(
        canvas::render::SkiaSurfaceAcquireCode::kUnavailable, "D3D12 back buffer unavailable");
  }
  return canvas::render::SkiaSurfaceAcquireResult::acquired(
      {impl_->surfaces[index].get(), generation_});
}
void WindowsD3D12SkiaSurfaceProvider::release() noexcept {}
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::lose() noexcept {
  lost_ = true;
  setOverlayVisible(false);
  return canvas::render::BackendSubmissionResult::accepted();
}
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::present() noexcept {
  if (lost_ || !impl_ || !impl_->swapChain || !impl_->context) {
    return canvas::render::BackendSubmissionResult::rejected("D3D12 surface unavailable");
  }
  const auto index = impl_->swapChain->GetCurrentBackBufferIndex();
  if (index >= impl_->surfaces.size() || !impl_->surfaces[index]) {
    return canvas::render::BackendSubmissionResult::rejected("D3D12 back buffer unavailable");
  }
  impl_->context->flushAndSubmit(impl_->surfaces[index].get(), GrSyncCpu::kNo);
  // Preview is transient and must not block input processing on a display
  // refresh interval. Canonical presentation has its own qualified handoff;
  // this surface only publishes the latest preview frame.
  if (FAILED(impl_->swapChain->Present(0, 0))) {
    lost_ = true;
    return canvas::render::BackendSubmissionResult::rejected("D3D12 present failed");
  }
  ++presents_;
  return canvas::render::BackendSubmissionResult::accepted();
}
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::resize(
    std::uint32_t width, std::uint32_t height) noexcept {
  if (width == 0U || height == 0U || width > 16384U || height > 16384U) {
    return canvas::render::BackendSubmissionResult::rejected("invalid D3D12 surface size");
  }
  if (width_ == width && height_ == height && impl_->swapChain && !lost_) {
    repositionOverlay();
    return canvas::render::BackendSubmissionResult::accepted();
  }
  width_ = width;
  height_ = height;
  destroyGpuSurface();
  if (!createGpuSurface() || !ensureOverlay()) {
    return canvas::render::BackendSubmissionResult::rejected("D3D12 surface creation failed");
  }
  ++generation_;
  if (generation_ == 0U) generation_ = 1U;
  lost_ = false;
  repositionOverlay();
  return canvas::render::BackendSubmissionResult::accepted();
}
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::readbackRgba(
    std::span<std::uint8_t>) noexcept {
  return canvas::render::BackendSubmissionResult::rejected("GPU provider has no readback");
}
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::advanceGeneration() noexcept {
  if (generation_ == std::numeric_limits<std::uint64_t>::max()) {
    return canvas::render::BackendSubmissionResult::rejected("D3D12 generation exhausted");
  }
  ++generation_;
  lost_ = false;
  return canvas::render::BackendSubmissionResult::accepted();
}
void WindowsD3D12SkiaSurfaceProvider::setOverlayVisible(bool visible) noexcept {
  visible_ = visible;
  if (attachToOwner_) {
    // The canonical provider is attached directly to the owner HWND.  It is
    // always part of the document presentation; only the transient Arc
    // provider is allowed to hide/show its popup surface.
    return;
  }
  if (!overlay_) return;
  if (impl_ && impl_->visual && impl_->composition) {
    // Detach the swap-chain content before hiding the popup.  Hiding only the
    // HWND is not sufficient for every Windows composition path: the last
    // visual can remain latched until another swap-chain present.
    if (visible && impl_->swapChain) {
      (void)impl_->visual->SetContent(impl_->swapChain.Get());
    } else {
      (void)impl_->visual->SetContent(nullptr);
    }
  }
  // Keep visibility changes on the stable HWND path. DirectComposition
  // visual-property animation is not required for lifecycle hiding and can
  // invalidate the visual on older Windows composition implementations.
  if (visible) {
    ShowWindow(overlay_, SW_SHOWNOACTIVATE);
  } else {
    ShowWindow(overlay_, SW_HIDE);
  }
  // ShowWindow changes the HWND visibility, while the pixels are owned by a
  // DirectComposition visual. Commit the visibility transition immediately so
  // the last Arc frame cannot remain in the compositor until the next input
  // or present tick.
  if (impl_ && impl_->composition) {
    (void)impl_->composition->Commit();
  }
}
bool WindowsD3D12SkiaSurfaceProvider::createGpuSurface() noexcept {
  ComPtr<IDXGIFactory6> factory;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&factory)))) return false;
  for (UINT i = 0;; ++i) {
    ComPtr<IDXGIAdapter1> candidate;
    if (factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                                             IID_PPV_ARGS(&candidate)) == DXGI_ERROR_NOT_FOUND) break;
    DXGI_ADAPTER_DESC1 desc{};
    candidate->GetDesc1(&desc);
    if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0) continue;
    if (SUCCEEDED(D3D12CreateDevice(candidate.Get(), D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&impl_->device)))) {
      impl_->adapter = candidate;
      break;
    }
  }
  if (!impl_->adapter || !impl_->device) return false;
  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(impl_->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&impl_->queue)))) return false;
  GrD3DBackendContext backend;
  backend.fAdapter.retain(impl_->adapter.Get());
  backend.fDevice.retain(impl_->device.Get());
  backend.fQueue.retain(impl_->queue.Get());
  impl_->context = GrDirectContexts::MakeD3D(backend);
  if (!impl_->context) return false;
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = width_; desc.Height = height_; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.BufferCount = 2; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.SampleDesc.Count = 1;
  desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
  ComPtr<IDXGISwapChain1> swap;
  if (FAILED(factory->CreateSwapChainForComposition(impl_->queue.Get(), &desc, nullptr, &swap)) ||
      FAILED(swap.As(&impl_->swapChain))) return false;
  for (UINT i = 0; i < impl_->buffers.size(); ++i) {
    if (FAILED(impl_->swapChain->GetBuffer(i, IID_PPV_ARGS(&impl_->buffers[i])))) return false;
    GrD3DTextureResourceInfo resourceInfo(
        impl_->buffers[i].Get(), nullptr, D3D12_RESOURCE_STATE_PRESENT,
        DXGI_FORMAT_B8G8R8A8_UNORM, 1U, 1U,
        DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN);
    const auto target = GrBackendRenderTargets::MakeD3D(
        static_cast<int>(width_), static_cast<int>(height_), resourceInfo);
    impl_->surfaces[i] = SkSurfaces::WrapBackendRenderTarget(
        impl_->context.get(), target, kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!impl_->surfaces[i]) return false;
  }
  return true;
}
bool WindowsD3D12SkiaSurfaceProvider::ensureOverlay() noexcept {
  if (attachToOwner_) {
    if (!owner_ || !impl_->swapChain) return false;
    if (!impl_->composition &&
        FAILED(DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&impl_->composition)))) {
      return false;
    }
    if (!impl_->target) {
      if (FAILED(impl_->composition->CreateTargetForHwnd(owner_, TRUE,
                                                          &impl_->target)) ||
          FAILED(impl_->composition->CreateVisual(&impl_->visual)) ||
          FAILED(impl_->visual->SetContent(impl_->swapChain.Get())) ||
          FAILED(impl_->target->SetRoot(impl_->visual.Get()))) {
        return false;
      }
    } else {
      (void)impl_->visual->SetContent(impl_->swapChain.Get());
    }
    // The owner HWND retains its normal GDI toolbar; the canonical surface
    // occupies the canvas band below it.
    (void)impl_->visual->SetOffsetX(0.0f);
    (void)impl_->visual->SetOffsetY(56.0f);
    return SUCCEEDED(impl_->composition->Commit());
  }
  if (overlay_ && impl_->composition) {
    if (impl_->visual && impl_->swapChain) {
      (void)impl_->visual->SetContent(impl_->swapChain.Get());
      (void)impl_->composition->Commit();
    }
    return true;
  }
  if (!owner_) return false;
  const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE));
  if (!registerOverlayClass(instance)) return false;
  overlay_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT,
                             kOverlayClass, L"Axiom D3D12 Arc Preview", WS_POPUP,
                             0, 0, static_cast<int>(width_), static_cast<int>(height_),
                             owner_, nullptr, instance, owner_);
  if (!overlay_) return false;
  // The preview popup is display-only. Do not register it as a touch or
  // pointer target: a visible transparent popup can otherwise become the
  // Windows input target when a second finger lands while the first stroke
  // is active. The owner HWND is the sole native input target; the forwarding
  // handlers below remain only for messages Windows may route to the popup
  // during a compositor transition.
  if (FAILED(DCompositionCreateDevice(nullptr, IID_PPV_ARGS(&impl_->composition)))) return false;
  if (FAILED(impl_->composition->CreateTargetForHwnd(overlay_, TRUE, &impl_->target)) ||
      FAILED(impl_->composition->CreateVisual(&impl_->visual)) ||
      FAILED(impl_->visual->SetContent(impl_->swapChain.Get())) ||
      FAILED(impl_->target->SetRoot(impl_->visual.Get())) ||
      FAILED(impl_->composition->Commit())) return false;
  return true;
}
void WindowsD3D12SkiaSurfaceProvider::destroyGpuSurface() noexcept {
  if (!impl_) return;
  if (impl_->visual) impl_->visual->SetContent(nullptr);
  for (auto& surface : impl_->surfaces) surface.reset();
  for (auto& buffer : impl_->buffers) buffer.Reset();
  impl_->swapChain.Reset();
  if (impl_->context) { impl_->context->abandonContext(); impl_->context.reset(); }
  impl_->queue.Reset(); impl_->device.Reset(); impl_->adapter.Reset();
}
void WindowsD3D12SkiaSurfaceProvider::repositionOverlay() noexcept {
  if (attachToOwner_) {
    if (impl_ && impl_->composition && impl_->visual) {
      (void)impl_->visual->SetOffsetX(static_cast<float>(overlayOffsetX_));
      (void)impl_->visual->SetOffsetY(static_cast<float>(56 + overlayOffsetY_));
      (void)impl_->composition->Commit();
    }
    return;
  }
  if (!owner_ || !overlay_) return;
  RECT client{};
  if (!GetClientRect(owner_, &client)) return;
  POINT origin{client.left + overlayOffsetX_, client.top + overlayOffsetY_};
  ClientToScreen(owner_, &origin);
  SetWindowPos(overlay_, HWND_TOP, origin.x, origin.y, static_cast<int>(width_),
               static_cast<int>(height_), SWP_NOACTIVATE | SWP_SHOWWINDOW);
}
void WindowsD3D12SkiaSurfaceProvider::destroyOverlay() noexcept {
  if (overlay_) DestroyWindow(overlay_);
  overlay_ = nullptr;
  if (impl_) { impl_->visual.Reset(); impl_->target.Reset(); impl_->composition.Reset(); }
}
}  // namespace canvas::ink_playground
#else
namespace canvas::ink_playground {
WindowsD3D12SkiaSurfaceProvider::~WindowsD3D12SkiaSurfaceProvider() = default;
canvas::render::RenderTargetInfo WindowsD3D12SkiaSurfaceProvider::describe() const noexcept { return {}; }
canvas::render::SkiaSurfaceAcquireResult WindowsD3D12SkiaSurfaceProvider::acquire() noexcept { return {}; }
void WindowsD3D12SkiaSurfaceProvider::release() noexcept {}
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::lose() noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::present() noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::resize(std::uint32_t, std::uint32_t) noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::readbackRgba(std::span<std::uint8_t>) noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsD3D12SkiaSurfaceProvider::advanceGeneration() noexcept { return {}; }
void WindowsD3D12SkiaSurfaceProvider::setOverlayVisible(bool visible) noexcept { visible_ = visible; }
}  // namespace canvas::ink_playground
#endif

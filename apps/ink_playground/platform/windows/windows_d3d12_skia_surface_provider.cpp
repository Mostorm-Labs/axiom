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
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendContext.h"
#include "include/gpu/ganesh/d3d/GrD3DBackendSurface.h"
#include "include/gpu/ganesh/d3d/GrD3DDirectContext.h"
#include "include/gpu/ganesh/d3d/GrD3DTypes.h"
#include <array>
#include <cstdio>
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
  ComPtr<ID3D12Fence> fence;
  HANDLE fenceEvent = nullptr;
  std::uint64_t fenceValue = 0;
  std::array<std::uint64_t, 2> bufferFenceValues{};
  std::uint32_t currentBufferIndex = 0;
};

WindowsD3D12SkiaSurfaceProvider::WindowsD3D12SkiaSurfaceProvider(
    HWND owner, bool attachToOwner) noexcept
    : impl_(std::make_unique<Impl>()), owner_(owner), attachToOwner_(attachToOwner) {}
WindowsD3D12SkiaSurfaceProvider::~WindowsD3D12SkiaSurfaceProvider() {
  destroyGpuSurface();
  destroyOverlay();
  if (impl_ && impl_->fenceEvent != nullptr) {
    CloseHandle(impl_->fenceEvent);
    impl_->fenceEvent = nullptr;
  }
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
  // Flip-model composition can hand back a buffer that is still being
  // consumed by DWM.  Wait for that specific buffer before Skia records into
  // it; a single device-wide fence is insufficient when the swap chain is
  // resized or maximized while frames are in flight.
  const auto ready = impl_->bufferFenceValues[index];
  if (ready != 0U && impl_->fence && impl_->fenceEvent &&
      impl_->fence->GetCompletedValue() < ready) {
    if (SUCCEEDED(impl_->fence->SetEventOnCompletion(ready, impl_->fenceEvent))) {
      (void)WaitForSingleObject(impl_->fenceEvent, 5000);
    }
  }
  impl_->currentBufferIndex = index;
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
  impl_->context->flush(impl_->surfaces[index].get(),
                        SkSurfaces::BackendSurfaceAccess::kPresent,
                        GrFlushInfo{});
  impl_->context->submit(GrSyncCpu::kNo);
  // Preview is transient and must not block input processing on a display
  // refresh interval. Canonical presentation has its own qualified handoff;
  // this surface only publishes the latest preview frame.
  const auto presentHr = impl_->swapChain->Present(0, 0);
  if (FAILED(presentHr)) {
    std::fprintf(stderr, "[d3d12] present failed owner=%d gen=%llu index=%u hr=0x%08lx\\n",
                 attachToOwner_ ? 1 : 0,
                 static_cast<unsigned long long>(generation_), index,
                 static_cast<unsigned long>(presentHr));
    lost_ = true;
    return canvas::render::BackendSubmissionResult::rejected("D3D12 present failed");
  }
  const auto fence = ++impl_->fenceValue;
  if (impl_->queue && impl_->fence && SUCCEEDED(impl_->queue->Signal(impl_->fence.Get(), fence))) {
    impl_->bufferFenceValues[index] = fence;
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
  std::fprintf(stderr, "[d3d12] resize owner=%d old=%ux%u new=%ux%u gen=%llu\\n",
               attachToOwner_ ? 1 : 0, width_, height_, width, height,
               static_cast<unsigned long long>(generation_));
  if (!attachToOwner_) {
    visible_ = false;
    if (overlay_) ShowWindow(overlay_, SW_HIDE);
  }
  // Keep the D3D device, queue and Skia context alive across a normal
  // maximize/restore. Recreating the whole device while the owner visual is
  // attached leaves a short interval where DComp can sample an uninitialised
  // backbuffer, which is the black frame seen during the next stroke.
  if (impl_->swapChain && impl_->context &&
      resizeGpuSurfaceBuffers(width, height)) {
    width_ = width;
    height_ = height;
    ++generation_;
    if (generation_ == 0U) generation_ = 1U;
    lost_ = false;
    repositionOverlay();
    std::fprintf(stderr, "[d3d12] resize-buffers ready owner=%d gen=%llu\\n",
                 attachToOwner_ ? 1 : 0,
                 static_cast<unsigned long long>(generation_));
    return canvas::render::BackendSubmissionResult::accepted();
  }
  width_ = width;
  height_ = height;
  destroyGpuSurface();
  if (!createGpuSurface() || !ensureOverlay()) {
    std::fprintf(stderr, "[d3d12] resize failed owner=%d new=%ux%u\\n",
                 attachToOwner_ ? 1 : 0, width, height);
    return canvas::render::BackendSubmissionResult::rejected("D3D12 surface creation failed");
  }
  ++generation_;
  if (generation_ == 0U) generation_ = 1U;
  lost_ = false;
  repositionOverlay();
  std::fprintf(stderr, "[d3d12] resize ready owner=%d gen=%llu\\n", attachToOwner_ ? 1 : 0,
               static_cast<unsigned long long>(generation_));
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
    // Keep the visual content stable across pointer-up/pointer-down cycles.
    // Detaching and reattaching a D3D12 swap chain while the compositor is
    // presenting can fault in D3D12Core on some Windows 10 drivers. Visibility
    // is controlled by the popup HWND below; the next present updates content.
    if (visible && impl_->swapChain) (void)impl_->visual->SetContent(impl_->swapChain.Get());
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
  // Avoid a second composition commit on the pointer-up path. The HWND
  // visibility change is sufficient and is less prone to racing Present(0,0).
}
void WindowsD3D12SkiaSurfaceProvider::suspendForResize() noexcept {
  if (!attachToOwner_ || !impl_ || !impl_->visual || !impl_->composition) return;
  (void)impl_->visual->SetContent(nullptr);
  (void)impl_->composition->Commit();
}
void WindowsD3D12SkiaSurfaceProvider::reposition() noexcept { repositionOverlay(); }
bool WindowsD3D12SkiaSurfaceProvider::createGpuSurface() noexcept {
  ComPtr<IDXGIFactory6> factory;
  const auto factoryHr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
  if (FAILED(factoryHr)) {
    std::fprintf(stderr, "[d3d12] CreateDXGIFactory2 failed hr=0x%08lx\\n",
                 static_cast<unsigned long>(factoryHr));
    return false;
  }
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
  if (!impl_->adapter || !impl_->device) {
    std::fprintf(stderr, "[d3d12] no hardware adapter\\n");
    return false;
  }
  D3D12_COMMAND_QUEUE_DESC queueDesc{};
  queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(impl_->device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&impl_->queue)))) return false;
  if (FAILED(impl_->device->CreateFence(0, D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(&impl_->fence)))) return false;
  if (impl_->fenceEvent == nullptr) {
    impl_->fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (impl_->fenceEvent == nullptr) return false;
  }
  impl_->bufferFenceValues.fill(0U);
  GrD3DBackendContext backend;
  backend.fAdapter.retain(impl_->adapter.Get());
  backend.fDevice.retain(impl_->device.Get());
  backend.fQueue.retain(impl_->queue.Get());
  impl_->context = GrDirectContexts::MakeD3D(backend);
  if (!impl_->context) return false;
  DXGI_SWAP_CHAIN_DESC1 desc{};
  desc.Width = width_; desc.Height = height_; desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.BufferCount = 2; desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  // CreateSwapChainForComposition is a DirectComposition swap-chain API.  Its
  // contract requires the sequential flip model; FLIP_DISCARD is supported by
  // Skia's HWND viewer path, but is not a valid/stable choice for a composition
  // visual on the Windows drivers used by the playground.  In particular,
  // maximize/restore can expose an uninitialised (black) frame when the
  // composition visual is rebound to a discard swap chain.  Keep the
  // composition path on the documented sequential model and rely on the
  // per-backbuffer fence below to prevent reuse while DWM is consuming it.
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; desc.SampleDesc.Count = 1;
  desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
  ComPtr<IDXGISwapChain1> swap;
  const auto swapHr = factory->CreateSwapChainForComposition(impl_->queue.Get(), &desc,
                                                              nullptr, &swap);
  if (FAILED(swapHr)) {
    std::fprintf(stderr, "[d3d12] CreateSwapChainForComposition failed hr=0x%08lx\\n",
                 static_cast<unsigned long>(swapHr));
    return false;
  }
  const auto swap3Hr = swap.As(&impl_->swapChain);
  if (FAILED(swap3Hr)) {
    std::fprintf(stderr, "[d3d12] swapchain QI failed hr=0x%08lx\\n",
                 static_cast<unsigned long>(swap3Hr));
    return false;
  }
  for (UINT i = 0; i < impl_->buffers.size(); ++i) {
    const auto bufferHr = impl_->swapChain->GetBuffer(i, IID_PPV_ARGS(&impl_->buffers[i]));
    if (FAILED(bufferHr)) {
      std::fprintf(stderr, "[d3d12] GetBuffer index=%u failed hr=0x%08lx\\n", i,
                   static_cast<unsigned long>(bufferHr));
      return false;
    }
    GrD3DTextureResourceInfo resourceInfo(
        impl_->buffers[i].Get(), nullptr, D3D12_RESOURCE_STATE_PRESENT,
        DXGI_FORMAT_B8G8R8A8_UNORM, 1U, 1U,
        DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN);
    const auto target = GrBackendRenderTargets::MakeD3D(
        static_cast<int>(width_), static_cast<int>(height_), resourceInfo);
    impl_->surfaces[i] = SkSurfaces::WrapBackendRenderTarget(
        impl_->context.get(), target, kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!impl_->surfaces[i]) {
      std::fprintf(stderr, "[d3d12] WrapBackendRenderTarget index=%u failed\\n", i);
      return false;
    }
  }
  return true;
}
bool WindowsD3D12SkiaSurfaceProvider::resizeGpuSurfaceBuffers(
    std::uint32_t width, std::uint32_t height) noexcept {
  if (!impl_ || !impl_->swapChain || !impl_->context || !impl_->queue ||
      !impl_->fence || !impl_->fenceEvent) return false;
  impl_->context->flushAndSubmit();
  const auto targetFence = ++impl_->fenceValue;
  if (FAILED(impl_->queue->Signal(impl_->fence.Get(), targetFence))) return false;
  if (impl_->fence->GetCompletedValue() < targetFence) {
    if (FAILED(impl_->fence->SetEventOnCompletion(targetFence, impl_->fenceEvent)) ||
        WaitForSingleObject(impl_->fenceEvent, 5000) != WAIT_OBJECT_0) {
      return false;
    }
  }
  if (impl_->visual && impl_->composition) {
    (void)impl_->visual->SetContent(nullptr);
    (void)impl_->composition->Commit();
    (void)impl_->composition->WaitForCommitCompletion();
  }
  for (auto& surface : impl_->surfaces) surface.reset();
  for (auto& buffer : impl_->buffers) buffer.Reset();
  const auto resizeHr = impl_->swapChain->ResizeBuffers(
      static_cast<UINT>(impl_->buffers.size()), width, height,
      DXGI_FORMAT_B8G8R8A8_UNORM, 0);
  if (FAILED(resizeHr)) {
    std::fprintf(stderr, "[d3d12] ResizeBuffers failed owner=%d %ux%u hr=0x%08lx\\n",
                 attachToOwner_ ? 1 : 0, width, height,
                 static_cast<unsigned long>(resizeHr));
    return false;
  }
  impl_->bufferFenceValues.fill(0U);
  impl_->currentBufferIndex = impl_->swapChain->GetCurrentBackBufferIndex();
  for (UINT i = 0; i < impl_->buffers.size(); ++i) {
    const auto bufferHr = impl_->swapChain->GetBuffer(i, IID_PPV_ARGS(&impl_->buffers[i]));
    if (FAILED(bufferHr)) return false;
    GrD3DTextureResourceInfo resourceInfo(
        impl_->buffers[i].Get(), nullptr, D3D12_RESOURCE_STATE_PRESENT,
        DXGI_FORMAT_B8G8R8A8_UNORM, 1U, 1U,
        DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN);
    const auto target = GrBackendRenderTargets::MakeD3D(
        static_cast<int>(width), static_cast<int>(height), resourceInfo);
    impl_->surfaces[i] = SkSurfaces::WrapBackendRenderTarget(
        impl_->context.get(), target, kTopLeft_GrSurfaceOrigin,
        kBGRA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!impl_->surfaces[i]) return false;
  }
  if (impl_->visual && impl_->composition) {
    if (FAILED(impl_->visual->SetContent(impl_->swapChain.Get()))) return false;
    return SUCCEEDED(impl_->composition->Commit());
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
  // This HWND is only a DirectComposition target.  Do not combine a
  // composition swap-chain with WS_EX_LAYERED: USER32's layered-window
  // redirection surface treats transparent premultiplied pixels as an
  // opaque black backing during maximize/activation transitions.  The
  // composition visual already supplies alpha; NOREDIRECTIONBITMAP keeps
  // the popup out of that extra GDI redirection path.
  overlay_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW |
                             WS_EX_NOREDIRECTIONBITMAP | WS_EX_TRANSPARENT,
                             kOverlayClass, L"Axiom D3D12 Arc Preview", WS_POPUP,
                             0, 0, static_cast<int>(width_), static_cast<int>(height_),
                             owner_, nullptr, instance, owner_);
  if (!overlay_) return false;
  // HTTRANSPARENT does not exclude a popup from Windows touch hit testing;
  // the owner remains the sole native input target and the overlay forwards
  // only messages Windows may route to it during a compositor transition.
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
  // Skia submits work to the same direct queue used by the composition
  // swap-chain. Wait for that queue before releasing wrapped backbuffers.
  if (impl_->context && impl_->queue && impl_->fence && impl_->fenceEvent) {
    impl_->context->flushAndSubmit();
    const auto target = ++impl_->fenceValue;
    if (SUCCEEDED(impl_->queue->Signal(impl_->fence.Get(), target)) &&
        impl_->fence->GetCompletedValue() < target &&
        SUCCEEDED(impl_->fence->SetEventOnCompletion(target, impl_->fenceEvent))) {
      (void)WaitForSingleObject(impl_->fenceEvent, 5000);
    }
  }
  if (impl_->visual && impl_->composition) {
    (void)impl_->visual->SetContent(nullptr);
    (void)impl_->composition->Commit();
    (void)impl_->composition->WaitForCommitCompletion();
  }
  if (impl_->visual) impl_->visual->SetContent(nullptr);
  for (auto& surface : impl_->surfaces) surface.reset();
  for (auto& buffer : impl_->buffers) buffer.Reset();
  impl_->swapChain.Reset();
  // The D3D12 command queue is fenced above.  Keep the context alive until
  // all wrapped surfaces and resources have been released; abandoning it
  // before the final COM releases can leave a queued clear/present referring
  // to a destroyed resource and produces the maximize black frame.
  if (impl_->context) { impl_->context->flushAndSubmit(); impl_->context.reset(); }
  impl_->queue.Reset(); impl_->device.Reset(); impl_->adapter.Reset();
  impl_->fence.Reset();
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
  // Moving the owner must not detach, hide, or recommit the DComp visual.
  // Keeping the existing swap-chain content latched avoids exposing the
  // driver's undefined/black backbuffer during a normal window drag.
  const UINT visibility = visible_ ? SWP_SHOWWINDOW : SWP_HIDEWINDOW;
  SetWindowPos(overlay_, HWND_TOP, origin.x, origin.y,
               static_cast<int>(width_), static_cast<int>(height_),
               SWP_NOACTIVATE | visibility);
}
void WindowsD3D12SkiaSurfaceProvider::destroyOverlay() noexcept {
  if (overlay_) DestroyWindow(overlay_);
  overlay_ = nullptr;
  if (impl_) { impl_->visual.Reset(); impl_->target.Reset(); impl_->composition.Reset(); }
}
}  // namespace canvas::ink_playground
#else
namespace canvas::ink_playground {
struct WindowsD3D12SkiaSurfaceProvider::Impl {};
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

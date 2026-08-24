#include <axiom/verification/windows_harness_adapter.hpp>

#ifdef _WIN32
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#endif

namespace axiom::verification::platform {

namespace {
#ifdef _WIN32
using Microsoft::WRL::ComPtr;

struct Win32SurfaceState {
  HINSTANCE instance{};
  HWND hwnd{};
  ATOM window_class{};
  ComPtr<IDXGIFactory6> factory;
  ComPtr<IDXGIAdapter1> warp_adapter;
  ComPtr<ID3D12Device> device;
  ComPtr<ID3D12CommandQueue> queue;
  ComPtr<IDXGISwapChain3> swap_chain;
};

LRESULT CALLBACK hidden_window_proc(HWND hwnd, UINT message, WPARAM wparam,
                                    LPARAM lparam) {
  return DefWindowProcW(hwnd, message, wparam, lparam);
}

bool create_native_surface(Win32SurfaceState& state) {
  state.instance = GetModuleHandleW(nullptr);
  constexpr wchar_t kClassName[] = L"AxiomG011WindowsNativeHost";
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = hidden_window_proc;
  window_class.hInstance = state.instance;
  window_class.lpszClassName = kClassName;
  state.window_class = RegisterClassW(&window_class);
  if (state.window_class == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;

  state.hwnd = CreateWindowExW(0, kClassName, L"Axiom GT-G0-11", WS_OVERLAPPEDWINDOW,
                               0, 0, 64, 64, nullptr, nullptr, state.instance, nullptr);
  if (state.hwnd == nullptr) return false;
  if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&state.factory)))) return false;
  if (FAILED(state.factory->EnumWarpAdapter(IID_PPV_ARGS(&state.warp_adapter)))) return false;
  if (FAILED(D3D12CreateDevice(state.warp_adapter.Get(), D3D_FEATURE_LEVEL_11_0,
                               IID_PPV_ARGS(&state.device)))) return false;

  D3D12_COMMAND_QUEUE_DESC queue_desc{};
  queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  if (FAILED(state.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&state.queue)))) return false;

  DXGI_SWAP_CHAIN_DESC1 swap_chain_desc{};
  swap_chain_desc.Width = 64;
  swap_chain_desc.Height = 64;
  swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  swap_chain_desc.BufferCount = 2;
  swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  swap_chain_desc.SampleDesc.Count = 1;
  ComPtr<IDXGISwapChain1> swap_chain;
  if (FAILED(state.factory->CreateSwapChainForHwnd(state.queue.Get(), state.hwnd,
                                                    &swap_chain_desc, nullptr, nullptr,
                                                    &swap_chain))) return false;
  if (FAILED(swap_chain.As(&state.swap_chain))) return false;
  state.factory->MakeWindowAssociation(state.hwnd, DXGI_MWA_NO_ALT_ENTER);
  return true;
}

void destroy_native_surface(Win32SurfaceState& state) {
  state.swap_chain.Reset();
  state.queue.Reset();
  state.device.Reset();
  state.warp_adapter.Reset();
  state.factory.Reset();
  if (state.hwnd != nullptr) DestroyWindow(state.hwnd);
  state.hwnd = nullptr;
}
#endif
}

WindowsHarnessAdapter::WindowsHarnessAdapter() {
#ifdef _WIN32
  auto* state = new Win32SurfaceState();
  if (create_native_surface(*state)) {
    native_state_ = state;
    native_surface_ready_ = true;
  } else {
    destroy_native_surface(*state);
    delete state;
  }
#endif
}

WindowsHarnessAdapter::~WindowsHarnessAdapter() {
#ifdef _WIN32
  if (native_state_ != nullptr) {
    auto* state = static_cast<Win32SurfaceState*>(native_state_);
    destroy_native_surface(*state);
    delete state;
    native_state_ = nullptr;
  }
#endif
}

void WindowsHarnessAdapter::create_canvas() {
  if (state_ != State::New) return;
  state_ = State::Created;
}

void WindowsHarnessAdapter::attach_host() {
  if (state_ == State::Created) state_ = State::HostAttached;
}

void WindowsHarnessAdapter::attach_document() {
  if (state_ == State::HostAttached) {
    state_ = State::DocumentAttached;
    document_attached_ = true;
  }
}

void WindowsHarnessAdapter::update_metrics(const WindowsMetrics&) {
  if (state_ != State::Destroyed) ++metrics_generation_;
}

void WindowsHarnessAdapter::surface_lost(std::uint64_t generation) {
  if (state_ == State::Destroyed || generation != surface_generation_) return;
  surface_lost_ = true;
}

void WindowsHarnessAdapter::rebind_surface() {
  if (state_ == State::Destroyed || !surface_lost_) return;
  surface_lost_ = false;
  ++surface_generation_;
}

void WindowsHarnessAdapter::device_lost(std::uint64_t generation) {
  if (state_ == State::Destroyed || generation != device_generation_) return;
  device_lost_ = true;
}

void WindowsHarnessAdapter::device_recover() {
  if (state_ == State::Destroyed || !device_lost_) return;
  device_lost_ = false;
  ++device_generation_;
}

void WindowsHarnessAdapter::hold_present_completions() { present_held_ = true; }

void WindowsHarnessAdapter::destroy_canvas() {
  if (state_ == State::Destroyed) return;
  state_ = State::Destroyed;
  document_attached_ = false;
}

std::uint64_t WindowsHarnessAdapter::semantic_revision() const noexcept {
  return semantic_revision_;
}
std::uint64_t WindowsHarnessAdapter::metrics_generation() const noexcept {
  return metrics_generation_;
}
std::uint64_t WindowsHarnessAdapter::surface_generation() const noexcept {
  return surface_generation_;
}
std::uint64_t WindowsHarnessAdapter::device_generation() const noexcept {
  return device_generation_;
}
bool WindowsHarnessAdapter::document_attached() const noexcept {
  return document_attached_;
}
TargetOwnership WindowsHarnessAdapter::target_ownership() const {
  return {"AXIOM", "ARC", true};
}
bool WindowsHarnessAdapter::native_surface_ready() const noexcept {
  return native_surface_ready_;
}
PublishDisposition WindowsHarnessAdapter::release_held_after_destroy() {
  if (!present_held_ || state_ != State::Destroyed) return PublishDisposition::Cancelled;
  present_held_ = false;
  return PublishDisposition::DroppedStaleScope;
}
std::size_t WindowsHarnessAdapter::events_after_destroy() const noexcept {
  return events_after_destroy_;
}

}  // namespace axiom::verification::platform

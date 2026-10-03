#define NOMINMAX
#include "canvas/debug_ui/windows_host.hpp"

#if defined(_WIN32)
#include "imgui.h"
#include "backends/imgui_impl_win32.h"
#include "include/core/SkColorType.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"

#include <algorithm>
#include <chrono>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg,
                                                              WPARAM wParam, LPARAM lParam);

namespace canvas::debug_ui {
namespace {
constexpr wchar_t kDebugUiClass[] = L"AxiomDebugUiPanel";
constexpr int kPanelWidth = 410;
constexpr int kPanelHeight = 560;

bool isInputMessage(UINT message) noexcept {
  switch (message) {
    case WM_MOUSEMOVE:
    case WM_NCMOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_CANCELMODE:
    case WM_CAPTURECHANGED:
    case WM_KILLFOCUS:
      return true;
    default:
      return false;
  }
}
}

struct WindowsDebugUiHost::Impl final {
  std::vector<std::uint8_t> pixels;
  sk_sp<SkSurface> surface;
  sk_sp<SkImage> fontTexture;
  int width = 0;
  int height = 0;
  std::chrono::steady_clock::time_point lastFrame{};
  bool rendering = false;
};

WindowsDebugUiHost::WindowsDebugUiHost() = default;
WindowsDebugUiHost::~WindowsDebugUiHost() { shutdown(); }

bool WindowsDebugUiHost::initialize(HWND w) {
  if (initialized_ || !w) return initialized_;
  window_ = w;
  HINSTANCE instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(w, GWLP_HINSTANCE));
  WNDCLASSW klass{};
  klass.hInstance = instance;
  klass.lpfnWndProc = overlayWindowProc;
  klass.lpszClassName = kDebugUiClass;
  klass.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
  if (RegisterClassW(&klass) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return false;
  overlay_ = CreateWindowExW(WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW, kDebugUiClass,
      L"Axiom ImGui Debug UI", WS_POPUP | WS_CLIPCHILDREN, 0, 0, kPanelWidth,
      kPanelHeight, w, nullptr, instance, this);
  if (!overlay_) return false;
  IMGUI_CHECKVERSION();
  context_ = ImGui::CreateContext();
  if (!context_) {
    DestroyWindow(overlay_);
    overlay_ = nullptr;
    return false;
  }
  ImGui::SetCurrentContext(context_);
  ImGui::StyleColorsDark();
  if (!ImGui_ImplWin32_Init(overlay_)) {
    ImGui::DestroyContext(context_);
    context_ = nullptr;
    DestroyWindow(overlay_);
    overlay_ = nullptr;
    return false;
  }
  impl_ = std::make_unique<Impl>();
  initialized_ = true;
  visible_ = false;
  ShowWindow(overlay_, SW_HIDE);
  syncOverlay();
  return ensureSurface();
}

void WindowsDebugUiHost::shutdown() noexcept {
  releaseInputCapture();
  if (context_) {
    ImGui::SetCurrentContext(context_);
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext(context_);
  }
  context_ = nullptr;
  impl_.reset();
  if (overlay_) DestroyWindow(overlay_);
  overlay_ = nullptr;
  initialized_ = false;
  visible_ = false;
  placementValid_ = false;
  placementShown_ = false;
  window_ = nullptr;
}

bool WindowsDebugUiHost::ensureSurface() noexcept {
  if (!impl_) return false;
  if (impl_->surface && impl_->fontTexture && impl_->width == kPanelWidth &&
      impl_->height == kPanelHeight) return true;
  ImGui::SetCurrentContext(context_);
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsAlpha8(&pixels, &width, &height);
  if (!pixels || width <= 0 || height <= 0) return false;
  const SkImageInfo fontInfo = SkImageInfo::MakeA8(width, height);
  const SkPixmap fontPixmap(fontInfo, pixels, fontInfo.minRowBytes());
  impl_->fontTexture = SkImages::RasterFromPixmapCopy(fontPixmap);
  const SkImageInfo surfaceInfo = SkImageInfo::Make(kPanelWidth, kPanelHeight,
      kBGRA_8888_SkColorType, kPremul_SkAlphaType);
  impl_->pixels.assign(static_cast<size_t>(kPanelWidth) * kPanelHeight * 4U, 0U);
  impl_->surface = SkSurfaces::WrapPixels(surfaceInfo, impl_->pixels.data(),
                                           static_cast<size_t>(kPanelWidth) * 4U);
  impl_->width = kPanelWidth;
  impl_->height = kPanelHeight;
  return impl_->surface != nullptr && impl_->fontTexture != nullptr;
}

void WindowsDebugUiHost::toggle() noexcept {
  visible_ = !visible_;
  placementValid_ = false;
  syncOverlay();
  if (visible_) renderFrame();
  if (overlay_) InvalidateRect(overlay_, nullptr, FALSE);
}

bool WindowsDebugUiHost::handleMessage(HWND source, UINT message, WPARAM wParam,
                                       LPARAM lParam) {
  if (!visible_ || !context_ || source != overlay_) return false;
  if (!isInputMessage(message)) return false;
  const bool down = message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN ||
                    message == WM_MBUTTONDOWN || message == WM_XBUTTONDOWN;
  const bool up = message == WM_LBUTTONUP || message == WM_RBUTTONUP ||
                  message == WM_MBUTTONUP || message == WM_XBUTTONUP;
  const bool cancel = message == WM_CANCELMODE || message == WM_CAPTURECHANGED ||
                      message == WM_KILLFOCUS;
  if (down) {
    activeInputSequence_ = DebugInputSequence{0U, ++inputSequence_};
    (void)inputCapture_->begin(*activeInputSequence_, DebugInputOwner::kDebug);
  }
  ImGui::SetCurrentContext(context_);
  const LRESULT handled = ImGui_ImplWin32_WndProcHandler(source, message, wParam, lParam);
  // Win32 backend events are queued until the next NewFrame. Consume the
  // queue before reading capture state; otherwise WantCaptureMouse describes
  // the previous frame and hover/click handling becomes intermittent.
  if (impl_ != nullptr && !impl_->rendering) renderFrame();
  // Hover is useful even when ImGui does not request capture. Always repaint
  // the overlay after consuming an input event, then decide ownership from
  // the capture state produced by the new frame.
  InvalidateRect(overlay_, nullptr, FALSE);
  const ImGuiIO& io = ImGui::GetIO();
  if (handled != 0 || io.WantCaptureMouse || io.WantCaptureKeyboard) {
    if (up || cancel) releaseInputCapture();
    return true;
  }
  if (down) releaseInputCapture();
  if (up || cancel) releaseInputCapture();
  return false;
}

void WindowsDebugUiHost::releaseInputCapture() noexcept {
  if (activeInputSequence_.has_value()) {
    (void)inputCapture_->terminal(*activeInputSequence_);
    activeInputSequence_.reset();
  }
}

void WindowsDebugUiHost::syncOverlay() noexcept {
  if (!overlay_ || !window_) return;
  RECT client{};
  if (!GetClientRect(window_, &client)) return;
  if (IsIconic(window_)) { ShowWindow(overlay_, SW_HIDE); return; }
  POINT origin{18, 68};
  if (!ClientToScreen(window_, &origin)) return;
  const int height = (std::min)(kPanelHeight, (std::max)(static_cast<int>(client.bottom) - 70, 1));
  const bool shown = visible_;
  if (placementValid_ && placementX_ == origin.x && placementY_ == origin.y &&
      placementWidth_ == kPanelWidth && placementHeight_ == height && placementShown_ == shown) return;
  const UINT flags = SWP_NOACTIVATE | (shown ? SWP_SHOWWINDOW : SWP_HIDEWINDOW);
  SetWindowPos(overlay_, shown ? HWND_TOP : nullptr, origin.x, origin.y,
               kPanelWidth, height, flags | (shown ? 0U : SWP_NOZORDER));
  placementX_ = origin.x; placementY_ = origin.y; placementWidth_ = kPanelWidth;
  placementHeight_ = height; placementShown_ = shown; placementValid_ = true;
}

void WindowsDebugUiHost::reposition() noexcept { placementValid_ = false; syncOverlay(); }

void WindowsDebugUiHost::raise() noexcept {
  if (!overlay_ || !window_ || !visible_ || IsIconic(window_)) return;
  SetWindowPos(overlay_, HWND_TOP, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void WindowsDebugUiHost::renderFrame() noexcept {
  if (!visible_ || !context_ || !ensureSurface() || impl_ == nullptr || impl_->rendering) return;
  impl_->rendering = true;
  ImGui::SetCurrentContext(context_);
  RECT client{};
  if (!GetClientRect(overlay_, &client)) {
    impl_->rendering = false;
    return;
  }
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(static_cast<float>(client.right - client.left),
                          static_cast<float>(client.bottom - client.top));
  const auto now = std::chrono::steady_clock::now();
  const float elapsed = impl_->lastFrame.time_since_epoch().count() == 0
      ? (1.0f / 60.0f)
      : std::chrono::duration<float>(now - impl_->lastFrame).count();
  io.DeltaTime = (std::max)(elapsed, 1.0f / 1000.0f);
  impl_->lastFrame = now;
  ImGui_ImplWin32_NewFrame();
  ImGui::NewFrame();
  buildImGuiPanels(snapshot_, selectedTool_, runtime_, axiomDebug_, platform_);
  ImGui::Render();
  ImGuiSkiaRenderer renderer;
  (void)renderer.render(ImGui::GetDrawData(), impl_->surface.get(), impl_->fontTexture.get());
  impl_->rendering = false;
}

void WindowsDebugUiHost::frame(const DebugSnapshot& s) {
  snapshot_ = s;
  if (snapshot_.selectedTool != 0U) selectedTool_ = static_cast<int>(snapshot_.selectedTool);
  if (!initialized_) return;
  syncOverlay();
  if (visible_) { renderFrame(); raise(); InvalidateRect(overlay_, nullptr, FALSE); }
}

void WindowsDebugUiHost::paintOverlay(HDC dc) const {
  if (!impl_ || impl_->pixels.empty()) return;
  RECT client{}; GetClientRect(overlay_, &client);
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = impl_->width; info.bmiHeader.biHeight = -impl_->height;
  info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32; info.bmiHeader.biCompression = BI_RGB;
  StretchDIBits(dc, 0, 0, client.right, client.bottom, 0, 0, impl_->width, impl_->height,
                impl_->pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
}

void WindowsDebugUiHost::paint(HDC) const {}

LRESULT CALLBACK WindowsDebugUiHost::overlayWindowProc(HWND w, UINT message,
                                                       WPARAM wParam, LPARAM lParam) {
  auto* self = reinterpret_cast<WindowsDebugUiHost*>(GetWindowLongPtrW(w, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
    self = static_cast<WindowsDebugUiHost*>(create->lpCreateParams);
    SetWindowLongPtrW(w, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
  }
  if (!self) return DefWindowProcW(w, message, wParam, lParam);
  if (self->handleMessage(w, message, wParam, lParam)) return 0;
  if (message == WM_PAINT) {
    PAINTSTRUCT paint{}; HDC dc = BeginPaint(w, &paint);
    self->paintOverlay(dc); EndPaint(w, &paint); return 0;
  }
  if (message == WM_ERASEBKGND) return 1;
  return DefWindowProcW(w, message, wParam, lParam);
}

}  // namespace canvas::debug_ui
#endif

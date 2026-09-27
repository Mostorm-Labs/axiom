#include "windows_skia_preview_surface_provider.hpp"

#if defined(_WIN32)

#include <algorithm>
#include <cstring>
#include <limits>

namespace canvas::ink_playground {
namespace {
constexpr wchar_t kOverlayClass[] = L"AxiomSkiaPreviewOverlay";

LRESULT CALLBACK overlayProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_NCHITTEST) return HTTRANSPARENT;
  if (message == WM_MOUSEACTIVATE) return MA_NOACTIVATE;
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

canvas::render::RenderTargetInfo WindowsSkiaPreviewSurfaceProvider::describe() const noexcept {
  return {"windows-skia-arc-preview", canvas::render::RenderTargetKind::kExternal,
          canvas::render::RenderTargetBackend::kRaster,
          canvas::render::RenderTargetFormat::kRgba8888,
          {static_cast<float>(width_), static_cast<float>(height_), width_, height_, 1.0F, 1.0F},
          {false, false, false, true, true, false}};
}

WindowsSkiaPreviewSurfaceProvider::~WindowsSkiaPreviewSurfaceProvider() {
  destroyOverlay();
}

canvas::render::SkiaSurfaceAcquireResult
WindowsSkiaPreviewSurfaceProvider::acquire() noexcept {
  if (lost_) {
    return canvas::render::SkiaSurfaceAcquireResult::rejected(
        canvas::render::SkiaSurfaceAcquireCode::kLost, "Windows preview surface is lost");
  }
  return raster_.acquire();
}

void WindowsSkiaPreviewSurfaceProvider::release() noexcept { raster_.release(); }

canvas::render::BackendSubmissionResult
WindowsSkiaPreviewSurfaceProvider::lose() noexcept {
  lost_ = true;
  visible_ = false;
  setOverlayVisible(false);
  return raster_.lose();
}

canvas::render::BackendSubmissionResult
WindowsSkiaPreviewSurfaceProvider::resize(std::uint32_t width,
                                          std::uint32_t height) noexcept {
  if (width == 0U || height == 0U) {
    return canvas::render::BackendSubmissionResult::rejected("invalid Windows preview size");
  }
  const auto result = raster_.resize(width, height);
  if (result.code != canvas::render::BackendSubmissionCode::kAccepted) return result;
  width_ = width;
  height_ = height;
  rgba_.resize(static_cast<std::size_t>(width) * height * 4U);
  bgra_.resize(rgba_.size());
  lost_ = false;
  if (!ensureOverlay()) {
    return canvas::render::BackendSubmissionResult::rejected(
        "failed to create Windows preview overlay");
  }
  if (bitmap_ != nullptr) {
    SelectObject(memoryDc_, oldBitmap_);
    DeleteObject(bitmap_);
    bitmap_ = nullptr;
    bitmapBits_ = nullptr;
    oldBitmap_ = nullptr;
  }
  BITMAPINFO bitmapInfo{};
  bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bitmapInfo.bmiHeader.biWidth = static_cast<LONG>(width_);
  bitmapInfo.bmiHeader.biHeight = -static_cast<LONG>(height_);
  bitmapInfo.bmiHeader.biPlanes = 1;
  bitmapInfo.bmiHeader.biBitCount = 32;
  bitmapInfo.bmiHeader.biCompression = BI_RGB;
  bitmap_ = CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits_, nullptr, 0);
  if (bitmap_ == nullptr || bitmapBits_ == nullptr) {
    return canvas::render::BackendSubmissionResult::rejected(
        "failed to create Windows preview bitmap");
  }
  oldBitmap_ = SelectObject(memoryDc_, bitmap_);
  RECT client{};
  if (!GetClientRect(owner_, &client)) {
    return canvas::render::BackendSubmissionResult::rejected(
        "failed to query Windows preview owner");
  }
  POINT origin{client.left, client.top};
  ClientToScreen(owner_, &origin);
  SetWindowPos(overlay_, HWND_TOP, origin.x, origin.y,
               static_cast<int>(width), static_cast<int>(height),
               SWP_NOACTIVATE | SWP_SHOWWINDOW);
  return canvas::render::BackendSubmissionResult::accepted();
}

canvas::render::BackendSubmissionResult
WindowsSkiaPreviewSurfaceProvider::present() noexcept {
  const auto presented = raster_.present();
  if (presented.code != canvas::render::BackendSubmissionCode::kAccepted) return presented;
  if (!ensureOverlay() || bitmapBits_ == nullptr || rgba_.empty()) {
    return canvas::render::BackendSubmissionResult::rejected(
        "Windows preview overlay is unavailable");
  }
  if (raster_.readbackRgba(rgba_).code != canvas::render::BackendSubmissionCode::kAccepted) {
    return canvas::render::BackendSubmissionResult::rejected(
        "Windows preview readback failed");
  }
  for (std::size_t i = 0; i + 3U < rgba_.size(); i += 4U) {
    bgra_[i + 0U] = rgba_[i + 2U];
    bgra_[i + 1U] = rgba_[i + 1U];
    bgra_[i + 2U] = rgba_[i + 0U];
    bgra_[i + 3U] = rgba_[i + 3U];
  }
  ++pixelCopies_;

  std::memcpy(bitmapBits_, bgra_.data(), bgra_.size());

  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = static_cast<LONG>(width_);
  info.bmiHeader.biHeight = -static_cast<LONG>(height_);
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 32;
  info.bmiHeader.biCompression = BI_RGB;
  POINT origin{};
  RECT client{};
  if (!GetClientRect(owner_, &client)) return canvas::render::BackendSubmissionResult::rejected("owner unavailable");
  origin = {client.left, client.top};
  ClientToScreen(owner_, &origin);
  SIZE size{static_cast<LONG>(width_), static_cast<LONG>(height_)};
  BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
  const auto screen = GetDC(nullptr);
  const auto updated = UpdateLayeredWindow(overlay_, screen, &origin, &size, memoryDc_,
                                           nullptr, 0, &blend, ULW_ALPHA);
  ReleaseDC(nullptr, screen);
  if (!updated) return canvas::render::BackendSubmissionResult::rejected("UpdateLayeredWindow failed");
  return canvas::render::BackendSubmissionResult::accepted();
}

canvas::render::BackendSubmissionResult
WindowsSkiaPreviewSurfaceProvider::readbackRgba(std::span<std::uint8_t> destination) noexcept {
  return raster_.readbackRgba(destination);
}

canvas::render::BackendSubmissionResult
WindowsSkiaPreviewSurfaceProvider::advanceGeneration() noexcept {
  lost_ = false;
  visible_ = false;
  setOverlayVisible(false);
  return raster_.advanceGeneration();
}

void WindowsSkiaPreviewSurfaceProvider::setOverlayVisible(bool visible) noexcept {
  visible_ = visible;
  if (overlay_ != nullptr) ShowWindow(overlay_, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

bool WindowsSkiaPreviewSurfaceProvider::ensureOverlay() noexcept {
  if (overlay_ != nullptr && memoryDc_ != nullptr) return true;
  if (owner_ == nullptr) return false;
  const auto instance = reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(owner_, GWLP_HINSTANCE));
  if (!registerOverlayClass(instance)) return false;
  overlay_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
                             kOverlayClass, L"Axiom ARC Preview", WS_POPUP,
                             0, 0, static_cast<int>(width_), static_cast<int>(height_),
                             owner_, nullptr, instance, nullptr);
  if (overlay_ == nullptr) return false;
  const auto screen = GetDC(nullptr);
  memoryDc_ = CreateCompatibleDC(screen);
  ReleaseDC(nullptr, screen);
  if (memoryDc_ == nullptr) {
    destroyOverlay();
    return false;
  }
  return true;
}

void WindowsSkiaPreviewSurfaceProvider::destroyOverlay() noexcept {
  if (memoryDc_ != nullptr && oldBitmap_ != nullptr) SelectObject(memoryDc_, oldBitmap_);
  oldBitmap_ = nullptr;
  if (memoryDc_ != nullptr) DeleteDC(memoryDc_);
  memoryDc_ = nullptr;
  if (bitmap_ != nullptr) DeleteObject(bitmap_);
  bitmap_ = nullptr;
  if (overlay_ != nullptr) DestroyWindow(overlay_);
  overlay_ = nullptr;
  bitmapBits_ = nullptr;
}

}  // namespace canvas::ink_playground

#else

namespace canvas::ink_playground {
WindowsSkiaPreviewSurfaceProvider::~WindowsSkiaPreviewSurfaceProvider() = default;
canvas::render::RenderTargetInfo WindowsSkiaPreviewSurfaceProvider::describe() const noexcept { return {}; }
canvas::render::SkiaSurfaceAcquireResult WindowsSkiaPreviewSurfaceProvider::acquire() noexcept { return {}; }
void WindowsSkiaPreviewSurfaceProvider::release() noexcept {}
canvas::render::BackendSubmissionResult WindowsSkiaPreviewSurfaceProvider::lose() noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsSkiaPreviewSurfaceProvider::present() noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsSkiaPreviewSurfaceProvider::resize(std::uint32_t, std::uint32_t) noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsSkiaPreviewSurfaceProvider::readbackRgba(std::span<std::uint8_t>) noexcept { return {}; }
canvas::render::BackendSubmissionResult WindowsSkiaPreviewSurfaceProvider::advanceGeneration() noexcept { return {}; }
void WindowsSkiaPreviewSurfaceProvider::setOverlayVisible(bool visible) noexcept { visible_ = visible; }
}  // namespace canvas::ink_playground

#endif

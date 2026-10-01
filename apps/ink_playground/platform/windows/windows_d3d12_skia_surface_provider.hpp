#pragma once

#include "canvas/render/skia_surface_provider.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif

#include <cstdint>
#include <memory>

namespace canvas::ink_playground {

class WindowsD3D12SkiaSurfaceProvider final
    : public canvas::render::SkiaSurfaceProvider {
 public:
#if defined(_WIN32)
  explicit WindowsD3D12SkiaSurfaceProvider(HWND owner,
                                           bool attachToOwner = false) noexcept;
#else
  explicit WindowsD3D12SkiaSurfaceProvider(void*) noexcept {}
#endif
  ~WindowsD3D12SkiaSurfaceProvider() override;
  [[nodiscard]] canvas::render::RenderTargetInfo describe() const noexcept override;
  [[nodiscard]] canvas::render::SkiaSurfaceAcquireResult acquire() noexcept override;
  void release() noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult lose() noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult present() noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult resize(std::uint32_t, std::uint32_t) noexcept override;
  [[nodiscard]] canvas::render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t>) noexcept override;
  [[nodiscard]] std::uint64_t generation() const noexcept override { return generation_; }
  [[nodiscard]] canvas::render::BackendSubmissionResult advanceGeneration() noexcept override;
  [[nodiscard]] std::uint64_t readbackCount() const noexcept override { return 0; }
  [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept override { return 0; }
  [[nodiscard]] std::uint64_t presentCount() const noexcept override { return presents_; }
  void setOverlayVisible(bool) noexcept override;
  void setOverlayOffset(int x, int y) noexcept { overlayOffsetX_ = x; overlayOffsetY_ = y; }
  [[nodiscard]] bool overlayVisible() const noexcept override { return visible_; }

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
#if defined(_WIN32)
  HWND owner_ = nullptr;
  HWND overlay_ = nullptr;
#endif
  bool attachToOwner_ = false;
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  std::uint64_t generation_ = 0;
  std::uint64_t presents_ = 0;
  int overlayOffsetX_ = 0;
  int overlayOffsetY_ = 0;
  bool visible_ = false;
  bool lost_ = false;

#if defined(_WIN32)
  [[nodiscard]] bool ensureOverlay() noexcept;
  [[nodiscard]] bool createGpuSurface() noexcept;
  void destroyGpuSurface() noexcept;
  void destroyOverlay() noexcept;
  void repositionOverlay() noexcept;
#endif
};

using WindowsSkiaPreviewSurfaceProvider = WindowsD3D12SkiaSurfaceProvider;

}  // namespace canvas::ink_playground

#pragma once

#include "canvas/render/skia_surface_provider.hpp"

struct ANativeWindow;

namespace canvas::ink_playground {

// Android production surface realization. Runtime/Skia owns draw semantics;
// this provider owns only ANativeWindow, EGL and the Ganesh framebuffer wrap.
class AndroidEglSkiaSurfaceProvider final : public render::SkiaSurfaceProvider {
 public:
  AndroidEglSkiaSurfaceProvider() = default;
  ~AndroidEglSkiaSurfaceProvider() override;
  AndroidEglSkiaSurfaceProvider(const AndroidEglSkiaSurfaceProvider&) = delete;
  AndroidEglSkiaSurfaceProvider& operator=(const AndroidEglSkiaSurfaceProvider&) = delete;

  [[nodiscard]] bool attach(ANativeWindow* window, std::uint32_t width,
                            std::uint32_t height) noexcept;
  void detach() noexcept;
  [[nodiscard]] render::RenderTargetInfo describe() const noexcept override;
  [[nodiscard]] render::SkiaSurfaceAcquireResult acquire() noexcept override;
  void release() noexcept override;
  [[nodiscard]] render::BackendSubmissionResult lose() noexcept override;
  [[nodiscard]] render::BackendSubmissionResult present() noexcept override;
  [[nodiscard]] render::BackendSubmissionResult resize(std::uint32_t width,
                                                        std::uint32_t height) noexcept override;
  [[nodiscard]] render::BackendSubmissionResult readbackRgba(
      std::span<std::uint8_t> destination) noexcept override;
  [[nodiscard]] std::uint64_t generation() const noexcept override { return generation_; }
  [[nodiscard]] render::BackendSubmissionResult advanceGeneration() noexcept override;
  [[nodiscard]] std::uint64_t readbackCount() const noexcept override { return 0; }
  [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept override { return 0; }
  [[nodiscard]] std::uint64_t presentCount() const noexcept override { return presents_; }

 private:
  struct Impl;
  Impl* impl_ = nullptr;
  std::uint32_t width_ = 0;
  std::uint32_t height_ = 0;
  std::uint64_t generation_ = 0;
  std::uint64_t presents_ = 0;
  bool attached_ = false;
  bool acquired_ = false;
};

}  // namespace canvas::ink_playground

#pragma once

#include <cstdint>

namespace canvas::runtime {

enum class SurfaceRole : std::uint8_t { kCanonicalCanvas, kDebugController };
enum class SurfaceMode : std::uint8_t { kPlatformDefault, kCpuReference, kGpuDefault };
enum class SurfaceControlState : std::uint8_t {
  kQueued,
  kApplied,
  kUnsupported,
  kStaleGeneration,
  kQueueFull,
  kExpired,
  kUnavailable,
  kFailed,
};

struct SurfaceModeRequest final {
  std::uint64_t requestId = 0;
  SurfaceRole target = SurfaceRole::kCanonicalCanvas;
  SurfaceMode mode = SurfaceMode::kPlatformDefault;
  std::uint64_t expectedGeneration = 0;
  std::uint64_t deadlineSequence = 0;
};

struct SurfaceModeReceipt final {
  std::uint64_t requestId = 0;
  SurfaceControlState state = SurfaceControlState::kFailed;
  SurfaceRole target = SurfaceRole::kCanonicalCanvas;
  SurfaceMode mode = SurfaceMode::kPlatformDefault;
  std::uint64_t generation = 0;
};

class PlatformDebugControl {
 public:
  virtual ~PlatformDebugControl() = default;
  // UI-facing admission point. Platform hosts may override this to enqueue
  // work for their SurfaceRebind safe point. The compatibility default keeps
  // small reference owners source-compatible while they migrate.
  [[nodiscard]] virtual SurfaceModeReceipt enqueueSurfaceMode(
      const SurfaceModeRequest& request) noexcept {
    return requestSurfaceMode(request);
  }
  // Owner safe-point application hook. It is intentionally separate from UI
  // admission so a WndProc/ImGui callback never performs a heavy rebind.
  virtual void processPendingSurfaceModes() noexcept {}
  [[nodiscard]] virtual SurfaceModeReceipt requestSurfaceMode(
      const SurfaceModeRequest& request) noexcept = 0;
};

}  // namespace canvas::runtime

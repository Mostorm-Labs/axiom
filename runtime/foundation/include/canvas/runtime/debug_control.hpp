#pragma once

#include <cstdint>

namespace canvas::runtime {

enum class AxiomDebugCommandKind : std::uint8_t {
  kSetOverlayFlags,
  kForceFullRedraw,
  kForceSceneRecompile,
  kEvictTileCache,
  kEvictRasterCache,
  kPauseBackgroundRaster,
  kSetRuntimeMemoryBudget,
  kResetRollingMetrics,
};

enum class AxiomDebugCommandState : std::uint8_t {
  kQueued,
  kApplied,
  kRejected,
  kFailed,
  kStaleGeneration,
  kExpired,
  kDestroyed,
  kQueueFull,
  kUnsupported,
};

struct AxiomDebugCommand final {
  std::uint64_t requestId = 0;
  AxiomDebugCommandKind kind = AxiomDebugCommandKind::kSetOverlayFlags;
  std::uint64_t expectedRuntimeGeneration = 0;
  std::uint64_t expectedDocumentGeneration = 0;
  std::uint64_t deadlineSequence = 0;
  std::uint64_t value = 0;
};

struct AxiomDebugCommandReceipt final {
  std::uint64_t requestId = 0;
  AxiomDebugCommandState state = AxiomDebugCommandState::kRejected;
  std::uint64_t appliedFrameId = 0;
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
};

class AxiomDebugControl {
 public:
  virtual ~AxiomDebugControl() = default;
  // Admission and owner-safe-point application are separate by design. A UI
  // callback must never synchronously mutate a Runtime owner.
  [[nodiscard]] virtual AxiomDebugCommandReceipt enqueue(
      const AxiomDebugCommand& command) noexcept = 0;
  [[nodiscard]] virtual AxiomDebugCommandReceipt receipt(
      std::uint64_t requestId) const noexcept = 0;
};

}  // namespace canvas::runtime

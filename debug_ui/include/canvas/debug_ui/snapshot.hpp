#pragma once

#include <array>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include "canvas/runtime/surface_debug_control.hpp"

namespace canvas::debug_ui {

struct DebugSnapshotStamp final {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
    std::uint64_t snapshotSequence = 0;
    std::uint64_t monotonicTimeNs = 0;
    std::uint64_t frameId = 0;
    std::uint64_t runtimeGeneration = 0;
    std::uint64_t documentGeneration = 0;
    std::uint64_t viewGeneration = 0;
    std::uint64_t surfaceGeneration = 0;
    friend bool operator==(const DebugSnapshotStamp&, const DebugSnapshotStamp&) = default;
};

enum class Capability : std::uint8_t {
    kInput = 0,
    kCanonicalSurface,
    kArcPreviewSurface,
    kSurfaceMode,
    kInspection,
    kTelemetry,
    kTrace,
    kGpuTiming,
    kCount,
};

enum class CapabilityState : std::uint8_t { kUnavailable, kAvailable, kDegraded };

struct DebugSnapshot final {
    DebugSnapshotStamp stamp{};
    std::array<CapabilityState, static_cast<std::size_t>(Capability::kCount)> capabilities{};
    std::uint64_t canonicalSurfaceGeneration = 0;
    std::uint64_t previewSurfaceGeneration = 0;
    std::uint64_t canonicalRevision = 0;
    std::uint64_t previewRevision = 0;
    std::uint32_t activePointerCount = 0;
    std::uint64_t inputBatchCount = 0;
    std::uint64_t handoffCount = 0;
    std::uint64_t presentCount = 0;
    std::uint64_t surfaceLostCount = 0;
    double sampleHz = 0.0;
    double frameMs = 0.0;
    double queueAgeMs = 0.0;
    bool surfaceAvailable = false;
    std::uint32_t droppedCommands = 0;
    bool arcPresenterActive = false;
    bool traceEnabled = false;
    std::uint32_t selectedTool = 0;
    canvas::runtime::SurfaceMode canonicalSurfaceMode =
        canvas::runtime::SurfaceMode::kPlatformDefault;
    std::uint64_t surfaceControlRequestId = 0;
    canvas::runtime::SurfaceControlState surfaceControlState =
        canvas::runtime::SurfaceControlState::kApplied;
    std::uint64_t surfaceControlGeneration = 0;

    [[nodiscard]] CapabilityState capability(Capability value) const noexcept {
        return capabilities[static_cast<std::size_t>(value)];
    }
};

class MutexCopySnapshotChannel final {
  public:
    MutexCopySnapshotChannel();
    void publish(DebugSnapshot snapshot);
    [[nodiscard]] DebugSnapshot read() const;
    [[nodiscard]] DebugSnapshotStamp stamp() const;

  private:
    mutable std::mutex mutex_;
    DebugSnapshot snapshot_{};
};

}  // namespace canvas::debug_ui

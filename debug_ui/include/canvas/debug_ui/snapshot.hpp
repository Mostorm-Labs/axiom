#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace canvas::debug_ui {

struct DebugSnapshotStamp final {
    std::uint64_t generation = 0;
    std::uint64_t sequence = 0;
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
    std::uint32_t droppedCommands = 0;
    bool arcPresenterActive = false;
    bool traceEnabled = false;

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
    std::shared_ptr<const DebugSnapshot> snapshot_;
};

}  // namespace canvas::debug_ui

#pragma once

#include "canvas/render/frame_state.hpp"

#include <cstdint>
#include <optional>

namespace canvas::render {

struct SurfaceSnapshot final {
    ViewId viewId{};
    SurfaceGeneration surfaceGeneration{};
    MetricsGeneration metricsGeneration{};
    SurfaceMetrics metrics{};

    bool operator==(const SurfaceSnapshot&) const = default;
};

enum class SurfaceLifecycleDisposition : std::uint8_t {
    kAcquired,
    kReplaced,
    kLost,
    kUnavailable,
    kWrongView,
    kInvalidGeneration,
    kGenerationExhausted,
    kInvalidMetrics,
    kMetricsMismatch,
    kNoGenerationAdvance,
    kStaleGeneration,
};

struct SurfaceAcquireResult final {
    SurfaceLifecycleDisposition disposition = SurfaceLifecycleDisposition::kUnavailable;
    std::optional<SurfaceSnapshot> snapshot;

    bool operator==(const SurfaceAcquireResult&) const = default;
};

class SurfaceLifecycle final {
  public:
    explicit SurfaceLifecycle(SurfaceSnapshot initial) noexcept;

    [[nodiscard]] SurfaceAcquireResult acquire() const;
    [[nodiscard]] SurfaceLifecycleDisposition replace(const SurfaceSnapshot& replacement);
    [[nodiscard]] SurfaceLifecycleDisposition markLost(ViewId viewId) noexcept;

    [[nodiscard]] const SurfaceSnapshot& current() const noexcept { return *_current; }
    [[nodiscard]] bool available() const noexcept { return _available; }

  private:
    std::optional<SurfaceSnapshot> _current;
    bool _available = false;
};

} // namespace canvas::render

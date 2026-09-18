#include "canvas/render/surface_lifecycle.hpp"

#include <cmath>
#include <cstdint>
#include <limits>

namespace canvas::render {
namespace {

bool validMetrics(const SurfaceMetrics& metrics) noexcept {
    return std::isfinite(metrics.logicalWidth) && metrics.logicalWidth > 0.0F &&
           std::isfinite(metrics.logicalHeight) && metrics.logicalHeight > 0.0F &&
           metrics.physicalWidth > 0U && metrics.physicalHeight > 0U &&
           std::isfinite(metrics.devicePixelRatio) && metrics.devicePixelRatio > 0.0F &&
           std::isfinite(metrics.displayScale) && metrics.displayScale > 0.0F;
}

bool exhausted(SurfaceGeneration generation) noexcept {
    return generation.value() == std::numeric_limits<std::uint64_t>::max();
}

bool exhausted(MetricsGeneration generation) noexcept {
    return generation.value() == std::numeric_limits<std::uint64_t>::max();
}

} // namespace

SurfaceLifecycle::SurfaceLifecycle(SurfaceSnapshot initial) noexcept
    : _current(initial),
      _available(_current->viewId.value() != 0U &&
                 _current->surfaceGeneration.value() != 0U &&
                 _current->metricsGeneration.value() != 0U &&
                 !exhausted(_current->surfaceGeneration) &&
                 !exhausted(_current->metricsGeneration) && validMetrics(_current->metrics)) {}

SurfaceAcquireResult SurfaceLifecycle::acquire() const {
    if (!_available) {
        return SurfaceAcquireResult{SurfaceLifecycleDisposition::kUnavailable, std::nullopt};
    }
    return SurfaceAcquireResult{SurfaceLifecycleDisposition::kAcquired, *_current};
}

SurfaceLifecycleDisposition SurfaceLifecycle::replace(const SurfaceSnapshot& replacement) {
    if (replacement.viewId != _current->viewId) {
        return SurfaceLifecycleDisposition::kWrongView;
    }
    if (replacement.surfaceGeneration.value() == 0U ||
        replacement.metricsGeneration.value() == 0U) {
        return SurfaceLifecycleDisposition::kInvalidGeneration;
    }
    if (exhausted(replacement.surfaceGeneration) ||
        exhausted(replacement.metricsGeneration)) {
        return SurfaceLifecycleDisposition::kGenerationExhausted;
    }
    if (!validMetrics(replacement.metrics)) {
        return SurfaceLifecycleDisposition::kInvalidMetrics;
    }

    const std::uint64_t surface = replacement.surfaceGeneration.value();
    const std::uint64_t metrics = replacement.metricsGeneration.value();
    const std::uint64_t currentSurface = _current->surfaceGeneration.value();
    const std::uint64_t currentMetrics = _current->metricsGeneration.value();
    if (surface < currentSurface || metrics < currentMetrics ||
        (!_available && surface <= currentSurface)) {
        return SurfaceLifecycleDisposition::kStaleGeneration;
    }
    if (metrics == currentMetrics && replacement.metrics != _current->metrics) {
        return SurfaceLifecycleDisposition::kMetricsMismatch;
    }
    if (surface == currentSurface && metrics == currentMetrics) {
        return SurfaceLifecycleDisposition::kNoGenerationAdvance;
    }

    _current.emplace(replacement);
    _available = true;
    return SurfaceLifecycleDisposition::kReplaced;
}

SurfaceLifecycleDisposition SurfaceLifecycle::markLost(ViewId viewId) noexcept {
    if (viewId != _current->viewId) {
        return SurfaceLifecycleDisposition::kWrongView;
    }
    _available = false;
    return SurfaceLifecycleDisposition::kLost;
}

} // namespace canvas::render

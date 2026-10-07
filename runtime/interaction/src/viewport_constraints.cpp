#include "canvas/interaction/viewport_constraints.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace canvas::interaction {
namespace {

constexpr float kEpsilonExtent = 1.0e-6F;

float featureValue(const foundation::WorldRect& rect, SnapFeatureKind feature,
                   bool horizontal) noexcept {
  const float minimum = horizontal ? rect.left : rect.top;
  const float maximum = horizontal ? rect.right : rect.bottom;
  if (feature == SnapFeatureKind::kCenter) return (minimum + maximum) * 0.5F;
  return feature == SnapFeatureKind::kMinEdge ? minimum : maximum;
}

bool objectIdLess(const foundation::ObjectId& left,
                  const foundation::ObjectId& right) noexcept {
  return left.bytes < right.bytes;
}

bool candidateLess(const SnapCandidate& left, const SnapCandidate& right) noexcept {
  const float leftDistance = std::abs(left.correction);
  const float rightDistance = std::abs(right.correction);
  if (leftDistance != rightDistance) return leftDistance < rightDistance;
  if (left.targetObjectId != right.targetObjectId) {
    return objectIdLess(left.targetObjectId, right.targetObjectId);
  }
  if (left.targetFeature != right.targetFeature) {
    return static_cast<std::uint8_t>(left.targetFeature) <
           static_cast<std::uint8_t>(right.targetFeature);
  }
  return static_cast<std::uint8_t>(left.sourceFeature) <
         static_cast<std::uint8_t>(right.sourceFeature);
}

}  // namespace

ViewportFitResult computeViewportFit(
    const std::optional<foundation::WorldRect>& target,
    float viewportWidth, float viewportHeight, float cameraRotationRadians,
    float dpr, const ViewportFitPolicy& policy) noexcept {
  (void)dpr;
  if (!target.has_value() || !target->isFiniteAndOrdered() ||
      !std::isfinite(viewportWidth) || !std::isfinite(viewportHeight) ||
      viewportWidth <= 0.0F || viewportHeight <= 0.0F ||
      !std::isfinite(cameraRotationRadians) ||
      !std::isfinite(policy.paddingLogicalPx) ||
      !std::isfinite(policy.minZoom) || !std::isfinite(policy.maxZoom) ||
      policy.minZoom <= 0.0F || policy.maxZoom < policy.minZoom) {
    return {};
  }

  const float width = std::max(target->right - target->left, kEpsilonExtent);
  const float height = std::max(target->bottom - target->top, kEpsilonExtent);
  const float c = std::abs(std::cos(cameraRotationRadians));
  const float s = std::abs(std::sin(cameraRotationRadians));
  const float projectedWidth = std::max(c * width + s * height, kEpsilonExtent);
  const float projectedHeight = std::max(s * width + c * height, kEpsilonExtent);
  const float availableWidth = std::max(viewportWidth - 2.0F * policy.paddingLogicalPx,
                                        kEpsilonExtent);
  const float availableHeight = std::max(viewportHeight - 2.0F * policy.paddingLogicalPx,
                                         kEpsilonExtent);
  const float zoom = std::clamp(std::min(availableWidth / projectedWidth,
                                         availableHeight / projectedHeight),
                                policy.minZoom, policy.maxZoom);
  return {true, zoom, (target->left + target->right) * 0.5F,
          (target->top + target->bottom) * 0.5F};
}

bool SnapResolver::sameCandidate(const SnapCandidate& left,
                                  const SnapCandidate& right) noexcept {
  return left.targetObjectId == right.targetObjectId &&
         left.targetFeature == right.targetFeature &&
         left.sourceFeature == right.sourceFeature;
}

std::optional<SnapCandidate> SnapResolver::resolveAxis(
    float sourceMin, float sourceCenter, float sourceMax,
    std::span<const SnapTarget> targets, bool horizontal, float dpr) noexcept {
  const std::array sourceFeatures{
      std::pair{SnapFeatureKind::kMinEdge, sourceMin},
      std::pair{SnapFeatureKind::kCenter, sourceCenter},
      std::pair{SnapFeatureKind::kMaxEdge, sourceMax}};
  std::optional<SnapCandidate> best;
  const float engage = kEngageThresholdLogicalPx / std::max(dpr, 1.0F);
  for (const auto& target : targets) {
    if (target.selected || target.transformed || !target.viewBounds.isFiniteAndOrdered()) {
      continue;
    }
    for (const auto targetFeature : {SnapFeatureKind::kMinEdge,
                                     SnapFeatureKind::kCenter,
                                     SnapFeatureKind::kMaxEdge}) {
      for (const auto& [sourceFeature, sourceValue] : sourceFeatures) {
        const float correction = featureValue(target.viewBounds, targetFeature, horizontal) -
                                 sourceValue;
        SnapCandidate candidate{target.objectId, targetFeature, sourceFeature, correction};
        if (std::abs(correction) > engage) continue;
        if (!best.has_value() || candidateLess(candidate, *best)) best = candidate;
      }
    }
  }
  return best;
}

SnapResolution SnapResolver::resolve(const SnapSource& source,
                                     std::span<const SnapTarget> targets,
                                     float dpr) noexcept {
  SnapResolution result{};
  if (!source.viewBounds.isFiniteAndOrdered() || !std::isfinite(dpr) || dpr <= 0.0F) {
    reset();
    return result;
  }
  for (const auto& target : targets) {
    if (!target.selected && !target.transformed && target.viewBounds.isFiniteAndOrdered()) {
      ++result.candidatesExamined;
    }
  }

  const auto x = resolveAxis(source.viewBounds.left,
                             (source.viewBounds.left + source.viewBounds.right) * 0.5F,
                             source.viewBounds.right, targets, true, dpr);
  const auto y = resolveAxis(source.viewBounds.top,
                             (source.viewBounds.top + source.viewBounds.bottom) * 0.5F,
                             source.viewBounds.bottom, targets, false, dpr);
  const float release = kReleaseThresholdLogicalPx / std::max(dpr, 1.0F);
  auto currentCorrection = [&](const std::optional<SnapCandidate>& active,
                               bool horizontal) -> std::optional<SnapCandidate> {
    if (!active.has_value()) return std::nullopt;
    const auto target = std::find_if(targets.begin(), targets.end(), [&](const SnapTarget& item) {
      return item.objectId == active->targetObjectId && !item.selected &&
             !item.transformed && item.viewBounds.isFiniteAndOrdered();
    });
    if (target == targets.end()) return std::nullopt;
    const float sourceMin = horizontal ? source.viewBounds.left : source.viewBounds.top;
    const float sourceMax = horizontal ? source.viewBounds.right : source.viewBounds.bottom;
    const float sourceValue = active->sourceFeature == SnapFeatureKind::kMinEdge
        ? sourceMin
        : active->sourceFeature == SnapFeatureKind::kCenter
            ? (sourceMin + sourceMax) * 0.5F : sourceMax;
    SnapCandidate refreshed = *active;
    refreshed.correction = featureValue(target->viewBounds, active->targetFeature,
                                         horizontal) - sourceValue;
    return std::abs(refreshed.correction) <= release
        ? std::optional<SnapCandidate>(refreshed) : std::nullopt;
  };
  auto retainOrEngage = [&currentCorrection](
      std::optional<SnapCandidate>& active, std::optional<SnapCandidate> candidate,
      bool horizontal) {
    if (active.has_value()) {
      const auto refreshed = currentCorrection(active, horizontal);
      if (refreshed.has_value()) {
        active = refreshed;
        return active;
      }
      active.reset();
    }
    active = std::move(candidate);
    return active;
  };
  result.x = retainOrEngage(activeX_, x, true);
  result.y = retainOrEngage(activeY_, y, false);
  return result;
}

void SnapResolver::reset() noexcept {
  activeX_.reset();
  activeY_.reset();
}

}  // namespace canvas::interaction

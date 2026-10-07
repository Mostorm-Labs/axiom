#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/foundation/world_geometry.hpp"

#include <cstdint>
#include <optional>
#include <span>

namespace canvas::interaction {

enum class ViewportFitTargetKind : std::uint8_t { kDocument, kSelection, kObject, kWorldRect };

struct ViewportFitPolicy final {
  float paddingLogicalPx = 32.0F;
  float minZoom = 0.05F;
  float maxZoom = 32.0F;
};

struct ViewportFitResult final {
  bool accepted = false;
  float zoom = 1.0F;
  float centerX = 0.0F;
  float centerY = 0.0F;
};

[[nodiscard]] ViewportFitResult computeViewportFit(
    const std::optional<foundation::WorldRect>& target,
    float viewportWidth, float viewportHeight, float cameraRotationRadians,
    float dpr, const ViewportFitPolicy& policy = {}) noexcept;

enum class SnapFeatureKind : std::uint8_t { kMinEdge = 0, kCenter = 1, kMaxEdge = 2 };

struct SnapTarget final {
  foundation::ObjectId objectId{};
  foundation::WorldRect viewBounds{};
  bool selected = false;
  bool transformed = false;
};

struct SnapSource final { foundation::WorldRect viewBounds{}; };

struct SnapCandidate final {
  foundation::ObjectId targetObjectId{};
  SnapFeatureKind targetFeature = SnapFeatureKind::kMinEdge;
  SnapFeatureKind sourceFeature = SnapFeatureKind::kMinEdge;
  float correction = 0.0F;
};

struct SnapResolution final {
  std::optional<SnapCandidate> x;
  std::optional<SnapCandidate> y;
  std::uint64_t candidatesExamined = 0;
};

class SnapResolver final {
 public:
  static constexpr float kEngageThresholdLogicalPx = 6.0F;
  static constexpr float kReleaseThresholdLogicalPx = 10.0F;

  [[nodiscard]] SnapResolution resolve(const SnapSource& source,
                                       std::span<const SnapTarget> targets,
                                       float dpr = 1.0F) noexcept;
  void reset() noexcept;

 private:
  [[nodiscard]] std::optional<SnapCandidate> resolveAxis(
      float sourceMin, float sourceCenter, float sourceMax,
      std::span<const SnapTarget> targets, bool horizontal, float dpr) noexcept;
  [[nodiscard]] static bool sameCandidate(const SnapCandidate& left,
                                          const SnapCandidate& right) noexcept;
  std::optional<SnapCandidate> activeX_;
  std::optional<SnapCandidate> activeY_;
};

}  // namespace canvas::interaction

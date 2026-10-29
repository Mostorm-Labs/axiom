#include "canvas/interaction/creation_session.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <array>

namespace canvas::interaction {
namespace {
bool finitePoint(foundation::WorldPoint p) noexcept { return std::isfinite(p.x) && std::isfinite(p.y); }
semantic::ObjectId makeId(std::uint64_t seed) noexcept { return foundation::ObjectId::fromUint64(seed == 0 ? 1 : seed); }
semantic::OrderKey makeOrder(std::uint64_t seed) {
  // Creation tools append user-authored objects above existing content. Keep
  // the key deterministic and valid while reserving the high lane for new
  // brush/shape commits.
  return semantic::OrderKey{{0xFEU,
      static_cast<std::uint8_t>(1U + ((seed >> 8U) & 0xFEU)),
      static_cast<std::uint8_t>(1U + (seed & 0xFEU))}};
}
}

bool CreationSession::begin(input::PointerKey pointer, CreationPreset preset, CreationStyle style,
                            foundation::WorldPoint start, foundation::WorldRect viewport,
                            std::uint64_t objectSeed) noexcept {
  if (!pointer.valid() || !finitePoint(start) || !viewport.isFiniteAndOrdered() ||
      viewport.right <= viewport.left || viewport.bottom <= viewport.top || active()) return false;
  pointer_ = pointer; objectSeed_ = objectSeed; preset_ = preset; style_ = std::move(style); viewport_ = viewport;
  preview_ = {}; preview_.active = true; preview_.preset = preset; preview_.start = start; preview_.end = start; preview_.viewport = viewport;
  snappedEndpoint_.reset();
  preview_.record.id = makeId(objectSeed); preview_.record.placement.order_key = makeOrder(objectSeed); preview_.record.properties = style_.properties;
  return rebuild(start);
}

bool CreationSession::rebuild(foundation::WorldPoint end) noexcept {
  if (!active() || !finitePoint(end)) {
    if (active()) clear();
    return false;
  }
  preview_.valid = false;
  preview_.snapped = false;
  preview_.guides.clear();
  snappedEndpoint_.reset();
  const auto start = preview_.start;
  preview_.end = end;
  foundation::WorldPoint snappedEnd = end;
  auto& record = preview_.record;
  record.properties = style_.properties;
  record.placement.order_key = makeOrder(objectSeed_);
  auto rebuildExtent = [&](foundation::WorldPoint finalEnd) {
    const float left = std::min(start.x, finalEnd.x);
    const float top = std::min(start.y, finalEnd.y);
    const float width = std::fabs(finalEnd.x - start.x);
    const float height = std::fabs(finalEnd.y - start.y);
    return std::array<float, 4>{left, top, width, height};
  };
  auto extent = rebuildExtent(snappedEnd);
  const bool shapePreset = preset_ == CreationPreset::kRectangle ||
      preset_ == CreationPreset::kEllipse || preset_ == CreationPreset::kTriangle;
  const bool boxPreset = shapePreset || preset_ == CreationPreset::kSticky;
  if ((boxPreset && (extent[2] <= std::numeric_limits<float>::epsilon() ||
                       extent[3] <= std::numeric_limits<float>::epsilon())) ||
      (!shapePreset && extent[2] <= std::numeric_limits<float>::epsilon() &&
                       extent[3] <= std::numeric_limits<float>::epsilon())) {
    return true;
  }
  if (!snapTargets_.empty()) {
    const foundation::WorldRect source{extent[0] * snapScale_, extent[1] * snapScale_,
                                       (extent[0] + extent[2]) * snapScale_,
                                       (extent[1] + extent[3]) * snapScale_};
    std::vector<interaction::SnapTarget> viewTargets;
    viewTargets.reserve(snapTargets_.size());
    for (const auto& target : snapTargets_) {
      const auto& bounds = target.viewBounds;
      if (!bounds.isFiniteAndOrdered() || bounds.right < viewport_.left ||
          bounds.left > viewport_.right || bounds.bottom < viewport_.top ||
          bounds.top > viewport_.bottom) continue;
      auto copy = target;
      copy.viewBounds = {target.viewBounds.left * snapScale_, target.viewBounds.top * snapScale_,
                         target.viewBounds.right * snapScale_, target.viewBounds.bottom * snapScale_};
      viewTargets.push_back(copy);
    }
    const auto mask = [](bool increasing) -> std::uint8_t {
      return static_cast<std::uint8_t>(2U | (increasing ? 4U : 1U));
    };
    const auto resolved = snapResolver_.resolve(
        interaction::SnapSource{source, mask(end.x >= start.x), mask(end.y >= start.y)},
        viewTargets, 1.0F);
    const auto apply = [&](const std::optional<interaction::SnapCandidate>& candidate,
                           bool horizontal) {
      if (!candidate.has_value()) return;
      float& endpoint = horizontal ? snappedEnd.x : snappedEnd.y;
      const float fixed = horizontal ? start.x : start.y;
      const bool increasing = endpoint >= fixed;
      const bool center = candidate->sourceFeature == interaction::SnapFeatureKind::kCenter;
      const bool endpointFeature = candidate->sourceFeature ==
          (increasing ? interaction::SnapFeatureKind::kMaxEdge : interaction::SnapFeatureKind::kMinEdge);
      if (!center && !endpointFeature) return;
      endpoint += (center ? 2.0F : 1.0F) * candidate->correction / snapScale_;
      preview_.snapped = true;
      // The movable endpoint is the only endpoint that can be attached by a
      // drag.  Preserve the target identity while the resolver supplies the
      // exact edge/center correction; the semantic anchor remains automatic.
      if ((preset_ == CreationPreset::kLine || preset_ == CreationPreset::kArrow) &&
          !snappedEndpoint_.has_value()) {
        snappedEndpoint_ = semantic::AttachedEndpoint{
            candidate->targetObjectId, semantic::AutoPerimeterAnchor{}};
      }
    };
    apply(resolved.x, true);
    apply(resolved.y, false);
    const auto guide = [&](const std::optional<interaction::SnapCandidate>& candidate,
                           const std::optional<interaction::SnapGuide>& value, bool horizontal) {
      if (!candidate || !value) return;
      const auto endpoint = horizontal ? end.x : end.y;
      const auto fixed = horizontal ? start.x : start.y;
      if (candidate->sourceFeature != interaction::SnapFeatureKind::kCenter &&
          candidate->sourceFeature != (endpoint >= fixed ? interaction::SnapFeatureKind::kMaxEdge
                                                         : interaction::SnapFeatureKind::kMinEdge)) return;
      auto result = *value;
      result.position /= snapScale_; result.spanStart /= snapScale_; result.spanEnd /= snapScale_;
      result.spanStart = std::max(result.spanStart, horizontal ? viewport_.top : viewport_.left);
      result.spanEnd = std::min(result.spanEnd, horizontal ? viewport_.bottom : viewport_.right);
      preview_.guides.push_back(result);
    };
    guide(resolved.x, resolved.xGuide, true);
    guide(resolved.y, resolved.yGuide, false);
    extent = rebuildExtent(snappedEnd);
  }
  preview_.end = snappedEnd;
  if (preset_ == CreationPreset::kSticky) {
    record.kind = semantic::ObjectKind::kSticky;
    record.kind_version = 1U;
    record.transform = {};
    record.transform.tx = extent[0];
    record.transform.ty = extent[1];
    const float side = std::max(extent[2], extent[3]);
    record.content = semantic::StickyContent{side, side};
  } else if (shapePreset) {
    record.kind = semantic::ObjectKind::kShape;
    record.kind_version = preset_ == CreationPreset::kTriangle ? 2U : 1U;
    record.transform = {};
    record.transform.tx = extent[0];
    record.transform.ty = extent[1];
    record.content = semantic::ShapeContent{
        preset_ == CreationPreset::kRectangle ? 1U : preset_ == CreationPreset::kEllipse ? 2U : 3U,
        extent[2], extent[3]};
  } else {
    record.kind = semantic::ObjectKind::kConnector;
    record.kind_version = 1U;
    record.transform = {};
    semantic::ConnectorEndpoint endPoint{semantic::FreePointEndpoint{{snappedEnd.x, snappedEnd.y}}};
    if (endAttachment_.has_value()) endPoint.value = *endAttachment_;
    else if (snappedEndpoint_.has_value()) endPoint.value = *snappedEndpoint_;
    record.content = semantic::ConnectorContent{
        {startAttachment_.has_value() ? semantic::ConnectorEndpoint{*startAttachment_}
                                      : semantic::ConnectorEndpoint{semantic::FreePointEndpoint{{start.x, start.y}}}}, endPoint,
        routing_};
    if (preset_ == CreationPreset::kArrow)
      record.properties.entries.push_back({0x201U, semantic::ConnectorDecorationValue::kArrow});
  }
  preview_.valid = true;
  return true;
}
bool CreationSession::move(input::PointerKey pointer, foundation::WorldPoint end) noexcept { return active() && pointer == pointer_ && rebuild(end); }
std::optional<CreationCommit> CreationSession::finish(input::PointerKey pointer) noexcept {
  if (!active() || pointer != pointer_) return std::nullopt;
  if (!preview_.valid) { clear(); return std::nullopt; }
  CreationCommit out{preview_.record, pointer_};
  clear();
  return out;
}
bool CreationSession::cancel(input::PointerKey pointer) noexcept { if (!active() || pointer != pointer_) return false; clear(); return true; }
void CreationSession::clear() noexcept { pointer_ = {}; objectSeed_ = 0; style_ = {}; preview_ = {}; snappedEndpoint_.reset(); startAttachment_.reset(); endAttachment_.reset(); snapResolver_.reset(); }
}  // namespace canvas::interaction

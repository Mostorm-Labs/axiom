#include "canvas/interaction/viewport_constraints.hpp"

#include <cassert>
#include <cmath>
#include <optional>
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

using namespace canvas::interaction;
using canvas::foundation::ObjectId;
using canvas::foundation::WorldRect;

void fitPolicy() {
  const auto rejected = computeViewportFit(std::nullopt, 800.0F, 600.0F, 0.0F, 1.0F);
  assert(!rejected.accepted);
  const auto degenerate = computeViewportFit(WorldRect{10, 20, 10, 20}, 800, 600, 0, 1);
  assert(degenerate.accepted && degenerate.centerX == 10.0F && degenerate.centerY == 20.0F);
  assert(degenerate.zoom == 32.0F);
  const auto normal = computeViewportFit(WorldRect{0, 0, 200, 100}, 800, 600, 0, 1);
  assert(normal.accepted && std::abs(normal.zoom - 3.68F) < 1.0e-4F);
  const auto rotated = computeViewportFit(WorldRect{0, 0, 200, 100}, 800, 600, 0.5F, 1);
  const auto highDpr = computeViewportFit(WorldRect{0, 0, 200, 100}, 800, 600, 0.5F, 2);
  assert(rotated.accepted && highDpr.accepted && std::abs(rotated.zoom - highDpr.zoom) < 1.0e-5F);
  assert(rotated.zoom < normal.zoom);
}

void snapPolicyBProfile() {
  SnapResolver resolver; resolver.setPolicy({3.0F,5.0F});
  const SnapSource source{WorldRect{98,40,118,60}};
  const std::vector<SnapTarget> targets{{ObjectId::fromUint64(9),WorldRect{100,0,200,100}}};
  const auto near=resolver.resolve(source,targets,1.0F); assert(near.x.has_value());
  const auto held=resolver.resolve(SnapSource{WorldRect{104,40,124,60}},targets,1.0F); assert(held.x.has_value());
  const auto released=resolver.resolve(SnapSource{WorldRect{106,40,126,60}},targets,1.0F); assert(!released.x.has_value());
}
void snapPolicy() {
  SnapResolver resolver;
  const SnapSource source{WorldRect{94, 40, 114, 60}};
  const std::vector<SnapTarget> targets{
      {ObjectId::fromUint64(9), WorldRect{100, 0, 200, 100}},
      {ObjectId::fromUint64(3), WorldRect{94, 0, 194, 100}},
      {ObjectId::fromUint64(2), WorldRect{0, 40, 100, 140}, true, false},
      {ObjectId::fromUint64(4), WorldRect{100, 40, 200, 140}, false, true}};
  const auto first = resolver.resolve(source, targets);
  assert(first.candidatesExamined == 2 && first.x.has_value() && first.y.has_value());
  assert(first.xGuide.has_value() && first.yGuide.has_value());
  assert(first.xGuide->axis == SnapGuideAxis::kVertical);
  assert(std::abs(first.xGuide->position - 94.0F) < 1.0e-5F);
  assert(first.xGuide->spanStart <= 0.0F && first.xGuide->spanEnd >= 100.0F);
  assert(first.yGuide->axis == SnapGuideAxis::kHorizontal);
  assert(std::abs(first.yGuide->position - 50.0F) < 1.0e-5F);
  assert(first.x->targetObjectId == ObjectId::fromUint64(3));
  assert(first.x->targetFeature == SnapFeatureKind::kMinEdge);
  assert(first.x->sourceFeature == SnapFeatureKind::kMinEdge);
  const auto retained = resolver.resolve(SnapSource{WorldRect{95, 40, 115, 60}}, targets);
  assert(retained.x.has_value() && retained.x->targetObjectId == ObjectId::fromUint64(3));
  const auto released = resolver.resolve(SnapSource{WorldRect{111, 40, 131, 60}}, targets);
  assert(!released.x.has_value() && !released.xGuide.has_value() && released.y.has_value() &&
         released.yGuide.has_value());
  const auto tie = resolver.resolve(SnapSource{WorldRect{95, 40, 115, 60}}, targets);
  assert(tie.x.has_value() && tie.x->targetObjectId == ObjectId::fromUint64(3));
}

struct ReferenceCandidate final {
  ObjectId target{};
  SnapFeatureKind targetFeature = SnapFeatureKind::kMinEdge;
  SnapFeatureKind sourceFeature = SnapFeatureKind::kMinEdge;
  float correction = 0.0F;
};

float referenceFeature(const WorldRect& bounds, SnapFeatureKind feature, bool horizontal) {
  const float minimum = horizontal ? bounds.left : bounds.top;
  const float maximum = horizontal ? bounds.right : bounds.bottom;
  if (feature == SnapFeatureKind::kCenter) return (minimum + maximum) * 0.5F;
  return feature == SnapFeatureKind::kMinEdge ? minimum : maximum;
}

bool referenceLess(const ReferenceCandidate& left, const ReferenceCandidate& right) {
  if (std::abs(left.correction) != std::abs(right.correction))
    return std::abs(left.correction) < std::abs(right.correction);
  if (left.target != right.target) return left.target < right.target;
  if (left.targetFeature != right.targetFeature)
    return static_cast<unsigned>(left.targetFeature) < static_cast<unsigned>(right.targetFeature);
  return static_cast<unsigned>(left.sourceFeature) < static_cast<unsigned>(right.sourceFeature);
}

std::optional<ReferenceCandidate> referenceResolveAxis(
    const WorldRect& source, const std::vector<SnapTarget>& targets,
    bool horizontal, float dpr, std::uint8_t sourceMask, float engage) {
  const std::array sourceFeatures{
      std::pair{SnapFeatureKind::kMinEdge, horizontal ? source.left : source.top},
      std::pair{SnapFeatureKind::kCenter, horizontal ? (source.left + source.right) * 0.5F
                                                       : (source.top + source.bottom) * 0.5F},
      std::pair{SnapFeatureKind::kMaxEdge, horizontal ? source.right : source.bottom}};
  std::optional<ReferenceCandidate> best;
  const float threshold = engage / std::max(dpr, 1.0F);
  for (const auto& target : targets) {
    if (target.selected || target.transformed || !target.viewBounds.isFiniteAndOrdered()) continue;
    for (const auto targetFeature : {SnapFeatureKind::kMinEdge, SnapFeatureKind::kCenter,
                                     SnapFeatureKind::kMaxEdge}) {
      for (const auto& [sourceFeature, sourceValue] : sourceFeatures) {
        if ((sourceMask & (1U << static_cast<unsigned>(sourceFeature))) == 0U) continue;
        const float correction = referenceFeature(target.viewBounds, targetFeature, horizontal) - sourceValue;
        if (std::abs(correction) > threshold) continue;
        ReferenceCandidate candidate{target.objectId, targetFeature, sourceFeature, correction};
        if (!best.has_value() || referenceLess(candidate, *best)) best = candidate;
      }
    }
  }
  return best;
}

void independentReferenceDifferential() {
  SnapResolver resolver;
  const std::vector<SnapTarget> targets{
      {ObjectId::fromUint64(101U), {100, 10, 140, 80}, false, false},
      {ObjectId::fromUint64(7U), {94, 30, 160, 90}, false, false},
      {ObjectId::fromUint64(88U), {96, 30, 160, 90}, true, false},
      {ObjectId::fromUint64(99U), {96, 30, 160, 90}, false, true},
      {ObjectId::fromUint64(500U), {400, 400, 450, 450}, false, false},
  };
  for (const float dpr : {1.0F, 2.0F, 3.0F}) {
    for (const auto source : {WorldRect{94, 40, 114, 60}, WorldRect{95, 40, 115, 60},
                              WorldRect{111, 40, 131, 60}, WorldRect{400, 400, 420, 420}}) {
      resolver.reset();
      const auto actual = resolver.resolve(SnapSource{source}, targets, dpr);
      const auto expectedX = referenceResolveAxis(source, targets, true, dpr, 7U, 6.0F);
      const auto expectedY = referenceResolveAxis(source, targets, false, dpr, 7U, 6.0F);
      assert(actual.x.has_value() == expectedX.has_value());
      assert(actual.y.has_value() == expectedY.has_value());
      if (expectedX) {
        assert(actual.x->targetObjectId == expectedX->target);
        assert(actual.x->targetFeature == expectedX->targetFeature);
        assert(actual.x->sourceFeature == expectedX->sourceFeature);
        assert(std::abs(actual.x->correction - expectedX->correction) < 1.0e-5F);
      }
      if (expectedY) {
        assert(actual.y->targetObjectId == expectedY->target);
        assert(actual.y->targetFeature == expectedY->targetFeature);
        assert(actual.y->sourceFeature == expectedY->sourceFeature);
        assert(std::abs(actual.y->correction - expectedY->correction) < 1.0e-5F);
      }
    }
    resolver.reset();
  }
}

void independentHysteresisDifferential() {
  const std::vector<SnapTarget> targets{{ObjectId::fromUint64(9U), {100, 0, 200, 100}, false, false}};
  SnapResolver resolver;
  resolver.setPolicy({3.0F, 5.0F});
  assert(resolver.resolve(SnapSource{WorldRect{98, 40, 118, 60}}, targets, 1.0F).x.has_value());
  assert(resolver.resolve(SnapSource{WorldRect{104, 40, 124, 60}}, targets, 1.0F).x.has_value());
  assert(!resolver.resolve(SnapSource{WorldRect{106, 40, 126, 60}}, targets, 1.0F).x.has_value());
  resolver.reset();
  assert(resolver.resolve(SnapSource{WorldRect{99, 40, 119, 60}}, targets, 2.0F).x.has_value());
  assert(resolver.resolve(SnapSource{WorldRect{102, 40, 122, 60}}, targets, 2.0F).x.has_value());
  assert(!resolver.resolve(SnapSource{WorldRect{103, 40, 123, 60}}, targets, 2.0F).x.has_value());
}

int main() {
  fitPolicy();
  snapPolicy();
  snapPolicyBProfile();
  independentReferenceDifferential();
  independentHysteresisDifferential();
}

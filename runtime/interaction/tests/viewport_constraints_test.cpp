#include "canvas/interaction/viewport_constraints.hpp"

#include <cassert>
#include <cmath>
#include <optional>
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
  assert(first.x->targetObjectId == ObjectId::fromUint64(3));
  assert(first.x->targetFeature == SnapFeatureKind::kMinEdge);
  assert(first.x->sourceFeature == SnapFeatureKind::kMinEdge);
  const auto retained = resolver.resolve(SnapSource{WorldRect{95, 40, 115, 60}}, targets);
  assert(retained.x.has_value() && retained.x->targetObjectId == ObjectId::fromUint64(3));
  const auto released = resolver.resolve(SnapSource{WorldRect{111, 40, 131, 60}}, targets);
  assert(!released.x.has_value() && released.y.has_value());
  const auto tie = resolver.resolve(SnapSource{WorldRect{95, 40, 115, 60}}, targets);
  assert(tie.x.has_value() && tie.x->targetObjectId == ObjectId::fromUint64(3));
}

int main() { fitPolicy(); snapPolicy(); }

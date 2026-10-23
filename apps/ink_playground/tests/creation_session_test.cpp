#include "../common/creation_session.hpp"

#include <cassert>
#include <limits>

using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::ink_playground::CreationPreset;
using canvas::ink_playground::CreationSession;
using canvas::ink_playground::CreationStyle;
using canvas::semantic::ConnectorContent;
using canvas::semantic::ObjectKind;
using canvas::semantic::ShapeContent;
using canvas::input::PointerKey;

namespace {
void down_freezes_preset_and_valid_up_commits_once() {
  CreationSession session;
  const PointerKey key{11U, 7U, 1U};
  assert(session.begin(key, CreationPreset::kRectangle, CreationStyle{},
                       {10.0F, 20.0F}, {0.0F, 0.0F, 200.0F, 160.0F}, 100));
  assert(session.move(key, {70.0F, 80.0F}));
  assert(session.preview().valid);
  assert(std::holds_alternative<ShapeContent>(session.preview().record.content));
  const auto content = std::get<ShapeContent>(session.preview().record.content);
  assert(content.shape_kind == 1U && content.width == 60.0 && content.height == 60.0);
  const auto commit = session.finish(key);
  assert(commit.has_value() && commit->record.kind == ObjectKind::kShape);
  assert(session.finish(key) == std::nullopt);
}

void triangle_and_arrow_use_frozen_semantic_tuples() {
  CreationSession triangle;
  const PointerKey triangleKey{11U, 1U, 1U};
  assert(triangle.begin(triangleKey, CreationPreset::kTriangle, CreationStyle{},
                        {40.0F, 40.0F}, {0.0F, 0.0F, 200.0F, 200.0F}, 200));
  assert(triangle.move(triangleKey, {100.0F, 80.0F}));
  assert(triangle.preview().record.kind == ObjectKind::kShape);
  assert(triangle.preview().record.kind_version == 2U);
  assert(std::get<ShapeContent>(triangle.preview().record.content).shape_kind == 3U);

  CreationSession arrow;
  const PointerKey arrowKey{12U, 2U, 1U};
  assert(arrow.begin(arrowKey, CreationPreset::kArrow, CreationStyle{},
                     {20.0F, 20.0F}, {0.0F, 0.0F, 200.0F, 200.0F}, 300));
  assert(arrow.move(arrowKey, {120.0F, 20.0F}));
  assert(arrow.preview().record.kind == ObjectKind::kConnector);
  const auto& connector = std::get<ConnectorContent>(arrow.preview().record.content);
  assert(std::holds_alternative<canvas::semantic::FreePointEndpoint>(connector.start.value));
  bool has_arrow = false;
  for (const auto& entry : arrow.preview().record.properties.entries)
    if (entry.field_id == 0x201U) has_arrow = true;
  assert(has_arrow);
}

void cancel_wrong_pointer_and_zero_extent_do_not_commit() {
  CreationSession session;
  const PointerKey key{13U, 9U, 1U};
  assert(session.begin(key, CreationPreset::kEllipse, CreationStyle{},
                       {10.0F, 10.0F}, {0.0F, 0.0F, 100.0F, 100.0F}, 400));
  assert(!session.move(PointerKey{14U, 8U, 1U}, {40.0F, 40.0F}));
  assert(!session.finish(PointerKey{14U, 8U, 1U}).has_value());
  assert(session.move(key, {10.0F, 10.0F}));
  assert(!session.finish(key).has_value());
  assert(!session.active());
}

void snapping_uses_visible_object_targets_not_viewport_center() {
  CreationSession session;
  const PointerKey key{21U, 4U, 1U};
  assert(session.begin(key, CreationPreset::kRectangle, CreationStyle{},
                      {40.0F, 40.0F}, {0.0F, 0.0F, 200.0F, 200.0F}, 500));
  session.setSnapTargets({
      {canvas::foundation::ObjectId::fromUint64(99U),
       {150.0F, 150.0F, 190.0F, 190.0F}, false, false}});
  assert(session.move(key, {97.0F, 80.0F}));
  assert(session.preview().end.x == 97.0F);
  assert(session.preview().snapped == false);
}

void nonfinite_move_cancels_without_ghost_session() {
  CreationSession session;
  const PointerKey key{22U, 5U, 1U};
  assert(session.begin(key, CreationPreset::kEllipse, CreationStyle{},
                      {10.0F, 10.0F}, {0.0F, 0.0F, 100.0F, 100.0F}, 600));
  assert(!session.move(key, {std::numeric_limits<float>::quiet_NaN(), 20.0F}));
  assert(!session.active());
  assert(!session.finish(key).has_value());
}

// A target beyond the frozen finite viewport must not pull an otherwise
// unsnapped endpoint onto its edge merely because its orthogonal axis is near.
void offscreen_target_cannot_snap_creation() {
  CreationSession session;
  const PointerKey key{23U, 5U, 1U};
  session.setSnapTargets({{canvas::foundation::ObjectId::fromUint64(100U),
                          {99.0F, 400.0F, 120.0F, 440.0F}, false, false}});
  assert(session.begin(key, CreationPreset::kRectangle, CreationStyle{},
                       {40.0F, 40.0F}, {0.0F, 0.0F, 200.0F, 200.0F}, 700));
  assert(session.move(key, {97.0F, 80.0F}));
  assert(session.preview().end.x == 97.0F);
  assert(!session.preview().snapped);
}
void fixed_edge_must_not_suppress_movable_endpoint() {
  CreationSession session;
  const PointerKey key{24U, 1U, 1U};
  session.setSnapTargets({{canvas::foundation::ObjectId::fromUint64(101U),
                          {40.0F, 150.0F, 100.0F, 180.0F}, false, false}});
  assert(session.begin(key, CreationPreset::kRectangle, CreationStyle{},
                       {40.0F, 40.0F}, {0.0F, 0.0F, 200.0F, 200.0F}, 701));
  assert(session.move(key, {97.0F, 80.0F}));
  assert(session.preview().end.x == 100.0F);
  assert(session.preview().snapped);
}

void connector_endpoint_attaches_to_shape_target() {
  CreationSession session;
  const PointerKey key{25U, 1U, 1U};
  session.setSnapTargets({{canvas::foundation::ObjectId::fromUint64(202U),
                           {100.0F, 40.0F, 160.0F, 100.0F}, false, false}});
  assert(session.begin(key, CreationPreset::kLine, CreationStyle{},
                       {20.0F, 70.0F}, {0.0F, 0.0F, 240.0F, 180.0F}, 702));
  assert(session.move(key, {98.0F, 70.0F}));
  const auto& connector = std::get<ConnectorContent>(session.preview().record.content);
  const auto* attached = std::get_if<canvas::semantic::AttachedEndpoint>(&connector.end.value);
  assert(attached != nullptr);
  assert(attached->target_object_id == canvas::foundation::ObjectId::fromUint64(202U));
}
}  // namespace

int main() {
  down_freezes_preset_and_valid_up_commits_once();
  triangle_and_arrow_use_frozen_semantic_tuples();
  cancel_wrong_pointer_and_zero_extent_do_not_commit();
  snapping_uses_visible_object_targets_not_viewport_center();
  nonfinite_move_cancels_without_ghost_session();
  offscreen_target_cannot_snap_creation();
  fixed_edge_must_not_suppress_movable_endpoint();
  connector_endpoint_attaches_to_shape_target();
  return 0;
}

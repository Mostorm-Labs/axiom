#include "../common/ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"

#include <cassert>
#include <algorithm>
#include <iostream>
#include <set>

using canvas::ink_playground::CreationPreset;
using canvas::ink_playground::InkPlaygroundHost;
using canvas::input::PlatformPointerBatch;
using canvas::input::PointerPhase;
using canvas::input::SampleProvenance;

static void pointer(InkPlaygroundHost& host, std::uint64_t sequence,
                    float x, float y, PointerPhase phase) {
  PlatformPointerBatch batch;
  batch.samples.push_back({11U, 7U, sequence, sequence * 1000000U, x, y, 0.5F,
                           0.0F, 0.0F, {}, {}, SampleProvenance::kConfirmedCurrent, phase});
  assert(host.acceptPlatformBatch(batch, sequence * 1000000U));
}

void remote_edit_must_fence_undo() {
  InkPlaygroundHost host;assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host,1,20,20,PointerPhase::kDown);pointer(host,2,100,80,PointerPhase::kUp);
  assert(host.setSelectionMode(true));assert(host.selectAtViewPoint(60,50));
  assert(host.setSelectedObjectSize(120,90));
  using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  assert(Access::applyRemote(host,canvas::semantic::SetObjectSizeOp{{{host.selectedPrimaryObject(),140,100}}}));
  const auto objects=Access::objects(host);const auto gen=host.semanticGeneration();const auto count=host.submittedOperationCount();
  assert(!host.undo());
  assert(host.canUndo() && !host.canRedo());
  assert(Access::objects(host)==objects && host.semanticGeneration()==gen && host.submittedOperationCount()==count);
}
void creation_records_conditional_history() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host,1,20,20,PointerPhase::kDown); pointer(host,2,100,80,PointerPhase::kUp);
  assert(host.canUndo());
  assert(host.undo()); assert(host.semanticObjectCount()==0);
  assert(host.redo()); assert(host.semanticObjectCount()==1);
}
void lock_blocks_actual_handle_preview() {
  InkPlaygroundHost host; assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host,1,20,20,PointerPhase::kDown);pointer(host,2,100,80,PointerPhase::kUp);
  assert(host.setSelectionMode(true));assert(host.selectAtViewPoint(60,50));
  assert(host.patchSelectedProperty(2U,canvas::semantic::PropertyValue{true}));
  const auto* overlay=host.selectionOverlay();assert(overlay);
  const auto point=overlay->handle(canvas::render::HandleKind::kRight).center;
  assert(host.selectionPointer(88,PointerPhase::kDown,point.x,point.y));
  assert(host.selectionPointer(88,PointerPhase::kMove,point.x+30,point.y));
  assert(!host.selectionTransformActive() && host.transientTransformCount()==0);
  assert(!host.deleteSelectedObjects());
}
void dependent_connector_survives_insert_undo() {
  InkPlaygroundHost host;assert(host.bindSurface(256,256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host,1,20,20,PointerPhase::kDown);pointer(host,2,100,80,PointerPhase::kUp);
  using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  const auto target=Access::objects(host).front().id;
  canvas::semantic::ObjectRecord connector;
  connector.id=canvas::foundation::ObjectId::fromUint64(0x991);
  connector.kind=canvas::semantic::ObjectKind::kConnector;connector.kind_version=1;
  connector.placement.order_key=canvas::semantic::OrderKey{{0x30}};
  canvas::semantic::ConnectorContent content;
  content.start.value=canvas::semantic::AttachedEndpoint{target,canvas::semantic::AutoPerimeterAnchor{}};
  content.end.value=canvas::semantic::FreePointEndpoint{{140,120}};
  content.routing=canvas::semantic::ConnectorRouting::kStraight;connector.content=content;
  assert(Access::applyRemote(host,canvas::semantic::InsertObjectsOp{{connector}}));
  const auto before=Access::objects(host);const auto generation=host.semanticGeneration();
  assert(!host.undo());assert(host.canUndo() && !host.canRedo());
  assert(Access::objects(host)==before && host.semanticGeneration()==generation);
}

void selected_attachment_point_creates_connector_and_target() {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  using namespace canvas::semantic;
  InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host, 1, 20, 20, PointerPhase::kDown);
  pointer(host, 2, 100, 80, PointerPhase::kUp);
  assert(host.setSelectionMode(true));
  assert(host.selectAtViewPoint(60, 50));
  const auto* overlay = host.selectionOverlay();
  assert(overlay != nullptr);
  assert(overlay->attachmentPoints().size() == 4U);
  const auto start = overlay->attachmentPoints()[1].center;
  assert(!overlay->attachmentPoints()[1].hovered);
  assert(host.hoverSelectionAtViewPoint(start.x, start.y));
  assert(host.selectionOverlay()->attachmentPoints()[1].hovered);
  const canvas::input::PointerKey key{11U, 91U, 1U};
  assert(host.selectionPointer(key, PointerPhase::kDown, start.x, start.y));
  assert(host.selectionPointer(key, PointerPhase::kMove, 180.0F, 60.0F));
  assert(host.selectionPointer(key, PointerPhase::kUp, 180.0F, 60.0F));
  const auto objects = Access::objects(host);
  assert(objects.size() == 3U);
  const auto connector = std::find_if(objects.begin(), objects.end(), [](const auto& object) {
    return object.kind == ObjectKind::kConnector;
  });
  assert(connector != objects.end());
  const auto& content = std::get<ConnectorContent>(connector->content);
  const auto startAttachment = std::get_if<AttachedEndpoint>(&content.start.value);
  assert(startAttachment != nullptr);
  assert(startAttachment->target_object_id == objects.front().id ||
         startAttachment->target_object_id == objects.back().id);
  const auto target = std::find_if(objects.begin(), objects.end(), [&](const auto& object) {
    return object.id != startAttachment->target_object_id && object.kind != ObjectKind::kConnector;
  });
  assert(target != objects.end());
  assert(target->kind == ObjectKind::kShape);
  const auto endAttachment = std::get_if<AttachedEndpoint>(&content.end.value);
  assert(endAttachment != nullptr);
  // Source is the selected Shape's right port, so the generated target is
  // attached on its left port.
  assert(std::holds_alternative<StablePortAnchor>(endAttachment->anchor));
  assert(std::get<StablePortAnchor>(endAttachment->anchor).port_id == 4U);
}

void selected_attachment_point_click_creates_connector_and_target() {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host, 1, 20, 20, PointerPhase::kDown);
  pointer(host, 2, 100, 80, PointerPhase::kUp);
  assert(host.setSelectionMode(true));
  assert(host.selectAtViewPoint(60, 50));
  const auto point = host.selectionOverlay()->attachmentPoints()[1].center;
  const canvas::input::PointerKey key{11U, 92U, 1U};
  assert(host.selectionPointer(key, PointerPhase::kDown, point.x, point.y));
  assert(host.selectionPointer(key, PointerPhase::kUp, point.x, point.y));
  const auto objects = Access::objects(host);
  assert(objects.size() == 3U);
}

void selected_attachment_point_click_uses_opposite_target_port_and_source_kind() {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  using namespace canvas::semantic;
  struct Case {
    CreationPreset preset;
    std::size_t sourcePortIndex;
    std::uint32_t expectedTargetPort;
    ObjectKind expectedKind;
    std::uint32_t expectedShapeKind;
  };
  const std::array cases{
      Case{CreationPreset::kRectangle, 0U, 3U, ObjectKind::kShape, 1U},
      Case{CreationPreset::kEllipse, 1U, 4U, ObjectKind::kShape, 2U},
      Case{CreationPreset::kTriangle, 2U, 1U, ObjectKind::kShape, 3U},
      Case{CreationPreset::kSticky, 3U, 2U, ObjectKind::kSticky, 0U},
      Case{CreationPreset::kTriangle, 1U, 4U, ObjectKind::kShape, 3U},
      Case{CreationPreset::kTriangle, 3U, 2U, ObjectKind::kShape, 3U},
  };
  for (const auto& test : cases) {
    InkPlaygroundHost host;
    assert(host.bindSurface(256, 256));
    assert(host.selectCreationPreset(test.preset));
    pointer(host, 1U, 20.0F, 20.0F, PointerPhase::kDown);
    pointer(host, 2U, 100.0F, 80.0F, PointerPhase::kUp);
    const auto sourceId = Access::objects(host).front().id;
    assert(host.setSelectionMode(true));
    assert(host.selectAtViewPoint(60.0F, 50.0F));
    const auto* overlay = host.selectionOverlay();
    assert(overlay != nullptr);
    assert(overlay->attachmentPoints().size() == 4U);
    const auto point = overlay->attachmentPoints()[test.sourcePortIndex].center;
    const auto sourcePoint = point;
    const canvas::input::PointerKey key{11U,
        static_cast<std::uint64_t>(100U + test.sourcePortIndex), 1U};
    assert(host.selectionPointer(key, PointerPhase::kDown, point.x, point.y));
    assert(host.selectionPointer(key, PointerPhase::kUp, point.x, point.y));

    const auto objects = Access::objects(host);
    assert(objects.size() == 3U);
    const auto connector = std::find_if(objects.begin(), objects.end(), [](const auto& object) {
      return object.kind == ObjectKind::kConnector;
    });
    assert(connector != objects.end());
    const auto& content = std::get<ConnectorContent>(connector->content);
    const auto start = std::get_if<AttachedEndpoint>(&content.start.value);
    const auto end = std::get_if<AttachedEndpoint>(&content.end.value);
    assert(start != nullptr && end != nullptr);
    assert(start->target_object_id == sourceId);
    assert(std::holds_alternative<StablePortAnchor>(start->anchor));
    assert(std::get<StablePortAnchor>(start->anchor).port_id ==
           static_cast<std::uint32_t>(test.sourcePortIndex + 1U));
    assert(std::holds_alternative<StablePortAnchor>(end->anchor));
    assert(std::get<StablePortAnchor>(end->anchor).port_id == test.expectedTargetPort);

    const auto target = std::find_if(objects.begin(), objects.end(), [&](const auto& object) {
      return object.id != sourceId && object.id != connector->id;
    });
    assert(target != objects.end());
    assert(target->kind == test.expectedKind);
    if (test.expectedKind == ObjectKind::kShape) {
      const auto* shape = std::get_if<ShapeContent>(&target->content);
      assert(shape != nullptr);
      assert(shape->shape_kind == test.expectedShapeKind);
    } else {
      assert(std::holds_alternative<StickyContent>(target->content));
    }
    double targetWidth = 0.0;
    double targetHeight = 0.0;
    if (const auto* shape = std::get_if<ShapeContent>(&target->content)) {
      targetWidth = shape->width;
      targetHeight = shape->height;
    } else {
      const auto* sticky = std::get_if<StickyContent>(&target->content);
      assert(sticky != nullptr);
      targetWidth = sticky->width;
      targetHeight = sticky->height;
    }
    canvas::foundation::WorldPoint expectedEnd{sourcePoint.x, sourcePoint.y};
    if (test.sourcePortIndex == 0U) expectedEnd.y -= 120.0F;
    else if (test.sourcePortIndex == 1U) expectedEnd.x += 120.0F;
    else if (test.sourcePortIndex == 2U) expectedEnd.y += 120.0F;
    else expectedEnd.x -= 120.0F;
    canvas::foundation::WorldPoint targetPoint{
        static_cast<float>(target->transform.tx), static_cast<float>(target->transform.ty)};
    if (test.expectedTargetPort == 1U) {
      targetPoint.x += static_cast<float>(targetWidth * 0.5);
    } else if (test.expectedTargetPort == 2U) {
      targetPoint.x += static_cast<float>(targetWidth * (test.expectedShapeKind == 3U ? 0.75 : 1.0));
      targetPoint.y += static_cast<float>(targetHeight * 0.5);
    } else if (test.expectedTargetPort == 3U) {
      targetPoint.x += static_cast<float>(targetWidth * 0.5);
      targetPoint.y += static_cast<float>(targetHeight);
    } else {
      targetPoint.x += static_cast<float>(targetWidth * (test.expectedShapeKind == 3U ? 0.25 : 0.0));
      targetPoint.y += static_cast<float>(targetHeight * 0.5);
    }
    assert(std::abs(targetPoint.x - expectedEnd.x) < 1.0e-4F);
    assert(std::abs(targetPoint.y - expectedEnd.y) < 1.0e-4F);
  }
}

void selected_attachment_click_does_not_snap_generated_target_to_neighbor() {
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  using namespace canvas::semantic;
  InkPlaygroundHost host;
  assert(host.bindSurface(512, 512));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host, 1U, 180.0F, 180.0F, PointerPhase::kDown);
  pointer(host, 2U, 260.0F, 260.0F, PointerPhase::kUp);
  const auto sourceId = Access::objects(host).front().id;
  // Put an existing object where the left-side click's generated target would
  // otherwise be close enough for normal drag snapping.
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host, 3U, 60.0F, 180.0F, PointerPhase::kDown);
  pointer(host, 4U, 180.0F, 260.0F, PointerPhase::kUp);
  const auto neighborId = Access::objects(host).back().id;
  assert(host.setSelectionMode(true));
  assert(host.selectAtViewPoint(220.0F, 220.0F));
  const auto leftPoint = host.selectionOverlay()->attachmentPoints()[3].center;
  const canvas::input::PointerKey key{11U, 121U, 1U};
  assert(host.selectionPointer(key, PointerPhase::kDown, leftPoint.x, leftPoint.y));
  assert(host.selectionPointer(key, PointerPhase::kUp, leftPoint.x, leftPoint.y));

  const auto objects = Access::objects(host);
  assert(objects.size() == 4U);
  const auto connector = std::find_if(objects.begin(), objects.end(), [](const auto& object) {
    return object.kind == ObjectKind::kConnector;
  });
  assert(connector != objects.end());
  const auto& content = std::get<ConnectorContent>(connector->content);
  const auto end = std::get_if<AttachedEndpoint>(&content.end.value);
  assert(end != nullptr);
  assert(end->target_object_id != neighborId);
  assert(end->target_object_id != sourceId);
  assert(std::holds_alternative<StablePortAnchor>(end->anchor));
  assert(std::get<StablePortAnchor>(end->anchor).port_id == 2U);
}

void off_center_attachment_click_still_creates_an_axis_aligned_sibling() {
  using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  using namespace canvas::semantic;
  for(unsigned port=0;port<4;++port) {
    InkPlaygroundHost host;assert(host.bindSurface(512,512));
    assert(host.selectCreationPreset(CreationPreset::kRectangle));
    pointer(host,1,180,180,PointerPhase::kDown);pointer(host,2,260,260,PointerPhase::kUp);
    assert(host.setSelectionMode(true)&&host.selectAtViewPoint(220,220));
    auto point=host.selectionOverlay()->attachmentPoints()[port].center;
    if(port%2==0)point.x+=6;else point.y+=6;
    const canvas::input::PointerKey key{11,600+port,1};
    assert(host.selectionPointer(key,PointerPhase::kDown,point.x,point.y));
    assert(host.selectionPointer(key,PointerPhase::kUp,point.x,point.y));
    const auto objects=Access::objects(host);assert(objects.size()==3);
    auto connector=*std::find_if(objects.begin(),objects.end(),[](const auto& o){return o.kind==ObjectKind::kConnector;});
    std::get<ConnectorContent>(connector.content).routing=ConnectorRouting::kOrthogonal;
    const auto route=canvas::scene::connectorGeometry(connector,objects);assert(route);
    assert(route->status==canvas::runtime::ConnectorRouteStatus::kReady);
    assert(route->route.size()==2);
    if(port%2==0)assert(route->start.x==route->end.x);else assert(route->start.y==route->end.y);
    assert(host.submittedOperationCount()==2);
  }
}

void connector_preview_follows_transient_target_without_canonical_edit() {
  using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  using namespace canvas::semantic;
  InkPlaygroundHost host;assert(host.bindSurface(512,512));
  ObjectRecord source{};source.id=canvas::foundation::ObjectId::fromUint64(501);
  source.kind=ObjectKind::kShape;source.kind_version=1;source.content=ShapeContent{1,80,80};
  source.placement.order_key=OrderKey{{10}};source.transform.tx=20;source.transform.ty=20;
  auto target=source;target.id=canvas::foundation::ObjectId::fromUint64(502);target.transform.tx=300;
  target.placement.order_key=OrderKey{{20}};
  ObjectRecord connector{};connector.id=canvas::foundation::ObjectId::fromUint64(503);
  connector.kind=ObjectKind::kConnector;connector.kind_version=1;connector.placement.order_key=OrderKey{{30}};
  connector.properties.entries={{0x101U,StrokeStyleValue{SolidStroke{{0,0,0,1},4,StrokeCap::kButt,RoundJoin{},SolidDash{}}}}};
  connector.content=ConnectorContent{{AttachedEndpoint{source.id,StablePortAnchor{2}}},
      {AttachedEndpoint{target.id,StablePortAnchor{4}}},ConnectorRouting::kOrthogonal};
  assert(Access::applyRemote(host,InsertObjectsOp{{source,target,connector}}));
  for(unsigned q=0;q<16;++q)assert(host.serviceConnectorRouting());
  const auto before=Access::objects(host);const auto generation=host.semanticGeneration();
  assert(host.setSelectionMode(true)&&host.selectAtViewPoint(60,60));
  const canvas::input::PointerKey key{11,450,1};
  assert(host.selectionPointer(key,PointerPhase::kDown,60,60));
  const std::array<std::array<unsigned,2>,3> moves{{{180,150},{160,135},{200,165}}};
  std::vector<std::uint8_t> pixels(512U*512U*4U);
  for(const auto& move:moves) {
    assert(host.selectionPointer(key,PointerPhase::kMove,60,static_cast<float>(move[0])));
    assert(host.presentTransientOverlayFrame(host.nextCanonicalFrameId()));
    assert(host.previewSurfaceProvider()->readbackRgba(pixels).code==canvas::render::BackendSubmissionCode::kAccepted);
    // The real rendered frame must contain the middle vertical leg, and no
    // diagonal chord. Coordinates are independent hand-checked fixture points.
    const auto elbow=(100U*512U+200U)*4U;
    const auto diagonal=(move[1]*512U+150U)*4U;
    assert(pixels[elbow]<32U&&pixels[elbow+1]<32U&&pixels[elbow+2]<32U);
    assert(pixels[diagonal]==255U&&pixels[diagonal+1]==255U&&pixels[diagonal+2]==255U);
    const auto route=host.previewConnectorGeometry(connector);
    assert(route.status==canvas::runtime::ConnectorRouteStatus::kReady&&route.route.size()==4);
    for(std::size_t n=1;n<route.route.size();++n)
      assert(route.route[n-1].x==route.route[n].x||route.route[n-1].y==route.route[n].y);
    assert(std::abs(route.start.y-static_cast<float>(move[0]))<1e-4F);
    assert(Access::objects(host)==before&&host.semanticGeneration()==generation);
  }
  assert(host.selectionPointer(key,PointerPhase::kCancel,60,200));
  assert(host.serviceConnectorRouting());
  assert(!host.connectorPreviewRoutingPending());
  assert(Access::objects(host)==before);
  const auto operations=host.submittedOperationCount();
  const canvas::input::PointerKey committed{11,451,1};
  assert(host.selectionPointer(committed,PointerPhase::kDown,60,60));
  assert(host.selectionPointer(committed,PointerPhase::kMove,60,180));
  const auto live=host.previewConnectorGeometry(connector);
  assert(host.selectionPointer(committed,PointerPhase::kUp,60,180));
  assert(host.presentCanonicalFrame(host.nextCanonicalFrameId(),0.0));
  for(unsigned q=0;q<16;++q)assert(host.serviceConnectorRouting());
  const auto records=Access::runtimeRecords(host);
  const auto found=std::find_if(records.begin(),records.end(),[&](const auto& r){return r.objectId==connector.id;});
  assert(found!=records.end());
  const auto& final=std::get<canvas::scene::ConnectorGeometry>(found->derivedGeometry);
  assert(final.status==canvas::runtime::ConnectorRouteStatus::kReady&&final.route.size()==live.route.size());
  for(std::size_t n=0;n<final.route.size();++n)
    assert(final.route[n].x==live.route[n].x&&final.route[n].y==live.route[n].y);
  assert(host.submittedOperationCount()==operations+1);
}

void remote_child_survives_conditional_insert_undo() {
  using namespace canvas::semantic;
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  InkPlaygroundHost host;
  assert(host.bindSurface(256, 256));
  ObjectRecord parent{};
  parent.id = canvas::foundation::ObjectId::fromUint64(0xA01);
  parent.kind = ObjectKind::kGroup;
  parent.kind_version = 1;
  parent.placement.order_key = OrderKey{{0x40}};
  parent.content = GroupContent{};
  assert(Access::insertHistoryFixture(host, parent));
  ObjectRecord child{};
  child.id = canvas::foundation::ObjectId::fromUint64(0xA02);
  child.kind = ObjectKind::kShape;
  child.kind_version = 1;
  child.placement = {parent.id, OrderKey{{0x40}}};
  child.content = ShapeContent{1, 40, 30};
  assert(Access::applyRemote(host, InsertObjectsOp{{child}}));
  const auto before = Access::objects(host);
  const auto generation = host.semanticGeneration();
  const auto operations = host.submittedOperationCount();
  const auto outcome = host.operationOutcome();
  assert(!host.undo());
  assert(host.canUndo() && !host.canRedo());
  assert(Access::objects(host) == before);
  assert(host.semanticGeneration() == generation);
  assert(host.submittedOperationCount() == operations);
  assert(host.operationOutcome().applied->operation_id == outcome.applied->operation_id);
}

struct HistoryRecorder final : canvas::interaction::LocalOperationSink {
  std::vector<canvas::semantic::Operation> operations;
  void applied(const canvas::semantic::Operation& operation,
               const canvas::semantic::CanonicalCommitRecord&) noexcept override {
    operations.push_back(operation);
  }
  canvas::interaction::AxisOperationStatus status(const canvas::semantic::OperationId&,
      const canvas::semantic::CanonicalCommitStamp&) const noexcept override { return {}; }
};

void selected_preimage_matrix_uses_real_conditional_history() {
  using namespace canvas::semantic;
  using Access = canvas::ink_playground::InkPlaygroundHistoryTestAccess;
  std::size_t checked = 0;
  for (const auto preset : {CreationPreset::kRectangle, CreationPreset::kEllipse,
                           CreationPreset::kTriangle, CreationPreset::kLine, CreationPreset::kArrow}) {
    // Each independent graph starts with absent opacity/lock properties and a
    // known sibling order. Compare entire records, including field absence.
    for (unsigned action = 0; action < 7; ++action) {
      if (action == 0 && preset >= CreationPreset::kLine) continue;
      InkPlaygroundHost host;
      HistoryRecorder recorder;
      assert(host.bindSurface(256, 256));
      host.setLocalOperationSink(&recorder);
      assert(host.selectCreationPreset(preset));
      const canvas::input::PointerKey key{11, 7, 1};
      assert(host.creationPointer(key, PointerPhase::kDown, 20, 20));
      assert(host.creationPointer(key, PointerPhase::kUp, 100, 80));
      auto sibling = Access::objects(host).front();
      sibling.id = canvas::foundation::ObjectId::fromUint64(0xFFFF);
      sibling.placement.order_key = OrderKey{{0xF0}};
      sibling.transform.tx = 180;
      assert(Access::applyRemote(host, InsertObjectsOp{{sibling}}));
      assert(host.setSelectionMode(true));
      // Both the filled Shapes and the diagonal Line/Arrow contain this point.
      assert(host.selectAtViewPoint(60, 50));
      const auto before = Access::objects(host);
      const auto operationCount = host.submittedOperationCount();
      const auto generation = host.semanticGeneration().value();
      const auto eventCount = recorder.operations.size();
      switch (action) {
        case 0: assert(host.setSelectedObjectSize(140, 110)); break;
        case 1: assert(host.patchSelectedProperty(3, PropertyValue{0.25F})); break;
        case 2: assert(host.setSelectedTransform({1, 0, 0, 1, 32, 16})); break;
        case 3: assert(host.setSelectedOrder(true)); break;
        case 4: assert(host.patchSelectedProperty(2, PropertyValue{true})); break;
        case 5: assert(host.setSelectedAppearance({true, {1,0,0,1}, true, {0,1,0,1}, 7, 0.5F})); break;
        case 6: assert(host.deleteSelectedObjects()); break;
        default: assert(false);
      }
      const auto after = Access::objects(host);
      assert(after != before);
      assert(host.submittedOperationCount() == operationCount + 1);
      assert(host.semanticGeneration().value() == generation + 1);
      assert(recorder.operations.size() == eventCount + 1);
      assert(host.undo());
      assert(Access::objects(host) == before);
      assert(host.redo());
      assert(Access::objects(host) == after);
      assert(host.submittedOperationCount() == operationCount + 3);
      assert(host.semanticGeneration().value() == generation + 3);
      assert(recorder.operations.size() == eventCount + 3);
      std::set<OperationId> identities;
      for (const auto& operation : recorder.operations) assert(identities.insert(operation.id).second);
      ++checked;
    }
  }
  std::cout << "PASS selected preimage/Undo/Redo matrix: " << checked << " independent graphs\n";
}

int main(int argc, char** argv) {
  if(argc>1 && std::string_view(argv[1])=="connector-preview"){connector_preview_follows_transient_target_without_canonical_edit();return 0;}
  if(argc>1 && std::string_view(argv[1])=="child"){remote_child_survives_conditional_insert_undo();return 0;}
  if(argc>1 && std::string_view(argv[1])=="preimages"){selected_preimage_matrix_uses_real_conditional_history();return 0;}
  if(argc>1 && std::string_view(argv[1])=="locked"){lock_blocks_actual_handle_preview();return 0;}
  if(argc>1 && std::string_view(argv[1])=="dependent"){dependent_connector_survives_insert_undo();return 0;}
  if(argc>1 && std::string_view(argv[1])=="creation"){creation_records_conditional_history();return 0;}
  if(argc>1){remote_edit_must_fence_undo();return 0;}
  off_center_attachment_click_still_creates_an_axis_aligned_sibling();
  selected_attachment_point_creates_connector_and_target();
  selected_attachment_point_click_creates_connector_and_target();
  selected_attachment_point_click_uses_opposite_target_port_and_source_kind();
  selected_attachment_click_does_not_snap_generated_target_to_neighbor();
  connector_preview_follows_transient_target_without_canonical_edit();
  InkPlaygroundHost host;
  assert(host.bindSurface(256U, 256U));
  assert(host.selectCreationPreset(CreationPreset::kRectangle));
  pointer(host, 1U, 20.0F, 20.0F, PointerPhase::kDown);
  pointer(host, 2U, 100.0F, 80.0F, PointerPhase::kMove);
  pointer(host, 3U, 100.0F, 80.0F, PointerPhase::kUp);
  assert(host.semanticObjectCount() == 1U);
  assert(host.setSelectionMode(true));
  assert(host.selectAtViewPoint(60.0F, 50.0F));
  const auto before = canvas::ink_playground::InkPlaygroundHistoryTestAccess::objects(host);
  const auto operations = host.submittedOperationCount();
  assert(host.setSelectedObjectSize(120.0, 90.0));
  assert(host.submittedOperationCount() == operations + 1U);
  assert(host.undo());
  assert(canvas::ink_playground::InkPlaygroundHistoryTestAccess::objects(host) == before);
  assert(host.redo());
  assert(host.patchSelectedProperty(0x00000002U, canvas::semantic::PropertyValue{true}));
  assert(host.selectionPointer(88U, PointerPhase::kDown, 100.0F, 50.0F));
  assert(host.selectionPointer(88U, PointerPhase::kMove, 120.0F, 50.0F));
  assert(host.selectionPointer(88U, PointerPhase::kUp, 120.0F, 50.0F));
  assert(host.unlockSelectedObjects());
  assert(host.deleteSelectedObjects());
  assert(host.semanticObjectCount() == 0U);
  assert(host.undo());
  assert(host.semanticObjectCount() == 1U);
  assert(host.redo());
  assert(host.semanticObjectCount() == 0U);
  return 0;
}

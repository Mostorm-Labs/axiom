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
  if(argc>1 && std::string_view(argv[1])=="child"){remote_child_survives_conditional_insert_undo();return 0;}
  if(argc>1 && std::string_view(argv[1])=="preimages"){selected_preimage_matrix_uses_real_conditional_history();return 0;}
  if(argc>1 && std::string_view(argv[1])=="locked"){lock_blocks_actual_handle_preview();return 0;}
  if(argc>1 && std::string_view(argv[1])=="dependent"){dependent_connector_survives_insert_undo();return 0;}
  if(argc>1 && std::string_view(argv[1])=="creation"){creation_records_conditional_history();return 0;}
  if(argc>1){remote_edit_must_fence_undo();return 0;}
  selected_attachment_point_creates_connector_and_target();
  selected_attachment_point_click_creates_connector_and_target();
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

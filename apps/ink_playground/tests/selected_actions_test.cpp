#include "../common/ink_playground_host.hpp"
#include "ink_playground_history_test_access.hpp"

#include <cassert>

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
int main(int argc, char** argv) {
  if(argc>1 && std::string_view(argv[1])=="locked"){lock_blocks_actual_handle_preview();return 0;}
  if(argc>1 && std::string_view(argv[1])=="dependent"){dependent_connector_survives_insert_undo();return 0;}
  if(argc>1 && std::string_view(argv[1])=="creation"){creation_records_conditional_history();return 0;}
  if(argc>1){remote_edit_must_fence_undo();return 0;}
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

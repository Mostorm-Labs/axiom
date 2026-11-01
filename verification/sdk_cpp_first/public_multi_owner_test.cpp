#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/runtime_facade.hpp"
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); return 1; } } while (0)

int main() {
  using namespace canvas;
  const auto id=[](std::uint64_t n){return foundation::ObjectId::fromUint64(n);};
  runtime::RuntimeCompositionCore composition;
  CHECK(composition.open());
  CHECK(composition.bindSurface(256,256));
  semantic::InsertObjectsOp insert;
  for (std::uint64_t n=1;n<=2;++n) {
    semantic::ObjectRecord object{};
    object.id=id(n); object.kind=semantic::ObjectKind::kShape; object.kind_version=1;
    object.content=semantic::ShapeContent{1,20,20};
    object.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(n)});
    object.transform.tx=n==1?10:60; object.transform.ty=n==1?20:60;
    insert.objects.push_back(std::move(object));
  }
  const semantic::Operation seed{semantic::OperationId{id(100)},semantic::DocumentId{id(1)},1,1,std::move(insert)};
  CHECK(composition.applyExternalOperation(seed,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied);
  CHECK(composition.registerSource(7100));
  runtime::RuntimeFacade& facade=composition.controlFacade();
  CHECK(facade.setSelectionMode(true,1,facade.readRuntimeState().identity.runtimeGeneration,7100).state==runtime::ProductControlState::kQueued);
  CHECK(composition.ownerPump());
  std::uint64_t seq=0;
  const auto send=[&](input::PointerPhase phase,float x,float y,bool shift){
    const auto snapshot=facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    input::PlatformPointerSample sample{};sample.source=7100;sample.pointer=1;sample.sequence=++seq;
    sample.timestampNs=seq*1000000;sample.phase=phase;
    sample.x=x*snapshot.cameraScale+snapshot.cameraTranslationX;
    sample.y=y*snapshot.cameraScale+snapshot.cameraTranslationY;
    sample.modifiers=shift?input::PointerModifiers::kShift:input::PointerModifiers::kNone;
    input::PlatformPointerBatch batch;batch.samples.push_back(sample);return composition.submit(batch);
  };
  CHECK(send(input::PointerPhase::kDown,15,25,false)); CHECK(send(input::PointerPhase::kUp,15,25,false));
  CHECK(send(input::PointerPhase::kDown,65,65,true)); CHECK(send(input::PointerPhase::kUp,65,65,true));
  CHECK(facade.readSelectionSummary().count==2);
  auto projection=facade.readMultiSelection();
  CHECK(projection.available);
  const auto unknown=projection.properties.at(0x100U);
  CHECK(!unknown.editAvailable);
  const semantic::PropertyValue red=semantic::FillStyleValue{semantic::SolidFill{semantic::ColorValue{1,0,0,1}}};
  CHECK(!facade.submitMultiSelectionProperty(0x100U,red,projection.stamp,unknown.eligible,7200,1).queued);
  CHECK(facade.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kLocalSingleUser));
  projection=facade.readMultiSelection();
  CHECK(projection.available && projection.roots.size()==2);
  const auto selected=projection.properties.at(0x100U);
  CHECK(selected.editAvailable && selected.eligible.size()==2);
  CHECK(facade.submitMultiSelectionProperty(0x100U,red,projection.stamp,selected.eligible,7200,2).queued);
  CHECK(composition.ownerPump());
  const auto receipt=facade.multiSelectionReceipt(7200,2);
  CHECK(receipt && receipt->applied && receipt->terminal && receipt->changed.size()==2);
  const auto after=facade.readMultiSelection();
  CHECK(after.properties.at(0x100U).uniformValue==red);
  CHECK(facade.undo(3,facade.readRuntimeState().identity.runtimeGeneration,7200).state==runtime::ProductControlState::kQueued);
  CHECK(composition.ownerPump());
  const auto undone=facade.readMultiSelection();
  CHECK(undone.available && undone.properties.at(0x100U).uniformValue!=red);
  CHECK(composition.close());
  std::puts("PASS public RuntimeFacade multi-selection delegates to one real owner");
}

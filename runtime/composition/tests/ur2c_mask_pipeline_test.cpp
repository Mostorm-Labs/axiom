#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "../../render/src/selection_region_geometry_internal.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include <stdexcept>
#include <cstdio>
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
 public:
 static auto work(const interaction::CanvasEditingRuntime& r){return r.skiaRenderer_.renderCacheWork();}
 static auto scene(const interaction::CanvasEditingRuntime& r){return r.sceneCoordinator_->runtimeScene().publicationWork();}
 static bool stampParity(const interaction::CanvasEditingRuntime& r){
  auto full=r.sceneCompiler_->compileFull(semantic::SemanticReadView(r.semanticObjects_,r.semanticGeneration_.current(),r.resourceManifestPort_));
  if(!full)return false;const auto visible=r.runtimeSceneHost_->read();
  for(const auto& record:full.value().records){const auto* actual=visible.find(record.objectId);if(!actual||actual->contentRevision!=record.contentRevision||actual->renderPayload!=record.renderPayload||actual->hitGeometry!=record.hitGeometry)return false;}return true;
 }
 static auto history(const interaction::CanvasEditingRuntime& r){return r.history_.size();}
};
}
using namespace canvas;
namespace {
using Access=ink_playground::InkPlaygroundHistoryTestAccess;
void require(bool v,const char* s){if(!v)throw std::runtime_error(s);}
struct Host {
 runtime::RuntimeCompositionCore core;std::uint64_t seq=0,request=0;
 Host(){
  require(core.open(),"open");auto canonical=std::make_unique<render::RasterSkiaSurfaceProvider>();auto preview=std::make_unique<render::TransparentOverlaySkiaSurfaceProvider>();
  require(canonical->resize(256,128).code==render::BackendSubmissionCode::kAccepted&&preview->resize(256,128).code==render::BackendSubmissionCode::kAccepted,"resize");
  require(core.registerSurfaceProvider("ur2b-canonical",std::move(canonical))&&core.registerPreviewSurfaceProvider("ur2b-preview",std::move(preview)),"providers");
  require(core.bindSurface(256,128)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"source/bind");
  runtime::CanvasControlPayload p{};p.kind=runtime::CanvasControlPayloadKind::kSelectPreset;p.preset={"round-handdrawn",4};control(p);
  p={};p.kind=runtime::CanvasControlPayloadKind::kInkOptions;p.ink.size=runtime::OptionPatch<float>::set(24);control(p);
  require(pointer(input::PointerPhase::kDown,20,64),"brush down");for(unsigned i=1;i<25;++i)require(pointer(input::PointerPhase::kMove,20+i*8,64),"brush move");require(pointer(input::PointerPhase::kUp,220,64),"brush up");settle();
  p={};p.kind=runtime::CanvasControlPayloadKind::kSelectTool;p.tool=runtime::CanvasToolKind::kEraser;control(p);
  p={};p.kind=runtime::CanvasControlPayloadKind::kEraserOptions;p.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);p.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(20);control(p);settle();
 }
 void control(runtime::CanvasControlPayload p){auto& f=core.controlFacade();runtime::CanvasControlRequest r{};r.clientId=72;r.requestId=++request;r.target=f.mountedCanvasTargets().front();r.payload=std::move(p);require(f.submitCanvasControl(r).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");}
 bool pointer(input::PointerPhase phase,float x,float y){input::PlatformPointerSample s{};s.source=71;s.pointer=1;s.sequence=++seq;s.timestampNs=seq*8000000;s.x=x;s.y=y;s.pressure=.5F;s.phase=phase;return core.submit({{s}});}
 void settle(){for(unsigned i=0;i<8192;++i){runtime::RuntimeFrameOpportunity f;require(core.serviceFrameOpportunity(false,f),"opportunity");if(f.previewWork)require(core.finishFrameOpportunity({f.previewWork,true}),"preview ack");if(f.canonicalWork)require(core.receivePlatformPresented({*f.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"strict work ack");if(!core.canonicalPresentationDirty()&&!core.lastSubmittedCanonicalWork()&&!core.previewActive()&&core.qualificationRuntime().pendingCanonicalHandoffCount()==0)return;}require(false,"settle progress");}
 void erase(float x){require(pointer(input::PointerPhase::kDown,x,45)&&pointer(input::PointerPhase::kMove,x,83)&&pointer(input::PointerPhase::kUp,x,83),"actual partial erase");}
};
}
int main(int argc,char** argv){try{
 const bool probe=argc>1&&std::string_view(argv[1])=="--probe";
 Host h;auto& runtime=h.core.qualificationRuntime();const auto initial=runtime.snapshotObjects();require(initial.size()==1,"real brush object");
 const auto gen=runtime.semanticGeneration().value();const auto hist=Access::history(runtime);
 h.erase(90);const auto first=runtime.snapshotObjects();require(first.size()==1&&first[0].erase_masks.size()==1&&runtime.semanticGeneration().value()==gen+1&&Access::history(runtime)==hist+1,"one Up exactly one mutation");
 const auto sw=Access::scene(runtime);
 const auto w=Access::work(runtime);h.erase(150);const auto second=runtime.snapshotObjects();const auto after=Access::work(runtime);
 const auto endScene=Access::scene(runtime);std::printf("{\"scene_materializations\":%llu,\"scene_records\":%llu,\"body_clone_bytes\":%llu,\"mask_only_hits\":%llu}\n",(unsigned long long)(endScene.materializationCalls-sw.materializationCalls),(unsigned long long)(endScene.materializationRecords-sw.materializationRecords),(unsigned long long)(endScene.bodyCloneBytes-sw.bodyCloneBytes),(unsigned long long)(endScene.maskOnlyHits-sw.maskOnlyHits));
 if(!probe)require(endScene.materializationCalls==sw.materializationCalls&&endScene.maskOnlyHits>sw.maskOnlyHits,"mask-only Up rebuilt Scene body");
 require(after.exactBodyUses>w.exactBodyUses,"actual Runtime Up did not consume prepared body");
 require(second[0].content==initial[0].content&&second[0].erase_masks.size()==2&&runtime.semanticGeneration().value()==gen+2&&Access::history(runtime)==hist+2,"second Up source/commit identity");
 RuntimeSceneRecord original;original.kind=initial[0].kind;original.content=initial[0].content;original.eraseMasks=first[0].erase_masks;
 const foundation::WorldPoint points[]{{150,45},{150,83}};const auto mask=interaction::partialEraseGeometry(points,initial[0].transform,10);
 require(mask&&render::internal::intersectEraseGeometryComponentOracle(original,*mask)==render::SelectionGeometryHit::kHit,"Runtime target differs from old CPU oracle");
 require(h.pointer(input::PointerPhase::kDown,200,45)&&h.pointer(input::PointerPhase::kMove,200,83)&&h.pointer(input::PointerPhase::kCancel,200,83),"cancel terminal");require(runtime.snapshotObjects()==second,"cancel mutated canonical");
 runtime::CanvasControlPayload undo{};undo.kind=runtime::CanvasControlPayloadKind::kUndo;h.control(undo);require(runtime.snapshotObjects()==first,"Undo exact query mutation");undo.kind=runtime::CanvasControlPayloadKind::kRedo;h.control(undo);require(runtime.snapshotObjects()==second,"Redo exact query mutation");
 require(Access::stampParity(runtime),"incremental stamp differs from independent full serialization");
 h.settle();std::printf("{\"actual_Runtime_Up_prepared_uses\":%llu,\"full_validation_calls\":%llu,\"source_unchanged\":true,\"exactly_once_history\":true,\"cancel_undo_redo\":true}\n",static_cast<unsigned long long>(after.exactBodyUses-w.exactBodyUses),static_cast<unsigned long long>(after.exactFullValidations-w.exactFullValidations));return 0;
}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}

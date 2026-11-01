#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/scene/scene_record_store.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "../../../../runtime/semantic/src/object_store_mutator.hpp"
#include <iostream>
#include <stdexcept>
using namespace canvas;
void require(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
class Inspect final : public interaction::CanvasEditingRuntime {
public:
  const auto& scene()const{return *runtimeSceneHost_;}
  const auto& projection()const{return sceneCoordinator_->runtimeScene();}
  const auto& store()const{return semanticObjects_;}
};
void editor(){
  Inspect runtime;
  require(runtime.bindSurface(800,500)&&runtime.registerInputSource(71,"scene-locality")&&runtime.selectBrushProfile("round-handdrawn",1),"setup");
  runtime.setPlatformPresentationDeferred(true);
  for(unsigned n=0;n<32;++n){
    input::PlatformPointerBatch batch;
    for(unsigned i=0;i<3;++i){input::PlatformPointerSample s;
      s.source=71;s.pointer=1;s.sequence=n*3+i+1;s.timestampNs=s.sequence*4'000'000;
      s.x=20+i*30;s.y=20+n*10;
      s.phase=i==0?input::PointerPhase::kDown:i==2?input::PointerPhase::kUp:input::PointerPhase::kMove;batch.samples.push_back(s);
    }
    require(runtime.acceptPlatformBatch(batch,batch.samples.back().timestampNs),"editor stroke");
    require(runtime.semanticObjectCount()==n+1,"lost canonical mutation");
    require(!runtime.scene().invalidationOutput().fullScene,"editor still forces full rebuild");
    const auto oracle=scene::FullSceneCompiler::compile({runtime.store(),runtime.projection().generation()},nullptr,true);
    require(bool(oracle)&&oracle.value().records.size()==runtime.projection().records().size(),"oracle count");
    for(const auto& r:oracle.value().records){
      const auto* actual=runtime.projection().find(r.objectId);
      require(actual&&actual->content==r.content&&actual->transform==r.transform&&actual->worldBounds==r.worldBounds&&
        actual->referenceGeometryDigest==r.referenceGeometryDigest&&actual->directDependencies==r.directDependencies,"production full oracle mismatch");
    }
  }
  require(runtime.presentCanonicalFrame(runtime.nextCanonicalFrameId(),0),"canonical frame");
  require(runtime.undo()&&runtime.semanticObjectCount()==31&&runtime.redo()&&runtime.semanticObjectCount()==32,"undo/redo");
  std::cout<<"32 editor strokes use local delta; full oracle and history agree\n";
}
SceneRecord record(unsigned id,WorldRect bounds){
  SceneRecord r;r.objectId=ObjectId::fromUint64(id);r.orderKey=SceneOrderKey(id);r.flags=SceneRecordFlags::kVisible;
  r.worldBounds=bounds;r.contentRevision=ContentRevision(1);r.renderPayload={id,1};r.hitGeometry={id,1};return r;
}
void emptyDelta(){
  Scene s(std::make_unique<DirectRenderScene>(),std::make_unique<UniformGridSpatialIndex>());
  const auto first=record(1,{10,20,30,40});
  require(bool(s.replace({SceneRevision(1),{first}})),"seed Scene");
  require(bool(s.apply({SceneRevision(1),SceneRevision(2),{},std::nullopt})),"empty delta");
  require(s.read().records().size()==1&&s.read().find(first.objectId)&&*s.read().find(first.objectId)==first,"empty delta cleared published records");
  auto moved=first;moved.worldBounds={100,200,300,400};
  require(bool(s.apply({SceneRevision(2),SceneRevision(3),{{SceneMutationKind::kUpdate,first.objectId,first,moved}},std::nullopt})),"empty delta destroyed participant store");
  require(s.publishedBounds()==moved.worldBounds,"delta aggregate bounds incorrect");
  const auto second=record(2,{-100,-200,-30,-40});
  require(bool(s.apply({SceneRevision(3),SceneRevision(4),{{SceneMutationKind::kInsert,second.objectId,std::nullopt,second}},std::nullopt})),"insert");
  require(s.publishedBounds()==foundation::unionRects(moved.worldBounds,second.worldBounds),"insert drops unaffected bounds");
  require(bool(s.apply({SceneRevision(4),SceneRevision(5),{{SceneMutationKind::kRemove,second.objectId,second,std::nullopt}},std::nullopt})),"remove");
  require(s.publishedBounds()==moved.worldBounds,"delete does not shrink aggregate");
  std::cout<<"empty/update/insert/remove Scene publication stays coherent\n";
}
int main(int argc,char**argv){try{if(argc>1&&std::string(argv[1])=="empty")emptyDelta();else editor();return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}

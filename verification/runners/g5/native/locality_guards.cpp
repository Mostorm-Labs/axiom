#include "canvas/scene/scene_types.hpp"
#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "../../../../runtime/semantic/src/object_store_mutator.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include <iostream>
#include <stdexcept>

namespace canvas {
class IncrementalRuntimeTestAccess final {
public:
  static bool tryPublish(RuntimeScene& scene, RuntimeSceneProjection projection) {
    auto prepared=scene.prepare(std::move(projection));
    if(!prepared)return false;
    scene.publish(std::move(prepared.value()));
    return true;
  }
  static void fullAuthority(RuntimeScene& scene){scene._fullProjectionAuthority=true;}
  static void publish(RuntimeScene& scene, RuntimeSceneProjection projection) {
    auto prepared=scene.prepare(std::move(projection));
    if(!prepared)throw std::runtime_error("prepare projection");
    scene.publish(std::move(prepared.value()));
  }
  static void gate(RuntimeScene& scene, ScenePublicationGate* gate){scene.setPublicationGate(gate);}
  static void incremental(RuntimeScene& scene, const semantic::SemanticReadView& view,
                          const semantic::ChangeSet& changes) {
    auto prepared=scene.prepareIncremental(view,changes);
    if(!prepared) {
      if (prepared.error().code != foundation::ErrorCode::kRequiresFullRebuild)
        throw std::runtime_error("prepare incremental projection");
      auto recovered=scene.replace(view);
      if(!recovered)throw std::runtime_error("full projection recovery");
      return;
    }
    scene.publish(std::move(prepared.value()));
  }
  static foundation::Result<RuntimeScene::PreparedPublication> prepareIncremental(
      const RuntimeScene& scene, const semantic::SemanticReadView& view,
      const semantic::ChangeSet& changes) {
    return scene.prepareIncremental(view, changes);
  }
};
}
void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
void lookup(){
  using namespace canvas;
  RuntimeScene scene;
  RuntimeSceneProjection projection;projection.generation=semantic::SemanticGeneration{1};
  for(unsigned i=0;i<4096;++i){RuntimeSceneRecord record;record.objectId=foundation::ObjectId::fromUint64((i*2053)%4096+1);projection.records.push_back(record);}
  const auto expected=projection.records;
  IncrementalRuntimeTestAccess::publish(scene,std::move(projection));
  std::uint64_t comparisons=0;
  for(unsigned i=1;i<=4096;++i){auto* r=scene.find(foundation::ObjectId::fromUint64(i),&comparisons);require(r&&r->objectId==foundation::ObjectId::fromUint64(i),"lookup lost ID");}
  require(!scene.find(foundation::ObjectId::fromUint64(8192),&comparisons),"foreign ID resolves");
  require(std::equal(scene.records().begin(),scene.records().end(),expected.begin()),"lookup index reordered traversal");
  require(comparisons<=4097*14,"RuntimeScene still scans records for every visible ID");
  RuntimeSceneProjection next;next.generation=semantic::SemanticGeneration{2};RuntimeSceneRecord newRecord;newRecord.objectId=foundation::ObjectId::fromUint64(9000);next.records.push_back(newRecord);
  IncrementalRuntimeTestAccess::publish(scene,std::move(next));
  ScenePublicationGate gate;gate.transactionActive=true;gate.previousGeneration=semantic::SemanticGeneration{1};
  IncrementalRuntimeTestAccess::gate(scene,&gate);
  require(scene.find(expected.front().objectId)&&!scene.find(newRecord.objectId),"publication gate leaked new index");
  gate.transactionActive=false;
  require(scene.find(newRecord.objectId)&&!scene.find(expected.front().objectId),"published index is stale");
  std::cout<<"lookup compared IDs="<<comparisons<<" for 4097 lookups\n";
}
// Count the real ObjectStore calls used by RuntimeScene; do not infer work
// from input cardinality or claim this diagnostic has localized projection.
class ObservedStore final : public canvas::semantic::ObjectStore {
  const canvas::semantic::ObjectStore& delegate_;
public:
  mutable std::uint64_t scans=0,copied=0,finds=0;
  explicit ObservedStore(const canvas::semantic::ObjectStore& delegate):delegate_(delegate){}
  std::size_t size()const noexcept override{return delegate_.size();}
  bool contains(const canvas::semantic::ObjectId& id)const noexcept override{return delegate_.contains(id);}
  const canvas::semantic::ObjectRecord* find(const canvas::semantic::ObjectId& id)const noexcept override{++finds;return delegate_.find(id);}
  std::vector<canvas::semantic::ObjectRecord> allObjects()const override{
    auto result=delegate_.allObjects();++scans;copied+=result.size();return result;
  }
  std::vector<canvas::semantic::ObjectRecord> children(const std::optional<canvas::semantic::ObjectId>& id)const override{return delegate_.children(id);}
};
void projectionWork(){
  using namespace canvas;
  for(unsigned population:{1000,10000}){
    semantic::ReferenceObjectStore store;
    for(unsigned i=1;i<=population;++i){
      semantic::ObjectRecord r;r.id=foundation::ObjectId::fromUint64(i);
      r.kind=semantic::ObjectKind::kShape;r.kind_version=1;
      r.content=semantic::ShapeContent{1,10,20};
      require(semantic::internal::ObjectStoreMutator::insertFresh(store,r),"seed store");
    }
    RuntimeScene scene;
    require(bool(scene.replace({store,semantic::SemanticGeneration{1}})),"seed projection");
    auto changed=*store.find(foundation::ObjectId::fromUint64(population));changed.transform.tx=1;
    require(semantic::internal::ObjectStoreMutator::replaceExisting(store,changed),"update store");
    ObservedStore observed(store);
    const auto changes=semantic::ChangeSet::fromChanges(semantic::SemanticGeneration{1},semantic::SemanticGeneration{2},
      {{changed.id,semantic::SemanticChangeFlags::kTransform,{}}});
    IncrementalRuntimeTestAccess::incremental(scene,{observed,semantic::SemanticGeneration{2}},changes);
    require(scene.find(changed.id)&&scene.find(changed.id)->transform.tx==1,"projection dropped change");
    require(observed.scans==0&&observed.copied==0,
      "flat single-object incremental projection still scans ObjectStore");
    const auto full=projectRuntimeScene(store.allObjects(),semantic::SemanticGeneration{2});
    require(std::equal(scene.records().begin(),scene.records().end(),full.records.begin(),full.records.end()),"projection oracle mismatch");
    std::cout<<"{\"population\":"<<population<<",\"changed\":1,\"actual_all_objects_calls\":"<<observed.scans
      <<",\"actual_copied_objects\":"<<observed.copied<<",\"classification\":\"FLAT_INCREMENTAL\"}\n";
  }
}
void duplicatePublication(){
  using namespace canvas;
  RuntimeScene scene;
  RuntimeSceneProjection seed;seed.generation=semantic::SemanticGeneration{1};
  RuntimeSceneRecord r;r.objectId=foundation::ObjectId::fromUint64(7);seed.records.push_back(r);
  require(IncrementalRuntimeTestAccess::tryPublish(scene,seed),"seed publication");
  auto duplicate=seed;duplicate.generation=semantic::SemanticGeneration{2};duplicate.records.push_back(r);
  require(!IncrementalRuntimeTestAccess::tryPublish(scene,std::move(duplicate)),"duplicate ObjectId published ambiguous lookup");
  auto zero=seed;zero.records.front().objectId={};
  require(!IncrementalRuntimeTestAccess::tryPublish(scene,std::move(zero)),"zero ObjectId published");
  require(scene.generation()==seed.generation&&scene.records().size()==1&&scene.find(r.objectId),"rejected publication changed stable state");

  // Resource-bearing ChangeSets do not merge repeated object entries. A
  // repeated create must never publish the same leaf twice.
  semantic::ReferenceObjectStore store;
  RuntimeScene incremental;
  require(bool(incremental.replace({store,semantic::SemanticGeneration{1}})),"seed duplicate changes");
  semantic::ObjectRecord object;object.id=r.objectId;object.kind_version=1;object.content=semantic::ShapeContent{1,10,20};
  require(semantic::internal::ObjectStoreMutator::insertFresh(store,object),"insert duplicate test object");
  const auto changes=semantic::ChangeSet::fromChanges(semantic::SemanticGeneration{1},semantic::SemanticGeneration{2},
    {{object.id,semantic::SemanticChangeFlags::kCreated,{}},{object.id,semantic::SemanticChangeFlags::kProperties,{}}},{},false);
  IncrementalRuntimeTestAccess::incremental(incremental,{store,semantic::SemanticGeneration{2}},changes);
  require(incremental.records().size()==1&&incremental.find(object.id),"repeated change published duplicate leaf");
}
void changes(bool fullAuthority=false){
  using namespace canvas;
  semantic::ReferenceObjectStore store;
  RuntimeScene scene;
  if(fullAuthority)IncrementalRuntimeTestAccess::fullAuthority(scene);
  require(bool(scene.replace({store,semantic::SemanticGeneration{1}})),"empty seed");
  std::uint64_t generation=1;
  unsigned random=42;
  for(unsigned step=0;step<300;++step){
    random=random*1664525+1013904223;
    const auto id=foundation::ObjectId::fromUint64(random%50+1);
    const auto* old=store.find(id);
    semantic::SemanticChangeFlags flag;
    if(!old){semantic::ObjectRecord r;r.id=id;r.kind_version=1;r.content=semantic::ShapeContent{1,10,20};
      r.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(random>>24)});
      require(semantic::internal::ObjectStoreMutator::insertFresh(store,r),"insert");flag=semantic::SemanticChangeFlags::kCreated;
    }else if(step%3==0){require(semantic::internal::ObjectStoreMutator::eraseExisting(store,id),"erase");flag=semantic::SemanticChangeFlags::kDeleted;
    }else{auto r=*old;r.transform.tx=step*.25;
      r.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(step)});
      require(semantic::internal::ObjectStoreMutator::replaceExisting(store,r),"update");flag=semantic::SemanticChangeFlags::kTransform|semantic::SemanticChangeFlags::kPlacement;
    }
    ObservedStore observed(store);
    const auto before=semantic::SemanticGeneration{generation++},after=semantic::SemanticGeneration{generation};
    const auto delta=semantic::ChangeSet::fromChanges(before,after,{{id,flag,{}}});
    IncrementalRuntimeTestAccess::incremental(scene,{observed,after},delta);
    require(observed.scans==0,"flat randomized update scanned canonical store");
    if(fullAuthority){
      const auto oracle=scene::FullSceneCompiler::compile({store,after});
      require(bool(oracle)&&oracle.value().records.size()==scene.records().size(),"full oracle count differs");
      for(std::size_t i=0;i<scene.records().size();++i){
        const auto& a=scene.records()[i];const auto& b=oracle.value().records[i];
        require(a.objectId==b.objectId&&a.content==b.content&&a.placement==b.placement&&a.transform==b.transform&&
          a.geometryBounds==b.geometryBounds&&a.visualBounds==b.visualBounds&&a.worldBounds==b.worldBounds&&
          a.derivedGeometry==b.derivedGeometry&&a.referenceGeometryDigest==b.referenceGeometryDigest&&
          a.directDependencies==b.directDependencies&&a.hierarchyOrder==b.hierarchyOrder,"localized full authority projection differs");
      }
    }else{
      const auto oracle=projectRuntimeScene(store.allObjects(),after);
      require(std::equal(scene.records().begin(),scene.records().end(),oracle.records.begin(),oracle.records.end()),"flat randomized projection differs");
    }
  }
  // A new ancestor/container makes the optimization unsafe. Confirm a full
  // fallback, then change its transform and verify its child/group bounds.
  semantic::ObjectRecord group;group.id=ObjectId::fromUint64(100);group.kind=semantic::ObjectKind::kGroup;group.kind_version=1;group.content=semantic::GroupContent{};
  require(semantic::internal::ObjectStoreMutator::insertFresh(store,group),"group insert");
  auto child=store.allObjects().front();child.placement.parent_id=group.id;
  require(semantic::internal::ObjectStoreMutator::replaceExisting(store,child),"reparent");
  for(unsigned step=0;step<2;++step){
    group.transform.tx=step*100;require(semantic::internal::ObjectStoreMutator::replaceExisting(store,group),"group move");
    ObservedStore observed(store);const auto before=semantic::SemanticGeneration{generation++},after=semantic::SemanticGeneration{generation};
    const auto delta=semantic::ChangeSet::fromChanges(before,after,{{group.id,semantic::SemanticChangeFlags::kTransform,{}},{child.id,semantic::SemanticChangeFlags::kPlacement,{}}});
    IncrementalRuntimeTestAccess::incremental(scene,{observed,after},delta);
    require(observed.scans==1,"hierarchy must keep explicit full dependency fallback");
    if(fullAuthority){
      const auto oracle=scene::FullSceneCompiler::compile({store,after});
      require(bool(oracle)&&oracle.value().records.size()==scene.records().size(),"fallback oracle count differs");
      for(const auto& expected:oracle.value().records){
        const auto* actual=scene.find(expected.objectId);
        require(actual&&actual->transform==expected.transform&&actual->worldBounds==expected.worldBounds&&
          actual->hierarchyOrder==expected.hierarchyOrder&&actual->referenceGeometryDigest==expected.referenceGeometryDigest,"full authority hierarchy fallback differs");
      }
    }else{
      const auto oracle=projectRuntimeScene(store.allObjects(),after);
      require(std::equal(scene.records().begin(),scene.records().end(),oracle.records.begin(),oracle.records.end()),"hierarchical fallback projection differs");
    }
  }
  // Once the last hierarchy edge/container disappears, the next leaf edit
  // can return to local projection without losing its full-compiler basis.
  child.placement.parent_id.reset();
  require(semantic::internal::ObjectStoreMutator::replaceExisting(store,child),"detach child");
  require(semantic::internal::ObjectStoreMutator::eraseExisting(store,group.id),"remove group");
  {
    const auto before=semantic::SemanticGeneration{generation++},after=semantic::SemanticGeneration{generation};
    const auto delta=semantic::ChangeSet::fromChanges(before,after,{{group.id,semantic::SemanticChangeFlags::kDeleted,{}},{child.id,semantic::SemanticChangeFlags::kPlacement,{}}});
    IncrementalRuntimeTestAccess::incremental(scene,{store,after},delta);
  }
  child.transform.ty=42;
  require(semantic::internal::ObjectStoreMutator::replaceExisting(store,child),"move detached leaf");
  {
    ObservedStore observed(store);
    const auto before=semantic::SemanticGeneration{generation++},after=semantic::SemanticGeneration{generation};
    const auto delta=semantic::ChangeSet::fromChanges(before,after,{{child.id,semantic::SemanticChangeFlags::kTransform,{}}});
    IncrementalRuntimeTestAccess::incremental(scene,{observed,after},delta);
    require(observed.scans==0&&scene.find(child.id)->transform.ty==42,"flat scene never returned to local projection");
  }
  // A resource change and an attached Connector are separate non-local
  // dependency domains. Both must use the full evaluator even for one leaf.
  semantic::ObjectRecord connector;connector.id=ObjectId::fromUint64(101);
  connector.kind=semantic::ObjectKind::kConnector;connector.kind_version=1;
  connector.content=semantic::ConnectorContent{
    .start={.value=semantic::AttachedEndpoint{.target_object_id=child.id}},
    .end={.value=semantic::FreePointEndpoint{.point={200,100}}}};
  for(unsigned step=0;step<3;++step){
    std::vector<semantic::ResourceManifestSemanticChange> resources;
    std::vector<semantic::ObjectSemanticChange> objects;
    if(step==0)resources.push_back({semantic::ResourceId{ObjectId::fromUint64(900)},semantic::ResourceManifestChangeFlags::kUpdated,{},{}});
    if(step==1){require(semantic::internal::ObjectStoreMutator::insertFresh(store,connector),"insert connector");objects.push_back({connector.id,semantic::SemanticChangeFlags::kCreated,{}});}
    if(step==2){child.transform.tx=100;require(semantic::internal::ObjectStoreMutator::replaceExisting(store,child),"move connector target");objects.push_back({child.id,semantic::SemanticChangeFlags::kTransform,{}});}
    ObservedStore observed(store);
    const auto before=semantic::SemanticGeneration{generation++},after=semantic::SemanticGeneration{generation};
    const auto delta=semantic::ChangeSet::fromChanges(before,after,std::move(objects),std::move(resources));
    IncrementalRuntimeTestAccess::incremental(scene,{observed,after},delta);
    require(observed.scans==1,"resource/connector dependency skipped full fallback");
    if(fullAuthority){
      const auto oracle=scene::FullSceneCompiler::compile({store,after});
      require(bool(oracle)&&oracle.value().records.size()==scene.records().size(),"dependency fallback count");
      for(const auto& expected:oracle.value().records){const auto* actual=scene.find(expected.objectId);
        require(actual&&actual->derivedGeometry==expected.derivedGeometry&&actual->worldBounds==expected.worldBounds&&actual->directDependencies==expected.directDependencies,"dependency fallback differs from oracle");}
    }else{
      const auto oracle=projectRuntimeScene(store.allObjects(),after);
      require(std::equal(scene.records().begin(),scene.records().end(),oracle.records.begin(),oracle.records.end()),"dependency fallback differs");
    }
  }
  // A flat object is not automatically independent: content that carries a
  // resource must stay on the full projection path until resource reverse
  // dependencies are owned by RuntimeScene.
  {
    semantic::ReferenceObjectStore resourceStore;
    semantic::ObjectRecord image;
    image.id=ObjectId::fromUint64(200);
    image.kind=semantic::ObjectKind::kImage;
    image.kind_version=1;
    image.content=semantic::ImageContent{.resource_id=semantic::ResourceId{ObjectId::fromUint64(900)}};
    require(semantic::internal::ObjectStoreMutator::insertFresh(resourceStore,image),"insert resource image");
    RuntimeScene resourceScene;
    require(bool(resourceScene.replace({resourceStore,semantic::SemanticGeneration{1}})),"seed resource scene");
    image.transform.tx=10;
    require(semantic::internal::ObjectStoreMutator::replaceExisting(resourceStore,image),"update resource image");
    ObservedStore observed(resourceStore);
    const auto delta=semantic::ChangeSet::fromChanges(semantic::SemanticGeneration{1},semantic::SemanticGeneration{2},
      {{image.id,semantic::SemanticChangeFlags::kContent,{}}});
    auto prepared=IncrementalRuntimeTestAccess::prepareIncremental(resourceScene,{observed,semantic::SemanticGeneration{2}},delta);
    require(!prepared && prepared.error().code==foundation::ErrorCode::kRequiresFullRebuild,
      "resource-bearing content did not explicitly request full projection fallback");
    auto recovered=resourceScene.replace({observed,semantic::SemanticGeneration{2}});
    require(bool(recovered)&&observed.scans==1,"resource-bearing content full projection fallback failed");
  }
  // Document-level background state is outside the flat object closure.
  {
    semantic::ReferenceObjectStore backgroundStore;
    RuntimeScene backgroundScene;
    require(bool(backgroundScene.replace({backgroundStore,semantic::SemanticGeneration{1}})),"seed background scene");
    ObservedStore observed(backgroundStore);
    const auto delta=semantic::ChangeSet::fromChanges(semantic::SemanticGeneration{1},semantic::SemanticGeneration{2},
      {},{},true);
    auto prepared=IncrementalRuntimeTestAccess::prepareIncremental(backgroundScene,{observed,semantic::SemanticGeneration{2}},delta);
    require(!prepared && prepared.error().code==foundation::ErrorCode::kRequiresFullRebuild,
      "document background change did not explicitly request full projection fallback");
    auto recovered=backgroundScene.replace({observed,semantic::SemanticGeneration{2}});
    require(bool(recovered)&&observed.scans==1,"document background full projection fallback failed");
  }
  std::cout<<"300 deterministic create/update/delete/reorder changes and hierarchy fallback agree\n";
}
std::vector<std::uint8_t> draw(const canvas::foundation::InkBrushConfig& config,
    const std::vector<canvas::foundation::InkKnot>& knots,float scale,float tx,float ty,bool reference,
    canvas::render::internal::CompactRasterWork& work,unsigned transform=0){
  using namespace canvas;
  render::RasterSkiaSurfaceProvider provider;require(provider.resize(800,500).code==render::BackendSubmissionCode::kAccepted,"resize");
  auto acquired=provider.acquire();auto* canvas=acquired.frame.surface->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
  canvas->translate(tx,ty);canvas->scale(scale,scale);
  if(transform==1){canvas->translate(250,100);canvas->rotate(37);}
  if(transform==2){canvas->translate(300,100);canvas->skew(.35F,-.2F);}
  if(transform==3){canvas->translate(500,100);canvas->scale(-1.F,.8F);}
  if(transform==4){SkMatrix perspective;perspective.setPerspX(.0001F);canvas->concat(perspective);}
  canvas->clipRect(SkRect::MakeLTRB(-20,-20,760,470));
  if(!reference)render::internal::drawCompactStrokeToSkCanvas(*canvas,config,knots,42,&work);
  else if(!knots.empty()){
    // Independent pre-optimization full-clip raster path and original grid.
    const auto clip=canvas->getDeviceClipBounds();const auto matrix=canvas->getTotalMatrix();
    for(int y=clip.top();y<clip.bottom();y+=256)for(int x=clip.left();x<clip.right();x+=256){
      const auto w=std::min(256,clip.right()-x),h=std::min(256,clip.bottom()-y);
      auto tile=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(w,h));require(bool(tile),"reference tile");
      ++work.allocatedTiles;work.allocatedPixels+=std::uint64_t(w)*h;work.compositedPixels+=std::uint64_t(w)*h;
      auto* target=tile->getCanvas();target->clear(SK_ColorBLACK);target->translate(float(-x),float(-y));target->concat(matrix);
      render::internal::drawCompactCoverageToSkCanvas(*target,config,knots);
      canvas->save();canvas->resetMatrix();canvas->clipRect(SkRect::MakeXYWH(float(x),float(y),float(w),float(h)));
      canvas->setMatrix(matrix);render::internal::compositeCompactCoverageToSkCanvas(*canvas,*tile->makeImageSnapshot(),config,42,x,y);canvas->restore();
    }
  }
  provider.release();std::vector<std::uint8_t> pixels(800*500*4);require(provider.readbackRgba(pixels).code==render::BackendSubmissionCode::kAccepted,"readback");return pixels;
}
void raster(){
  using namespace canvas;
  foundation::InkBrushConfig config;config.size=24;config.opacity=.5;
  const std::vector<std::vector<foundation::InkKnot>> paths{{},{{40,40,32768},{80,40,40000}},
    {{10000,10000,10000},{11000,12000,65535}},{{-30,100,32768},{790,100,60000}},
    {{250,250,40000},{260,260,50000}},{{-1e8,230,32768},{1e8,250,32768}}};
  for(unsigned material:{1U,2U})for(auto family:{foundation::InkFamily::kRound,foundation::InkFamily::kMarker,foundation::InkFamily::kChalk,foundation::InkFamily::kCrayon,foundation::InkFamily::kWatercolor}){
    config.materialVersion=material;
    config.materialResourceId=material==1?foundation::kInkMaterialResourceId:foundation::kInkMaterialV2ResourceId;
    config.materialResourceSha256=material==1?foundation::kInkMaterialResourceSha256:foundation::kInkMaterialV2ResourceSha256;
    config.family=family;
    for(float scale:{.5F,1.F,2.F})for(const auto& path:paths)for(unsigned transform=0;transform<5;++transform){
      render::internal::CompactRasterWork a,b;
      auto actual=draw(config,path,scale,3.25F,-2.75F,false,a,transform);
      require(actual==draw(config,path,scale,3.25F,-2.75F,true,b,transform),"clipped coverage changed reference pixels");
      if(family==foundation::InkFamily::kRound&&scale==1.F&&transform==0&&path.size()==2&&path.front().x==40)
        std::cout<<"short stroke: old tiles="<<b.allocatedTiles<<" new tiles="<<a.allocatedTiles
          <<" old shaded pixels="<<b.compositedPixels<<" new shaded pixels="<<a.compositedPixels<<'\n';
      if(path.empty()||(transform<4&&path.front().y>=10000))require(a.allocatedTiles==0,"empty/offscreen coverage allocated tile");
      if(transform==0&&path.size()==2&&path.front().x==40)require(a.allocatedTiles<=1,"short stroke materialized whole viewport");
      if(transform==0&&path.size()==2&&path.front().x==40)require(a.compositedPixels<=20000,"short stroke shades empty tile area");
    }
  }
  std::cout<<"compact culling exact pixels across 900 material-version/family/scale/path/transform cases\n";
}
int main(int argc,char** argv){try{if(argc>1&&std::string(argv[1])=="raster")raster();else if(argc>1&&std::string(argv[1])=="projection")projectionWork();else if(argc>1&&std::string(argv[1])=="changes")changes();else if(argc>1&&std::string(argv[1])=="full-changes")changes(true);else if(argc>1&&std::string(argv[1])=="duplicates")duplicatePublication();else lookup();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}

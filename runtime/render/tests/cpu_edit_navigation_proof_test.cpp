// Offline attribution using saved real root snapshots and existing Scene/source
// builders. This does not manufacture browser timing or mutate a document.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "../src/navigation_pixel_validity.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace canvas;using namespace canvas::render;using nlohmann::json;
namespace{
std::vector<std::uint8_t> read(const char* p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("snapshot input");return{std::istreambuf_iterator<char>(f),{}};}
RuntimeScene loadScene(const char* p){auto d=semantic::SnapshotCodec::decode(read(p),semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!d.ok())throw std::runtime_error("decode");semantic::IndexedObjectStore store;semantic::ResourceManifest manifest;semantic::SemanticGenerationState generation;auto phase=semantic::DocumentRuntimeState::kLoading;const auto& snap=*d.snapshot;if(!semantic::SnapshotBootstrapper::restore(snap,semantic::SnapshotCapability::kAllWithCompactStrokeV1,phase,store,manifest,generation,{snap.document_id,snap.document_revision,snap.recovery_frontier,snap.checkpoint_digest}).restored)throw std::runtime_error("restore");RuntimeScene s;if(!s.replace({store,generation.current()}))throw std::runtime_error("Scene");return s;}
std::string id(foundation::ObjectId value){const char* digits="0123456789abcdef";std::string s;for(auto b:value.bytes){s+=digits[b>>4];s+=digits[b&15];}return s;}
FrameState frame(semantic::SemanticGeneration generation){return{ViewId{1},{{610.333333F,416.666667F},1.5,0,CameraGeneration{1}},{133.333333F,80,1087.333333F,753.333333F},{1431,1010,2862,2020,2,1},generation,SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{1},{0,0,1431,1010}};}
VisibilityResult visible(const RuntimeScene& s,const FrameState& f){VisibilityResult v{f.sceneGeneration,f.sceneReadToken,f.worldViewport};std::vector<const RuntimeSceneRecord*> ordered;for(const auto& r:s.records())if(r.worldBounds.intersects(f.worldViewport))ordered.push_back(&r);std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){const auto ink=[](auto k){return k==semantic::ObjectKind::kVectorStroke||k==semantic::ObjectKind::kDabStroke;};if(ink(a->kind)!=ink(b->kind))return !ink(a->kind);if(a->hierarchyOrder&&b->hierarchyOrder)return a->hierarchyOrder<b->hierarchyOrder;if(a->placement.order_key!=b->placement.order_key)return a->placement.order_key<b->placement.order_key;return a->objectId<b->objectId;});for(auto r:ordered)v.backToFront.push_back(r->objectId);v.visibleRecords=v.backToFront.size();v.candidatesExamined=s.records().size();return v;}
}
int main(int argc,char** argv)try{
 if(argc<3)throw std::runtime_error("before after");auto before=loadScene(argv[1]),after=loadScene(argv[2]);auto f=frame(before.generation());
 auto a=DirectReferenceSource::build(f,visible(before,f),before),b=DirectReferenceSource::buildNavigation(f,visible(after,f),after);
 if(!a||!b)throw std::runtime_error(!a?a.error().message:b.error().message);auto delta=internal::maskPixelDifference(a.value().entries,b.value().entries);json out={{"sameRegion",true},{"beforeCount",a.value().entries.size()},{"afterCount",b.value().entries.size()},{"compatible",delta.compatible}};out["differences"]=json::array();
 for(const auto& x:a.value().entries){auto it=std::find_if(b.value().entries.begin(),b.value().entries.end(),[&](const auto& y){return x.record.objectId==y.record.objectId;});if(it==b.value().entries.end()){out["differences"].push_back({{"id",id(x.record.objectId)},{"missing",true}});continue;}const auto& y=*it;
  const auto& p=x.record;const auto& q=y.record;
  json diff={{"id",id(p.objectId)},{"kind",unsigned(p.kind)},{"content",p.content==q.content},{"command",x.command==y.command},{"masks",p.eraseMasks==q.eraseMasks},{"chunkSpan",x.navigationChunkSpan==y.navigationChunkSpan},{"chunkIndices",x.geometryChunkIndices==y.geometryChunkIndices},{"transform",p.transform==q.transform},{"placement",p.placement==q.placement},{"properties",p.properties==q.properties},{"derived",p.derivedGeometry==q.derivedGeometry},{"bounds",p.geometryBounds==q.geometryBounds&&p.visualBounds==q.visualBounds&&p.worldBounds==q.worldBounds},{"digest",p.referenceGeometryDigest==q.referenceGeometryDigest},{"chunks",p.geometryChunks==q.geometryChunks},{"hierarchy",p.hierarchyOrder==q.hierarchyOrder},{"textLayout",p.textLayout==q.textLayout}};
  if(const auto* c=std::get_if<CompactStrokeReferenceCommand>(&x.command))diff["algorithm"]=c->content.get().stroke.config.algorithmVersion;
  if(x.navigationChunkSpan!=y.navigationChunkSpan||p!=q||x.command!=y.command)out["differences"].push_back(diff);
 }
 auto aligned=b.value().entries;
 for(auto& y:aligned){auto it=std::find_if(a.value().entries.begin(),a.value().entries.end(),[&](const auto& x){return x.record.objectId==y.record.objectId;});if(it!=a.value().entries.end()){y.navigationChunkSpan=it->navigationChunkSpan;y.geometryChunkIndices=it->geometryChunkIndices;y.geometryChunked=it->geometryChunked;}}
 out["alignedHintsCompatible"]=internal::maskPixelDifference(a.value().entries,aligned).compatible;
 std::cout<<out.dump(2)<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

// Private engineering fixture/oracle. Never linked into a consumer SDK.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/scene/scene.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/scene/geometry_chunk.hpp"
#include "../../../../runtime/render/src/chunk_query.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>
#include <algorithm>
#include <optional>
#include <iterator>
#include <stdexcept>
#include <cmath>
using namespace canvas;using Json=nlohmann::json;
namespace {
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point at){return std::chrono::duration<double,std::milli>(Clock::now()-at).count();}
void emit(Json j){std::cout<<j.dump()<<std::endl;}
std::string hex(std::span<const std::uint8_t> data){const char* h="0123456789abcdef";std::string out;for(auto b:data){out+=h[b>>4];out+=h[b&15];}return out;}
foundation::WorldRect bounds(std::size_t i,const std::string& layout){
 const float x=float(i%64)*(layout=="dense"?8:32),y=float((i/64)%(layout=="dense"?64:10000000))*(layout=="dense"?6:32);
 return {x,y,x+12,y+12};
}
semantic::ObjectRecord object(std::size_t i,const std::string& layout){
 semantic::ObjectRecord o;o.id=foundation::ObjectId::fromUint64(i+1);o.kind=semantic::ObjectKind::kShape;o.kind_version=1;
 std::vector<std::uint8_t> key(8);for(unsigned k=0;k<8;++k)key[k]=std::uint8_t((i+1)>>(56-k*8));key.push_back(0x80);o.placement.order_key=semantic::OrderKey(std::move(key));o.content=semantic::ShapeContent{1,12,12};const auto b=bounds(i,layout);o.transform.tx=b.left;o.transform.ty=b.top;return o;
}
semantic::ObjectRecord stroke(std::size_t count,std::size_t ordinal=0){
 auto o=object(ordinal,"sparse");o.kind=semantic::ObjectKind::kVectorStroke;o.kind_version=3;o.transform={};
 semantic::CompactStrokeRecord s;s.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;s.seed=42;s.knots.reserve(count);
 for(std::size_t i=0;i<count;++i)s.knots.push_back({i*.5,100.+ordinal*30.+std::sin(double(i)*.01)*20,32768});
 o.content=semantic::CompactStrokeContent{std::move(s)};return o;
}
std::size_t pointCount(const semantic::ObjectContent& v){return std::visit([](const auto& c)->std::size_t{
 if constexpr(requires{c.stroke.knots;})return c.stroke.knots.size();
 if constexpr(requires{c.stroke.vector_output.outline;})return c.stroke.vector_output.outline.size();
 if constexpr(requires{c.stroke.data;})if(const auto* d=std::get_if<semantic::VectorStrokeData>(&c.stroke.data))return d->samples.size();
 return 0;},v);}
std::optional<std::size_t> visiblePoints(const semantic::SemanticSnapshot& s){
 std::size_t visible=0;
 for(const auto& o:s.objects){
  if(pointCount(o.content)&&o.placement.parent_id)return std::nullopt;
  const auto add=[&](double x,double y){const auto& t=o.transform;const auto px=t.a*x+t.c*y+t.tx,py=t.b*x+t.d*y+t.ty;if(px>=0&&px<800&&py>=0&&py<500)++visible;};
  std::visit([&](const auto& c){
   if constexpr(requires{c.stroke.knots;})for(const auto& p:c.stroke.knots)add(p.x,p.y);
   if constexpr(requires{c.stroke.vector_output.outline;})for(const auto& p:c.stroke.vector_output.outline)add(p.x,p.y);
   if constexpr(requires{c.stroke.data;})if(const auto* d=std::get_if<semantic::VectorStrokeData>(&c.stroke.data))for(const auto& p:d->samples)add(p.position.x,p.position.y);
  },o.content);
 }
 return visible;
}
Json inspect(const std::vector<std::uint8_t>& bytes){
 const auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
 if(!decoded.ok())return {{"stage","inspect"},{"status","rejected"},{"semantic_error",int(decoded.error)}};
 const auto& s=*decoded.snapshot;const auto visible=visiblePoints(s);std::size_t points=0;for(const auto& o:s.objects)points+=pointCount(o.content);
 return {{"stage","inspect"},{"status","completed"},{"scope","semantic-fixture"},{"documentId",hex(s.document_id.value().bytes)},{"objects",s.objects.size()},{"points",points},
  {"visible_points",visible?Json(*visible):Json(nullptr)},{"documentDigest",hex(s.document_digest)},{"checkpointDigest",s.checkpoint_digest},{"documentRevision",std::to_string(s.document_revision)},{"recoveryFrontier",s.recovery_frontier},{"resourceBinding","1"}};
}
void fixture(const std::string& axis,std::size_t count,const std::string& layout,const std::filesystem::path& file,std::size_t pointsPerStroke){
 semantic::SemanticSnapshot s;s.document_id=semantic::DocumentId(foundation::ObjectId::fromUint64(0x9999));s.schema_version=3;s.manifest_present=true;s.manifest.manifest_revision=1;s.document_revision=1;s.digest_algorithm=1;s.recovery_frontier={8,1,16,1,24,1};
 emit({{"stage","fixture_begin"},{"axis",axis},{"requested",count}});auto start=Clock::now();
 if(axis=="points"){for(std::size_t offset=0,ordinal=0;offset<count;++ordinal){const auto n=std::min(pointsPerStroke,count-offset);s.objects.push_back(stroke(n,ordinal));offset+=n;}}else{s.objects.reserve(count);for(std::size_t i=0;i<count;++i){s.objects.push_back(object(i,layout));if(i%250000==249999)emit({{"stage","fixture_construct"},{"objects_constructed",i+1}});}}
 const auto construct=ms(start);emit({{"stage","fixture_bind"},{"objects",s.objects.size()},{"construct_ms",construct}});start=Clock::now();
 if(!semantic::SnapshotCodec::populateRequirements(s)||!semantic::SnapshotCodec::bindDigests(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1)){
  emit({{"stage","fixture_bind"},{"status","rejected"},{"scope","semantic-fixture"},{"objects",s.objects.size()},{"points",axis=="points"?count:0}});return;}
 const auto bind=ms(start);start=Clock::now();auto encoded=semantic::SnapshotCodec::encode(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
 if(!encoded.ok()){emit({{"stage","fixture_encode"},{"status","rejected"},{"semantic_error",int(encoded.error)}});return;}
 auto result=inspect(encoded.bytes);if(result["status"]!="completed"){emit(std::move(result));return;}
 std::filesystem::create_directories(file.parent_path());auto temp=file;temp+=".tmp";{std::ofstream f(temp,std::ios::binary);f.write(reinterpret_cast<const char*>(encoded.bytes.data()),std::streamsize(encoded.bytes.size()));if(!f)throw std::runtime_error("fixture write");}std::filesystem::rename(temp,file);
 std::size_t visibleObjects=0;if(axis=="points"){for(const auto& o:s.objects)if(std::ranges::any_of(std::get<semantic::CompactStrokeContent>(o.content).stroke.knots,[](const auto& p){return p.x>=0&&p.x<800&&p.y>=0&&p.y<500;}))++visibleObjects;}else for(std::size_t i=0;i<count;++i){const auto b=bounds(i,layout);if(b.right>0&&b.left<800&&b.bottom>0&&b.top<500)++visibleObjects;}result["visible_objects"]=visibleObjects;
 result["stage"]="fixture_complete";result["snapshot_bytes"]=encoded.bytes.size();result["construct_ms"]=construct;result["bind_ms"]=bind;result["encode_decode_write_ms"]=ms(start);result["checks"]={{"roundtrip",true}};emit(std::move(result));
}
void component(const std::string& axis,std::size_t count,const std::string& layout){
 const WorldRect region{0,0,800,500};emit({{"stage","component_begin"},{"axis",axis},{"requested",count}});auto start=Clock::now();
 if(axis=="points"){
  auto o=stroke(count);
  if(!semantic::validCompactStroke(std::get<semantic::CompactStrokeContent>(o.content).stroke)){
   emit({{"stage","component_admission"},{"status","rejected"},{"scope","point-projection"},{"objects",1},{"points",count},{"reason","invalid_compact_stroke"}});return;
  }
  auto projection=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});auto& r=projection.records.front();r.geometryIndex=scene::buildGeometryChunkIndex(r);
  const auto projectMs=ms(start);start=Clock::now();render::internal::ChunkQueryWork work;std::vector<std::uint32_t> actual,expected;
  render::internal::queryChunks(r,region,0,work,[&](const auto& c){actual.push_back(c.index);});const auto queryMs=ms(start);
  for(const auto& c:std::as_const(r.geometryChunks)){const auto& b=c.bounds;if(b.right>region.left&&b.left<region.right&&b.bottom>region.top&&b.top<region.bottom)expected.push_back(c.index);}
  std::size_t visible=0;for(const auto& p:std::get<semantic::CompactStrokeContent>(o.content).stroke.knots)if(p.x>=0&&p.x<800&&p.y>=0&&p.y<500)++visible;
  bool covers=true;std::size_t pointIndex=0;
  for(const auto& p:std::get<semantic::CompactStrokeContent>(o.content).stroke.knots){
   if(p.x>=0&&p.x<800&&p.y>=0&&p.y<500){bool found=false;for(const auto id:actual){const auto& c=std::as_const(r.geometryChunks)[id];found|=pointIndex>=c.pointOffset&&pointIndex<c.pointOffset+c.pointCount;}covers&=found;}++pointIndex;
  }
  emit({{"stage","component_complete"},{"status","completed"},{"scope","point-projection"},{"axis",axis},{"objects",1},{"points",count},{"visible_points",visible},{"project_ms",projectMs},{"query_ms",queryMs},{"chunk_candidates",work.candidates},{"index_nodes",work.nodes},{"index_bytes",r.geometryIndex?r.geometryIndex->bytes():0},{"cache_used_bytes",nullptr},{"checks",{{"linear_parity",actual==expected},{"visible_point_coverage",covers}}}});return;
 }
 std::vector<SceneRecord> records;records.reserve(count);std::vector<foundation::ObjectId> expected;
 for(std::size_t i=0;i<count;++i){SceneRecord r;r.objectId=ObjectId::fromUint64(i+1);r.orderKey=SceneOrderKey(i+1);r.kind=SceneObjectKind::kShape;r.flags=SceneRecordFlags::kVisible;r.worldBounds=bounds(i,layout);r.contentRevision=ContentRevision{1};r.renderPayload={std::uint32_t(i+1),1};r.hitGeometry={std::uint32_t(i+1),1};if(r.worldBounds.right>0&&r.worldBounds.left<800&&r.worldBounds.bottom>0&&r.worldBounds.top<500)expected.push_back(r.objectId);records.push_back(std::move(r));}
 auto index=std::make_unique<UniformGridSpatialIndex>();auto* observer=index.get();Scene scene(std::make_unique<DirectRenderScene>(),std::move(index));
 auto replaced=scene.replace({SceneRevision{1},std::move(records)});if(!replaced)throw std::runtime_error("Scene replace");const auto loadMs=ms(start);const auto old=scene.read();start=Clock::now();auto q=scene.query({region});if(!q)throw std::runtime_error("Scene query");const auto queryMs=ms(start);
 auto actual=q.value().backToFront;
 std::sort(actual.begin(),actual.end());std::sort(expected.begin(),expected.end());
 emit({{"stage","component_query"},{"status","completed"},{"scope","scene-index"},{"objects",count},{"points",0},{"visible_objects",actual.size()},{"load_ms",loadMs},{"query_ms",queryMs},{"index_bytes",observer->diagnostics().estimatedBytes},{"cache_used_bytes",nullptr},{"checks",{{"linear_parity",actual==expected}}}});
 const auto before=old.records().front();auto after=before;after.worldBounds.left+=2048;after.worldBounds.right+=2048;start=Clock::now();auto changed=scene.apply({SceneRevision{1},SceneRevision{2},{{SceneMutationKind::kUpdate,before.objectId,before,after}},std::nullopt});
 const auto updateMs=ms(start);auto updatedQuery=scene.query({region});std::vector<ObjectId> updated=updatedQuery?updatedQuery.value().backToFront:std::vector<ObjectId>{};std::sort(updated.begin(),updated.end());auto expectedAfter=expected;std::erase(expectedAfter,before.objectId);
 const auto* retained=old.find(before.objectId);const auto* current=scene.read().find(before.objectId);
 emit({{"stage","component_complete"},{"status","completed"},{"scope","scene-index"},{"axis",axis},{"objects",count},{"points",0},{"visible_objects",actual.size()},{"visible_points",0},{"load_ms",loadMs},{"query_ms",queryMs},{"update_ms",updateMs},{"index_bytes",observer->diagnostics().estimatedBytes},{"cache_used_bytes",nullptr},{"checks",{{"linear_parity",actual==expected},{"update",bool(changed)&&current&&current->worldBounds==after.worldBounds},{"old_reader",retained&&retained->worldBounds==before.worldBounds},{"updated_query",bool(updatedQuery)&&updated==expectedAfter}}}});
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="inspect"){std::ifstream in(argv[2],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};emit(inspect(bytes));return 0;}
 if(argc<5)throw std::runtime_error("usage: fixture|component objects|points count sparse|dense [snapshot-path [points-per-stroke]]");
 const std::string mode=argv[1],axis=argv[2],layout=argv[4];const auto count=std::stoull(argv[3]);
 if(!count||count>1000000||(axis!="objects"&&axis!="points")||(layout!="sparse"&&layout!="dense"))throw std::runtime_error("invalid profile");
 if(mode=="component")component(axis,count,layout);else if(mode=="fixture"&&(argc==6||(argc==7&&axis=="points"))){const auto perStroke=argc==7?std::stoull(argv[6]):count;if(!perStroke)throw std::runtime_error("invalid points per stroke");fixture(axis,count,layout,argv[5],perStroke);}else throw std::runtime_error("invalid mode");
}catch(const std::exception& e){emit({{"stage","exception"},{"status","failed"},{"error",e.what()}});return 2;}}

// Offline verification: canonical snapshot + existing shared SnapResolver.
// No candidate pixels or live Runtime state participate in the pose prediction.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/semantic/table_content.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/interaction/viewport_constraints.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
using namespace canvas;
using nlohmann::json;
std::string idHex(foundation::ObjectId id){std::ostringstream s;for(auto b:id.bytes)s<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(b);return s.str();}
foundation::WorldRect translated(foundation::WorldRect b,double x,double y){return{float(b.left+x),float(b.top+y),float(b.right+x),float(b.bottom+y)};}
bool intersects(foundation::WorldRect a,foundation::WorldRect b){return a.intersects(b);}
json rectangle(foundation::WorldRect b){return{b.left,b.top,b.right,b.bottom};}
json candidate(const std::optional<interaction::SnapCandidate>& c){if(!c)return nullptr;return{{"target",idHex(c->targetObjectId)},{"sourceFeature",unsigned(c->sourceFeature)},{"targetFeature",unsigned(c->targetFeature)},{"correction",c->correction}};}
int main(int argc,char**argv){try{
 if(argc!=2&&argc!=3)throw std::runtime_error("usage committed_snapshot [known-sticky-80-30]");
 std::ifstream in(argv[1],std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(in),{}};
 auto decoded=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
 if(!decoded.ok())throw std::runtime_error("canonical decode");
 auto snapshot=std::move(*decoded.snapshot);
 if(argc==3){if(std::string(argv[2])!="known-sticky-80-30")throw std::runtime_error("foreign clone");for(auto&r:snapshot.objects)if(r.kind==semantic::ObjectKind::kSticky){r.transform.tx+=80;r.transform.ty+=30;}}
 const auto projected=projectRuntimeScene(snapshot.objects,semantic::SemanticGeneration{1});
 const RuntimeSceneRecord* table=nullptr;for(const auto&r:projected.records)if(r.kind==semantic::ObjectKind::kTable)table=&r;
 if(!table)throw std::runtime_error("table missing");
 const auto& t=std::get<semantic::TableContent>(table->content.get());
 interaction::SnapResolver snap;json steps=json::array();
 for(const auto delta:{std::array<double,2>{8,4},std::array<double,2>{40,16},std::array<double,2>{80,30}}){
  const auto source=translated(table->visualBounds,table->transform.tx+delta[0],table->transform.ty+delta[1]);
  const float radius=interaction::SnapResolver::kEngageThresholdLogicalPx;
  const foundation::WorldRect query{source.left-radius,source.top-radius,source.right+radius,source.bottom+radius};
  std::vector<interaction::SnapTarget>targets;json targetRows=json::array();
  for(const auto&r:projected.records){if(r.objectId==table->objectId||!intersects(r.worldBounds,query))continue;
   // These frozen mixed fixtures have no derived-appearance target in this
   // query. Fail closed rather than silently substitute the wrong bounds.
   for(const auto&p:r.properties.entries)if(p.field_id==0x102U||p.field_id==0x103U)throw std::runtime_error("derived target requires exact interactionBounds adapter");
   targets.push_back({r.objectId,r.worldBounds,false,false});
   targetRows.push_back({{"id",idHex(r.objectId)},{"kind",unsigned(r.kind)},{"worldBounds",rectangle(r.worldBounds)}});
  }
  const auto resolved=snap.resolve({source},targets,1.0F);
  const double dx=delta[0]+(resolved.x?resolved.x->correction:0),dy=delta[1]+(resolved.y?resolved.y->correction:0);
  steps.push_back({{"inputDelta",delta},{"sourceBounds",rectangle(source)},{"query",rectangle(query)},{"targets",targetRows},{"x",candidate(resolved.x)},{"y",candidate(resolved.y)},{"effectiveDelta",{dx,dy}},{"transform",{table->transform.a,table->transform.b,table->transform.c,table->transform.d,table->transform.tx+dx,table->transform.ty+dy}}});
 }
 json objects=json::array();for(const auto&r:projected.records)if(r.kind!=semantic::ObjectKind::kVectorStroke)objects.push_back({{"id",idHex(r.objectId)},{"kind",unsigned(r.kind)},{"transform",{r.transform.a,r.transform.b,r.transform.c,r.transform.d,r.transform.tx,r.transform.ty}},{"worldBounds",rectangle(r.worldBounds)}});
 json out{{"status","CANONICAL_SNAP_REFERENCE"},{"table",{{"id",idHex(table->objectId)},{"width",semantic::tableWidth(t)},{"height",semantic::tableHeight(t)},{"visualBounds",rectangle(table->visualBounds)},{"transform",{table->transform.a,table->transform.b,table->transform.c,table->transform.d,table->transform.tx,table->transform.ty}}}},{"steps",steps},{"objects",objects},{"scope","Canonical SnapshotCodec -> projectRuntimeScene bounds -> existing SnapResolver; same three raw pointer deltas, camera1/0/0; no candidate pixels. Only frozen simple target bounds; spatial AABB filter shares exact accepted bounds, no extra hit geometry."}};
 std::cout<<out.dump(2)<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

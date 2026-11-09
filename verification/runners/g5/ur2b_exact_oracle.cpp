// Private, uninstalled batch decision oracle. Freeze old binary before routing.
// The candidate build uses the same DTO but never supplies oracle geometry.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/skia_renderer.hpp"
#if defined(UR2B_CANDIDATE_QUERY)
#include "../../../runtime/render/src/skia_prepared_geometry.hpp"
#endif
#include "../../../runtime/render/src/selection_region_geometry_internal.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <cmath>
#include <limits>
using namespace canvas;
using nlohmann::json;
namespace {
json read(const std::filesystem::path& p){std::ifstream f(p);if(!f)throw std::runtime_error("missing JSON");json v;f>>v;return v;}
void write(const std::filesystem::path& p,const json& v){std::ofstream f(p);f<<v.dump(2)<<'\n';if(!f)throw std::runtime_error("write JSON");}
std::string hex(foundation::ObjectId id){std::ostringstream s;for(auto b:id.bytes)s<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return s.str();}
semantic::SemanticSnapshot decode(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("snapshot missing");std::vector<std::uint8_t> b{std::istreambuf_iterator<char>(f),{}};auto r=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!r.ok())throw std::runtime_error("snapshot decode rejected");return std::move(*r.snapshot);}
double number(const json& j){const auto v=j.get<double>();if(!std::isfinite(v))throw std::runtime_error("nonfinite DTO");return v;}
semantic::EraseMaskGeometry mask(const json& j){
 if(j.at("kind")=="filled_path"){
  semantic::FilledPathMask m;
  for(const auto& c:j.at("commands")){const auto op=c.at("op").get<std::string>();
   if(op=="move")m.path.commands.push_back(semantic::MoveTo{{number(c.at("x")),number(c.at("y"))}});
   else if(op=="line")m.path.commands.push_back(semantic::LineTo{{number(c.at("x")),number(c.at("y"))}});
   else if(op=="close")m.path.commands.push_back(semantic::ClosePath{});
   else throw std::runtime_error("unknown mask command");}
  return m;
 }
 if(j.at("kind")=="swept_circle"){
  semantic::SweptCircleMask m;
  for(const auto& s:j.at("segments")){const auto& a=s.at("p0");const auto& b=s.at("p1");const auto& c=s.at("c1");const auto& d=s.at("c2");m.segments.push_back({{{number(a[0]),number(a[1])},number(a[2])},{{number(b[0]),number(b[1])},number(b[2])},{number(c[0]),number(c[1])},{number(d[0]),number(d[1])}});}
  return m;
 }
 throw std::runtime_error("unknown mask kind");
}
json box(double x,double y,double w,double h){return {{"kind","filled_path"},{"commands",json::array({{{"op","move"},{"x",x},{"y",y}},{{"op","line"},{"x",x+w},{"y",y}},{{"op","line"},{"x",x+w},{"y",y+h}},{{"op","line"},{"x",x},{"y",y+h}},{{"op","close"}}})}};}
void generate(const std::filesystem::path& base,const std::filesystem::path& dir){
 std::filesystem::create_directories(dir);auto templateSnapshot=decode(base);
 const auto found=std::find_if(templateSnapshot.objects.begin(),templateSnapshot.objects.end(),[](const auto& o){return std::holds_alternative<semantic::CompactStrokeContent>(o.content);});
 if(found==templateSnapshot.objects.end())throw std::runtime_error("template compact absent");const auto prototype=*found;
 json requests=json::array();
 for(const auto layout:{"small","sparse","dense"})for(const unsigned copies:{1U,8U,16U,32U}){
  if(std::string(layout)=="small"&&copies!=1)continue;
  auto s=templateSnapshot;s.objects.clear();
  for(unsigned i=0;i<copies;++i){auto o=prototype;o.id=foundation::ObjectId::fromUint64(i+1);o.erase_masks.clear();o.transform={};o.placement.parent_id={};o.placement.order_key=semantic::OrderKey({static_cast<std::uint8_t>(i+1)});
   auto c=std::get<semantic::CompactStrokeContent>(o.content);c.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;c.stroke.config.size=20;c.stroke.knots.clear();
   const auto count=std::string(layout)=="small"?73U:7199U;
   for(unsigned k=0;k<count;++k){const bool dense=std::string(layout)!="sparse";c.stroke.knots.push_back({dense?100+65*std::sin(k*.075):k*.2,dense?100+50*std::cos(k*.11):100+25*std::sin(k*.023),std::uint16_t(2000+(k*173)%62000)});}
   o.content=std::move(c);s.objects.push_back(std::move(o));}
  if(!semantic::SnapshotCodec::populateRequirements(s)||!semantic::SnapshotCodec::bindDigests(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1))throw std::runtime_error("fixture bind");
  auto encoded=semantic::SnapshotCodec::encode(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!encoded.ok())throw std::runtime_error("fixture encode");
  const auto file=dir/(std::string(layout)+"-"+std::to_string(copies)+".axsnap");{std::ofstream f(file,std::ios::binary);f.write(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());}
  json qs=json::array();
  for(unsigned n=0;n<(std::string(layout)=="small"?64U:5U);++n)for(unsigned i=0;i<copies;++i){const auto h=std::string(layout)=="small"?n%11:std::array{0U,1U,2U,5U,10U}[n];
   json histories=json::array();for(unsigned k=0;k<h;++k)histories.push_back(box(60+(k*17+n*7)%75,60+(k*11+n*13)%75,10+k%9,12+k%7));
   const auto q=std::string(layout)=="small"?box(65+(n*19)%80,65+(n*23)%80,n%3?.02:20,n%4?.03:15):box(100,95,5,10);
   qs.push_back({{"id",std::to_string(n)+"-"+std::to_string(i)},{"object_id",hex(s.objects[i].id)},{"mask",q},{"history",histories},{"history_ids","verification-owned per query, no document mutation"}});}
  requests.push_back({{"schema","g5-ur2b-exact-query-v1"},{"snapshot",file.string()},{"snapshot_sha256","BOUND_BY_RUNNER_BEFORE_FREEZE"},{"queries",qs},{"layout",layout},{"copies",copies}});
 }
 write(dir/"requests-unbound.json",requests);
}
json batch(const json& request){
 if(request.at("schema")!="g5-ur2b-exact-query-v1"||request.at("snapshot_sha256").get<std::string>().size()!=64)throw std::runtime_error("unbound request");
 auto snapshot=decode(request.at("snapshot").get<std::string>());json rows=json::array();
#if defined(UR2B_CANDIDATE_QUERY)
 render::internal::PreparedGeometryCache prepared;render::internal::EraseMaskPathCache queryCache;render::RenderCacheOptions options;auto budget=std::make_shared<resources::ResourceBudgetManager>();
#endif
 for(const auto& q:request.at("queries")){
  const auto found=std::find_if(snapshot.objects.begin(),snapshot.objects.end(),[&](const auto& o){return hex(o.id)==q.at("object_id").get<std::string>();});if(found==snapshot.objects.end())throw std::runtime_error("object id missing");
  RuntimeSceneRecord r;r.objectId=found->id;r.kind=found->kind;r.kindVersion=found->kind_version;r.transform=found->transform;r.content=found->content;r.eraseMasks=found->erase_masks;r.properties=found->properties;
  if(q.contains("history")){r.eraseMasks.clear();unsigned i=0;for(const auto& h:q.at("history"))r.eraseMasks.push_back({foundation::ObjectId::fromUint64(0x10000+ ++i),mask(h)});}
  const auto fault=q.value("fault",std::string{});
  if(fault=="nonfinite_source")r.content.update([](auto& v){std::get<semantic::CompactStrokeContent>(v).stroke.knots.back().x=std::numeric_limits<double>::quiet_NaN();});
  else if(fault=="invalid_history"){auto m=mask(box(0,0,20,20));std::get<semantic::MoveTo>(std::get<semantic::FilledPathMask>(m).path.commands.front()).point.x=std::numeric_limits<double>::quiet_NaN();r.eraseMasks.push_back({foundation::ObjectId::fromUint64(0xdead),m});}
  else if(!fault.empty())throw std::runtime_error("unknown fault");
  const auto m=mask(q.at("mask"));const auto start=std::chrono::steady_clock::now();
#if defined(UR2B_CANDIDATE_QUERY)
  const auto hit=queryCache.query(r,m,budget,8*1024*1024,&prepared,&options);
#else
  const auto hit=render::internal::intersectEraseGeometryComponentOracle(r,m);
#endif
  rows.push_back({{"id",q.at("id")},{"object_id",q.at("object_id")},{"decision",unsigned(hit)},{"elapsed_us",std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count()}});
 }
 return {{"schema","g5-ur2b-exact-query-result-v1"},{"snapshot_sha256",request.at("snapshot_sha256")},{"rows",rows}};
}
}
int main(int argc,char** argv){try{if(argc==4&&std::string(argv[1])=="--generate"){generate(argv[2],argv[3]);return 0;}if(argc!=3)throw std::runtime_error("usage: requests.json output.json");const auto requests=read(argv[1]);json out=json::array();for(const auto& r:requests)out.push_back(batch(r));write(argv[2],out);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

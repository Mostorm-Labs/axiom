// Private offline workload proof. Geometry is delegated to the unchanged
// Runtime coordinate conversion / partialEraseGeometry and the pinned old
// uncached exact-query executable. This helper emits requests, never Operations.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"
#include "../../../runtime/interaction/src/partial_erase_geometry.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <cmath>
using namespace canvas;using nlohmann::json;
namespace{
json read(const std::filesystem::path& p){std::ifstream f(p);json j;f>>j;return j;}
void write(const std::filesystem::path& p,const json& j){std::ofstream f(p);f<<j.dump(2)<<'\n';if(!f)throw std::runtime_error("write failed");}
std::string hex(foundation::ObjectId id){std::ostringstream s;for(auto b:id.bytes)s<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return s.str();}
json maskJson(const semantic::EraseMaskGeometry& g){
 if(const auto* s=std::get_if<semantic::SweptCircleMask>(&g)){json out={{"kind","swept_circle"},{"segments",json::array()}};for(const auto& c:s->segments)out["segments"].push_back({{"p0",{c.p0.position.x,c.p0.position.y,c.p0.radius}},{"p1",{c.p1.position.x,c.p1.position.y,c.p1.radius}},{"c1",{c.control1.x,c.control1.y}},{"c2",{c.control2.x,c.control2.y}}});return out;}
 json out={{"kind","filled_path"},{"commands",json::array()}};
 for(const auto& c:std::get<semantic::FilledPathMask>(g).path.commands)std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,semantic::MoveTo>)out["commands"].push_back({{"op","move"},{"x",v.point.x},{"y",v.point.y}});
  else if constexpr(std::is_same_v<T,semantic::LineTo>)out["commands"].push_back({{"op","line"},{"x",v.end.x},{"y",v.end.y}});
  else if constexpr(std::is_same_v<T,semantic::ClosePath>)out["commands"].push_back({{"op","close"}});
  else throw std::runtime_error("pinned old DTO does not support this filled primitive");},c);
 return out;
}
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("usage: bound-request.json output.json");const auto in=read(argv[1]);if(in.at("schema")!="g5-transient-trajectory-request-v1")throw std::runtime_error("schema");
 const auto file=in.at("snapshot").get<std::string>();std::ifstream f(file,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);if(!decoded.ok())throw std::runtime_error("snapshot decode");
 const auto& camera=in.at("camera");interaction::ViewportInteractionController viewport;if(!viewport.setCamera(camera.at("scale").get<float>(),camera.at("x").get<float>(),camera.at("y").get<float>()))throw std::runtime_error("camera");
 std::vector<foundation::WorldPoint> world;for(const auto& p:in.at("view_points")){const auto v=viewport.viewToContent(p[0].get<float>(),p[1].get<float>());world.push_back({v.first,v.second});}
 const auto radius=in.at("radius_world").get<double>();
 if(world.empty()||!std::isfinite(radius)||radius<=0||radius>16.0)throw std::runtime_error("owner radius/trace mismatch");
 const auto prior=in.value("prior",json::object());json request={{"schema","g5-ur2b-exact-query-v1"},{"snapshot",file},{"snapshot_sha256",in.at("snapshot_sha256")},{"queries",json::array()}};json basis={{"schema","g5-transient-trajectory-basis-v1"},{"camera",camera},{"DPR",in.at("dpr")},{"radius_world",radius},{"world_points",json::array()},{"objects",json::array()}};
 for(const auto& p:world)basis["world_points"].push_back({p.x,p.y});
 for(const auto& o:decoded.snapshot->objects){if(o.kind!=semantic::ObjectKind::kVectorStroke&&o.kind!=semantic::ObjectKind::kDabStroke)continue;const auto id=hex(o.id);const auto g=interaction::partialEraseGeometry(world,o.transform,radius);if(!g)throw std::runtime_error("geometry");json history=json::array();for(const auto& h:o.erase_masks)history.push_back(maskJson(h.geometry));if(prior.contains(id))for(const auto& h:prior.at(id))history.push_back(h);
  request["queries"].push_back({{"id",id},{"object_id",id},{"mask",maskJson(*g)},{"history",history}});basis["objects"].push_back({{"id",id},{"transform",{o.transform.a,o.transform.b,o.transform.c,o.transform.d,o.transform.tx,o.transform.ty}},{"history_count",history.size()}});
 }
 if(request["queries"].size()!=16)throw std::runtime_error("fixture must have16ink objects");write(argv[2],{{"request",request},{"basis",basis}});return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

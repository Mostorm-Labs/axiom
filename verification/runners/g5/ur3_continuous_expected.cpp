// Immutable expected-state clones from confirmed public input, verification only.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace canvas;using nlohmann::json;
void need(bool b){if(!b)std::exit(2);}
semantic::SemanticSnapshot read(const char*p){std::ifstream f(p,std::ios::binary);std::vector<std::uint8_t>b{std::istreambuf_iterator<char>(f),{}};auto d=semantic::SnapshotCodec::decode(b,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(d.ok());return std::move(*d.snapshot);}
std::string hex(foundation::ObjectId id){std::string s;for(auto b:id.bytes){s+="0123456789abcdef"[b>>4];s+="0123456789abcdef"[b&15];}return s;}
int main(int argc,char**argv){need(argc==5);auto snapshot=read(argv[1]);std::ifstream f(argv[2]);const json request=json::parse(f);const std::string mode=argv[3];
 if(mode=="affine"){
  const auto id=request.at("selected").at("primary").get<std::string>();const auto matrix=request.at("expectedTransform").get<std::array<double,6>>();bool found=false;
  for(auto&o:snapshot.objects)if(hex(o.id)==id){found=true;o.transform={matrix[0],matrix[1],matrix[2],matrix[3],matrix[4],matrix[5]};}need(found);
 }else{
  auto loaded=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);need(bool(loaded));
  if(request.contains("config")){
   const auto&c=request["config"];auto& v=*loaded.package.compactConfig;
   v.version=c.at("version");v.family=static_cast<foundation::InkFamily>(c.at("family").get<unsigned>());
   v.roundMode=static_cast<foundation::InkRoundMode>(c.at("roundMode").get<unsigned>());
   v.pressureSource=static_cast<foundation::InkPressureSource>(c.at("pressureSource").get<unsigned>());
   v.size=c.at("size");v.opacity=c.at("opacity");v.color=c.at("color").get<std::array<double,4>>();
   v.thinning=c.at("thinning");v.streamline=c.at("streamline");v.smoothing=c.at("smoothing");
   v.pressureModel=static_cast<foundation::InkPressureModel>(c.at("pressureModel").get<unsigned>());
   v.easing=static_cast<decltype(v.easing)>(c.at("easing").get<unsigned>());
   v.startTaper=c.at("startTaper");v.endTaper=c.at("endTaper");v.startCap=c.at("startCap");v.endCap=c.at("endCap");
   need(v.algorithmVersion==4&&v.family==foundation::InkFamily::kRound);
  }
  ink::BrushSession session{1,ink::resolveBrushState(loaded.package,0x4501)};need(session.begin());ink::BrushPreviewDelta delta;unsigned n=0;
  const auto count=request.value("count",request.at("samples").size());
  for(const auto&s:request.at("samples")){if(n>=count)break;++n;const bool terminal=s.at("phase")=="up";
   const auto time=s.at("timestampNs").is_string()?std::stoull(s.at("timestampNs").get<std::string>()):s.at("timestampNs").get<std::uint64_t>();
   ink::BrushSample point{static_cast<float>(s.at("x").get<double>()),static_cast<float>(s.at("y").get<double>()),.5,false,n,time,false,terminal};need(session.append(std::span(&point,1),{},delta)==ink::BrushSessionError::kNone);}
  std::vector<foundation::InkKnot> knots;
  if(mode=="live"){
   std::vector<const ink::BrushPreviewDelta::Chunk*> parts;for(auto p=delta.compactChunks.get();p;p=p->previous.get())parts.push_back(p);
   for(auto i=parts.rbegin();i!=parts.rend();++i)for(auto k:(*i)->knots)if(knots.empty()||knots.back()!=k)knots.push_back(k);
  }else{ink::BrushCommitIntent intent;need(session.seal(intent)==ink::BrushSessionError::kNone);knots=intent.knots;}
  if(mode=="verify-seal"){
   unsigned matches=0;json diagnostic=json::array();for(const auto&o:snapshot.objects)if(const auto*v=std::get_if<semantic::CompactStrokeContent>(&o.content)){if(v->stroke.knots==knots&&v->stroke.config==*loaded.package.compactConfig)++matches;if(v->stroke.knots.size()<1000){std::size_t first=0;while(first<std::min(v->stroke.knots.size(),knots.size())&&v->stroke.knots[first]==knots[first])++first;diagnostic.push_back({{"id",hex(o.id)},{"actual_knots",v->stroke.knots.size()},{"config_equal",v->stroke.config==*loaded.package.compactConfig},{"knots_equal",v->stroke.knots==knots},{"first_difference",first}});}}
   std::cout<<json{{"status",matches==1?"QUALIFIED":"MISMATCH"},{"confirmed_samples",count},{"replay_knots",knots.size()},{"exact_canonical_matches",matches},{"diagnostic",diagnostic}}<<'\n';return matches==1?0:3;
  }
  if(request.value("onlyNewInk",false))snapshot.objects.clear();
  semantic::ObjectRecord object;object.id=foundation::ObjectId::fromUint64(0xf00d);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
  object.placement.order_key=semantic::OrderKey({255,255,255});semantic::CompactStrokeContent content;content.stroke.config=*loaded.package.compactConfig;
  if(mode=="live"){content.stroke.config.color={1,.62,.04,1};content.stroke.config.opacity=.92;}
  content.stroke.seed=0x4501;content.stroke.knots=std::move(knots);object.content=std::move(content);snapshot.objects.push_back(std::move(object));
 }
 need(semantic::SnapshotCodec::populateRequirements(snapshot));need(semantic::SnapshotCodec::bindDigests(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1));auto encoded=semantic::SnapshotCodec::encode(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(encoded.ok());std::ofstream out(argv[4],std::ios::binary);out.write((const char*)encoded.bytes.data(),encoded.bytes.size());std::cout<<json{{"status","EXPECTED_CLONE_ENCODED"},{"mode",mode},{"objects",snapshot.objects.size()}}<<'\n';
}

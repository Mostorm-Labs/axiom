// Offline, non-installed qualification consumer. Decodes opaque snapshots and
// evaluates canonical geometry directly; never reads candidate cache pixels.
#include "canvas/semantic/snapshot.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/ink/brush_session.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/scene/scene_types.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPathUtils.h"
#include "../../../../runtime/render/src/compact_contour_skia_path.hpp"
#include "../../../../runtime/render/src/skia_geometry_internal.hpp"
#include "../../../../runtime/interaction/src/partial_erase_geometry.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <cmath>
using nlohmann::json;
using namespace canvas;
namespace {
std::string hex(foundation::ObjectId id){std::ostringstream o;for(auto b:id.bytes)o<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return o.str();}
json readJson(const char* p){std::ifstream f(p);if(!f)throw std::runtime_error("JSON file missing");json j;f>>j;return j;}
std::optional<semantic::SemanticSnapshot> decode(const char* p,json& out){
 std::ifstream f(p,std::ios::binary);if(!f)throw std::runtime_error("snapshot file missing");
 std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(f),{}};
 auto result=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
 out["decode_error"]=static_cast<unsigned>(result.error);out["byte_length"]=bytes.size();return std::move(result.snapshot);
}
RuntimeSceneRecord record(const semantic::ObjectRecord& r){RuntimeSceneRecord v;v.objectId=r.id;v.kind=r.kind;v.kindVersion=r.kind_version;v.transform=r.transform;v.content=r.content;v.eraseMasks=r.erase_masks;v.placement=r.placement;v.properties=r.properties;return v;}
json pixelReference(semantic::SemanticSnapshot snapshot,const json& request,const std::string& prefix){
 const auto width=request.at("width").get<unsigned>(),height=request.at("height").get<unsigned>();
 const double logicalWidth=request.value("logical_width",800.),logicalHeight=request.value("logical_height",500.);
 if(!width||!height||std::uint64_t(width)*height>16777216||logicalWidth<=0||logicalHeight<=0)throw std::runtime_error("invalid oracle dimensions");
 const auto camera=request.at("camera");const double scale=camera.at("scale"),tx=camera.at("x"),ty=camera.at("y");
 if(!std::isfinite(scale)||scale<=0||!std::isfinite(tx)||!std::isfinite(ty))throw std::runtime_error("invalid oracle camera");
 for(auto& r:snapshot.objects){
  if(r.kind!=semantic::ObjectKind::kShape&&!std::holds_alternative<semantic::CompactStrokeContent>(r.content))throw std::runtime_error("pixel oracle fixture kind unsupported");
  if(const auto* compact=std::get_if<semantic::CompactStrokeContent>(&r.content);compact&&(compact->stroke.config.family!=foundation::InkFamily::kRound||compact->stroke.config.algorithmVersion!=4))throw std::runtime_error("pixel oracle frozen fixture requires round-handdrawn v4");
 }
 // Only expected live presentation receives the confirmed eraser trace. It
 // does not mutate the public snapshot or any production document/Operation.
 if(request.contains("live_erase")){
  const auto live=request.at("live_erase");std::vector<foundation::WorldPoint> trace;
  for(const auto& p:live.at("view_points"))trace.push_back({float((p[0].get<double>()-tx)/scale),float((p[1].get<double>()-ty)/scale)});
  for(auto& r:snapshot.objects)if(std::holds_alternative<semantic::CompactStrokeContent>(r.content)){
   const auto mask=interaction::partialEraseGeometry(trace,r.transform,live.at("diameter").get<double>()*.5);
   if(!mask)throw std::runtime_error("independent live mask invalid");r.erase_masks.push_back({foundation::ObjectId::fromUint64(0x7755),*mask});
  }
  // bindDigests here only seals the oracle-owned expected presentation clone;
  // the original public snapshot bytes are preserved and independently hashed.
  if(!semantic::SnapshotCodec::bindDigests(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1))throw std::runtime_error("oracle expected-state digest");
 }
 semantic::IndexedObjectStore store;semantic::DocumentRuntimeState phase=semantic::DocumentRuntimeState::kLoading;
 semantic::SemanticGenerationState generation{semantic::SemanticGeneration{snapshot.document_revision}};
 semantic::ResourceManifest manifest;
 const semantic::SnapshotBootstrapper::CheckpointBinding checkpoint{snapshot.document_id,snapshot.document_revision,snapshot.recovery_frontier,snapshot.checkpoint_digest};
 const auto restored=semantic::SnapshotBootstrapper::restore(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1,phase,store,manifest,generation,checkpoint);
 if(!restored.restored)throw std::runtime_error("oracle snapshot restore rejected");
 const semantic::SemanticReadView view(store,generation.current());RuntimeScene scene;
 if(!scene.replace(view))throw std::runtime_error("oracle full Scene projection rejected");
 const render::FrameState frame{render::ViewId{1},render::CameraState{{float((logicalWidth*.5-tx)/scale),float((logicalHeight*.5-ty)/scale)},float(scale),0,render::CameraGeneration{1}},
  {float(-tx/scale),float(-ty/scale),float((logicalWidth-tx)/scale),float((logicalHeight-ty)/scale)},
  render::SurfaceMetrics{float(logicalWidth),float(logicalHeight),width,height,float(width/logicalWidth),1},generation.current(),SceneRevision{snapshot.document_revision},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{0,0,float(logicalWidth),float(logicalHeight)}};
 render::VisibilityResult visibility{generation.current(),frame.sceneReadToken,frame.worldViewport};
 std::vector<RuntimeSceneRecord> ordered(scene.records().begin(),scene.records().end());
 std::sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.hierarchyOrder<b.hierarchyOrder;});
 for(const auto& r:ordered)visibility.backToFront.push_back(r.objectId);
 visibility.visibleRecords=visibility.backToFront.size();visibility.candidatesExamined=visibility.visibleRecords;
 auto refs=render::DirectReferenceSource::build(frame,visibility,scene);if(!refs)throw std::runtime_error("oracle direct reference rejected");
 const auto info=SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB());
 auto surface=SkSurfaces::Raster(info),objectSurface=SkSurfaces::Raster(info);if(!surface||!objectSurface)throw std::runtime_error("oracle raster allocation");
 const auto plan=render::FramePlanBuilder::build(frame,refs.value(),snapshot.background,false,true);if(!plan)throw std::runtime_error("oracle FramePlan invalid");
 if(render::internal::drawReferencePlanToSkCanvas(*surface->getCanvas(),plan.value(),nullptr,nullptr,true,nullptr).code!=render::BackendSubmissionCode::kAccepted)throw std::runtime_error("uncached CPU oracle draw rejected");
 std::vector<std::uint8_t> rgba(std::size_t(width)*height*4),one(rgba.size()),edge(std::size_t(width)*height,0);
 auto boundarySurface=SkSurfaces::Raster(SkImageInfo::MakeA8(width,height));if(!boundarySurface)throw std::runtime_error("oracle geometric boundary allocation");
 boundarySurface->getCanvas()->clear(SK_ColorTRANSPARENT);
 if(!surface->readPixels(info,rgba.data(),width*4,0,0))throw std::runtime_error("oracle readback failed");
 for(const auto& entry:refs.value().entries){
  if(const auto* compact=std::get_if<render::CompactStrokeReferenceCommand>(&entry.command)){
   const auto& transform=entry.record.transform;const auto& w=refs.value().worldToView;
   auto matrix=SkMatrix::Scale(float(width/logicalWidth),float(height/logicalHeight));
   matrix.preConcat(SkMatrix::MakeAll(w.a,w.c,w.tx,w.b,w.d,w.ty,0,0,1));
   matrix.preConcat(SkMatrix::MakeAll(transform.a,transform.c,transform.tx,transform.b,transform.d,transform.ty,0,0,1));
   SkPaint boundary;boundary.setColor(SK_ColorWHITE);boundary.setStyle(SkPaint::kStroke_Style);boundary.setStrokeWidth(2);boundary.setAntiAlias(true);
   const auto draw=[&](const SkPath& path){boundarySurface->getCanvas()->drawPath(path.makeTransform(matrix),boundary);};
   const auto& stroke=compact->content.get().stroke;const auto contour=ink::compileCompactContour(stroke.config,stroke.knots);
   for(std::size_t i=0;i+1<contour.patches.size();++i)draw(render::internal::compactContourRibbon(std::span(contour.patches).subspan(i,2)));
   for(const auto& cap:contour.caps){SkPathBuilder path;path.addCircle(float(cap.x),float(cap.y),float(cap.radius));draw(path.detach());}
   for(const auto& mask:entry.record.eraseMasks){
    if(const auto* f=std::get_if<semantic::FilledPathMask>(&mask.geometry))draw(render::internal::semanticPath(f->path));
    else if(const auto* swept=std::get_if<semantic::SweptCircleMask>(&mask.geometry))for(const auto& part:swept->segments){
     SkPathBuilder path;path.moveTo(part.p0.position.x,part.p0.position.y);path.cubicTo(part.control1.x,part.control1.y,part.control2.x,part.control2.y,part.p1.position.x,part.p1.position.y);
     SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(2*std::max(part.p0.radius,part.p1.radius));p.setStrokeCap(SkPaint::kRound_Cap);draw(skpathutils::FillPathWithPaint(path.detach(),p));
    }
   }
  }
  auto only=refs.value();only.entries={entry};only.canonicalBytes.clear();only.digest.clear();
  const auto objectPlan=render::FramePlanBuilder::build(frame,only,{},true,true);if(!objectPlan)throw std::runtime_error("oracle object plan");
  if(render::internal::drawReferencePlanToSkCanvas(*objectSurface->getCanvas(),objectPlan.value(),nullptr,nullptr,true,nullptr).code!=render::BackendSubmissionCode::kAccepted||!objectSurface->readPixels(info,one.data(),width*4,0,0))throw std::runtime_error("oracle geometry edge draw");
  for(unsigned y=1;y+1<height;++y)for(unsigned x=1;x+1<width;++x){const auto index=std::size_t(y)*width+x;const auto alpha=one[index*4+3];
   if(alpha!=one[(index-1)*4+3]||alpha!=one[(index+1)*4+3]||alpha!=one[(index-width)*4+3]||alpha!=one[(index+width)*4+3]){
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)edge[std::size_t(int(y)+dy)*width+int(x)+dx]=1;
   }
  }
 }
 std::vector<std::uint8_t> geometric(edge.size());if(!boundarySurface->readPixels(SkImageInfo::MakeA8(width,height),geometric.data(),width,0,0))throw std::runtime_error("oracle geometric boundary readback");
 for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||geometric[i];
 const auto writeBytes=[](const std::string& path,const auto& bytes){std::ofstream file(path,std::ios::binary);file.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!file)throw std::runtime_error("oracle output failed");};
 writeBytes(prefix+".rgba",rgba);writeBytes(prefix+".edge",edge);
 return {{"status","REFERENCE_RENDERED"},{"method","accepted public snapshot -> independent full Scene/reference -> CPU Raster Direct with null prepared/cache"},{"width",width},{"height",height},{"rgba_bytes",rgba.size()},{"edge_bytes",edge.size()},{"objects",snapshot.objects.size()},{"boundary","accepted per-primitive two-device-pixel contour/mask boundary plus independent per-object alpha edges; never candidate difference"},{"private_frame_ids","oracle-local only; not production receipts"}};
}
}
int main(int argc,char** argv){json out{{"schema","g5-ur0b-fixture-oracle-v1"},{"method","offline SnapshotCodec and uncached canonical exact geometry"}};
 try{
  if(argc<2)throw std::runtime_error("usage: snapshot [literal-fixture actions initial-state]");
  if(std::string(argv[1])=="--pixels"){
   if(argc!=5)throw std::runtime_error("usage: --pixels snapshot request-json output-prefix");
   auto snapshot=decode(argv[2],out);if(!snapshot){out["status"]="INVALID_SNAPSHOT";std::cout<<out.dump(2)<<'\n';return 2;}
   auto result=pixelReference(std::move(*snapshot),readJson(argv[3]),argv[4]);std::cout<<result.dump(2)<<'\n';return 0;
  }
  auto snapshot=decode(argv[1],out);if(!snapshot){out["status"]="INVALID_SNAPSHOT";std::cout<<out.dump(2)<<'\n';return 2;}
  out["document_revision"]=std::to_string(snapshot->document_revision);out["object_count"]=snapshot->objects.size();out["objects"]=json::array();
  bool bodies=true;const semantic::CompactStrokeRecord* original=nullptr;
  for(const auto& r:snapshot->objects){
   const auto* compact=std::get_if<semantic::CompactStrokeContent>(&r.content);
   json item{{"id",hex(r.id)},{"kind",static_cast<unsigned>(r.kind)},{"kind_version",r.kind_version},{"transform",{r.transform.a,r.transform.b,r.transform.c,r.transform.d,r.transform.tx,r.transform.ty}},{"mask_count",r.erase_masks.size()}};
   if(compact){const auto& s=compact->stroke;if(!original)original=&s;
    bodies=bodies&&(s==*original);
    const auto& c=s.config;
    item["seed"]=std::to_string(s.seed);item["knots"]=s.knots.size();
    item["config"]={{"version",c.version},{"algorithmVersion",c.algorithmVersion},{"signalVersion",c.signalVersion},{"materialVersion",c.materialVersion},{"family",static_cast<unsigned>(c.family)},{"roundMode",static_cast<unsigned>(c.roundMode)},{"pressureSource",static_cast<unsigned>(c.pressureSource)},{"pressureModel",static_cast<unsigned>(c.pressureModel)},{"size",c.size},{"opacity",c.opacity},{"color",c.color},{"thinning",c.thinning},{"smoothing",c.smoothing},{"streamline",c.streamline},{"startCap",c.startCap},{"endCap",c.endCap},{"startTaper",c.startTaper},{"endTaper",c.endTaper},{"easing",static_cast<unsigned>(c.easing)},{"markerAngle",c.markerAngle},{"markerFlatness",c.markerFlatness},{"density",c.density},{"textureScale",c.textureScale},{"deposition",c.deposition},{"wetEdge",c.wetEdge},{"laserFadeMs",c.laserFadeMs},{"laserTailLength",c.laserTailLength},{"materialResourceId",c.materialResourceId},{"materialResourceSha256",c.materialResourceSha256},{"materialSamplingVersion",c.materialSamplingVersion}};
   }else bodies=false;
   out["objects"].push_back(std::move(item));
  }
  out["copy_bodies_equal"]=bodies;
  if(argc>=5&&original){
   const auto fixture=readJson(argv[2]),actions=readJson(argv[3]),state=readJson(argv[4]);
   auto loaded=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);
   if(!loaded)throw std::runtime_error("pinned catalog unavailable");
   // Use the immutable actual snapshot config: replay does not replace or
   // round an authored config merely to match a catalog presentation default.
   loaded.package.compactConfig=original->config;
   ink::BrushSession replay{1,ink::resolveBrushState(loaded.package,original->seed)};
   if(!replay.begin())throw std::runtime_error("replay begin");
   ink::BrushPreviewDelta preview;std::uint64_t count=0;
   for(const auto& s:fixture["samples"]){
    ink::BrushSample point{double(float(s[0].get<double>()/1000)),double(float(s[1].get<double>()/1000)),.5,false,++count,std::stoull(s[2].get<std::string>()),false,s[4]=="up"};
    if(replay.append(std::span(&point,1),{},preview)!=ink::BrushSessionError::kNone)throw std::runtime_error("replay append");
   }
   ink::BrushCommitIntent sealed;if(replay.seal(sealed)!=ink::BrushSessionError::kNone)throw std::runtime_error("replay seal");
   out["input_confirmed_samples"]=count;out["replay_knots"]=sealed.knots.size();out["replay_matches_snapshot"]=sealed.knots==original->knots;
   double maxPosition=0;std::size_t pressureDifferences=0;json firstDifferences=json::array();
   for(std::size_t i=0;i<std::min(sealed.knots.size(),original->knots.size());++i){const auto& a=sealed.knots[i];const auto& b=original->knots[i];
    maxPosition=std::max({maxPosition,std::abs(a.x-b.x),std::abs(a.y-b.y)});pressureDifferences+=a.pressure!=b.pressure;
    if(a!=b&&firstDifferences.size()<5)firstDifferences.push_back({{"index",i},{"replay",{a.x,a.y,double(a.pressure)}},{"snapshot",{b.x,b.y,double(b.pressure)}}});
   }
   out["replay_diagnostics"]={{"max_position_delta",maxPosition},{"pressure_differences",pressureDifferences},{"first_differences",firstDifferences}};
   if(argc>5&&std::string(argv[5])=="replay-only"){out["status"]="DIAGNOSTIC_ONLY";std::cout<<out.dump(2)<<'\n';return 0;}
   const auto camera=state["state"]["camera"];const double scale=camera["scale"],tx=camera["x"],ty=camera["y"];
   std::vector<RuntimeSceneRecord> records;for(const auto& r:snapshot->objects)records.push_back(record(r));
   const auto selected=state["selection"]["primary"].get<std::string>();
   json stages=json::array();
   for(const auto& interaction:{"E0","E1"}){
    std::vector<foundation::WorldPoint> trace;
    for(const auto& e:actions["plans"]["masked_drag"]["events"])
     if(e["interaction_id"]==interaction&&e["kind"]=="pointer")trace.push_back({float((e["x_milli"].get<double>()/1000-tx)/scale),float((e["y_milli"].get<double>()/1000-ty)/scale)});
    json targets=json::array();
    for(auto& r:records){const auto mask=interaction::partialEraseGeometry(trace,r.transform,16);
     if(!mask)throw std::runtime_error("offline mask construction");
     const auto hit=render::intersectEraseGeometry(r,*mask);
     if(hit==render::SelectionGeometryHit::kInvalid||hit==render::SelectionGeometryHit::kUnsupported)throw std::runtime_error("uncached exact erase oracle invalid");
     if(hit==render::SelectionGeometryHit::kHit){targets.push_back(hex(r.objectId));r.eraseMasks.push_back({foundation::ObjectId::fromUint64(stages.size()+1),*mask});}
    }
    stages.push_back({{"interaction",interaction},{"exact_targets",targets}});
   }
   out["erase_exact_queries"]=stages;
   const float x=float((400-tx)/scale),y=float((120-ty)/scale),epsilon=.0005F;
   const foundation::WorldPoint region[]{{x-epsilon,y-epsilon},{x+epsilon,y-epsilon},{x+epsilon,y+epsilon},{x-epsilon,y+epsilon}};
   json hits=json::array();bool selectedHit=false;
   for(const auto& r:records){const auto h=render::intersectVisibleSelectionGeometry(r,region);
    if(h==render::SelectionGeometryHit::kInvalid||h==render::SelectionGeometryHit::kUnsupported)throw std::runtime_error("fixed point oracle invalid");
    if(h==render::SelectionGeometryHit::kHit){hits.push_back(hex(r.objectId));selectedHit|=hex(r.objectId)==selected;}
   }
   out["fixed_drag"]={{"logical_point",{400,120}},{"world_point",{x,y}},{"selected",selected},{"selected_visible_body_hit_after_E0_E1",selectedHit},{"exact_hits",hits},{"adaptive_search",false}};
   out["status"]=bodies&&sealed.knots==original->knots&&selectedHit&&count==7200&&snapshot->objects.size()==16?"QUALIFIED":"TASK_PACKAGE_DEFECT";
  }else out["status"]="DECODED";
  std::cout<<out.dump(2)<<'\n';return out["status"]=="TASK_PACKAGE_DEFECT"?3:0;
 }catch(const std::exception& error){out["status"]="ORACLE_ERROR";out["error"]=error.what();std::cout<<out.dump(2)<<'\n';return 2;}
}

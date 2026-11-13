// Independent canonical OperationEngine replay and uncached CPU pixels. The
// candidate pending projection, cache, scheduler and prepared jobs are absent.
#include "canvas/semantic/snapshot.hpp"
#include "cpu_reference_boundary.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/snapshot_bootstrap.hpp"
#include "canvas/semantic/operation_engine.hpp"
#include "canvas/semantic/applied_operation_ledger.hpp"
#include "canvas/semantic/indexed_object_store.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/render/editing_overlay.hpp"
#include "canvas/interaction/viewport_constraints.hpp"
#include "canvas/runtime/resource_provider.hpp"
#include "canvas/scene/scene_types.hpp"
#include "canvas/ink/compact_geometry.hpp"
#include "../../../runtime/render/src/compact_contour_skia_path.hpp"
#include "../../../runtime/render/src/skia_geometry_internal.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkPathUtils.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>
using namespace canvas;using nlohmann::json;
namespace {
void need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<std::uint8_t> read(const std::string& file){std::ifstream stream(file,std::ios::binary);need(bool(stream),"reference file");return {std::istreambuf_iterator<char>(stream),{}};}
void write(const std::string& file,std::span<const std::uint8_t> bytes){std::ofstream stream(file,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());need(bool(stream),"reference write");}
std::string hex(foundation::ObjectId id){std::ostringstream out;for(auto b:id.bytes)out<<std::hex<<std::setfill('0')<<std::setw(2)<<unsigned(b);return out.str();}
}
int main(int argc,char** argv)try{
 need(argc==3,"cpu reference request output-prefix");const auto request=json::parse(read(argv[1]));const std::string prefix=argv[2];
 auto decoded=semantic::SnapshotCodec::decode(read(request.at("snapshot")),semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(decoded.ok(),"reference Snapshot decode");auto snapshot=std::move(*decoded.snapshot);
 semantic::IndexedObjectStore store;semantic::ResourceManifest manifest;semantic::SemanticGenerationState generation;auto phase=semantic::DocumentRuntimeState::kLoading;
 need(semantic::SnapshotBootstrapper::restore(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1,phase,store,manifest,generation,{snapshot.document_id,snapshot.document_revision,snapshot.recovery_frontier,snapshot.checkpoint_digest}).restored,"reference restore");
 semantic::AppliedOperationLedger ledger;semantic::CanonicalCommitClock clock{semantic::RuntimeEpoch{1}};semantic::OperationEngine engine;
 for(const auto& file:request.value("operations",json::array())){const auto operation=semantic::SemanticCodec::decodeProtobufOperation(read(file));need(operation.ok(),"reference Operation decode");need(engine.apply(operation.operation,semantic::ApplySource::kRemoteSync,store,manifest,ledger,generation,clock).disposition==semantic::ApplyDisposition::kApplied,"reference ordered apply");}
 if(request.contains("preview_pose")){
  const auto& n=request.at("preview_pose");const auto idText=n.at("id").get<std::string>();foundation::ObjectId id;for(unsigned i=0;i<16;++i)id.bytes[i]=std::stoul(idText.substr(i*2,2),nullptr,16);const auto values=n.at("pose").get<std::array<double,6>>();semantic::Transform2D pose{values[0],values[1],values[2],values[3],values[4],values[5]};semantic::Operation change{semantic::OperationId{foundation::ObjectId::fromUint64(0x7fffffa2)},snapshot.document_id,1,1,semantic::SetTransformsOp{{{id,pose}}}};need(engine.apply(change,semantic::ApplySource::kRemoteSync,store,manifest,ledger,generation,clock).disposition==semantic::ApplyDisposition::kApplied,"reference preview sequential pose");
 }
 if(request.contains("negative_pose")){
  const auto& n=request.at("negative_pose");const auto idText=n.at("id").get<std::string>();foundation::ObjectId id;for(unsigned i=0;i<16;++i)id.bytes[i]=std::stoul(idText.substr(i*2,2),nullptr,16);const auto* object=store.find(id);need(object!=nullptr,"negative pose object");auto pose=object->transform;pose.tx+=n.at("dx").get<double>();pose.ty+=n.value("dy",0.0);semantic::Operation change{semantic::OperationId{foundation::ObjectId::fromUint64(0x7fffffa1)},snapshot.document_id,1,1,semantic::SetTransformsOp{{{id,pose}}}};need(engine.apply(change,semantic::ApplySource::kRemoteSync,store,manifest,ledger,generation,clock).disposition==semantic::ApplyDisposition::kApplied,"negative pose original mutation");
 }
 snapshot.objects=store.allObjects();snapshot.manifest=manifest;
 if(request.contains("document_revision"))snapshot.document_revision=request.at("document_revision").get<std::uint64_t>();
 need(semantic::SnapshotCodec::populateRequirements(snapshot)&&semantic::SnapshotCodec::bindDigests(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1),"reference digest");const auto saved=semantic::SnapshotCodec::encode(snapshot,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(saved.ok(),"reference encode");write(prefix+".axsnap",saved.bytes);
 if(request.value("semantic_only",false)){std::cout<<json{{"status","REFERENCE_REPLAYED"},{"objects",store.size()},{"snapshot_bytes",saved.bytes.size()}}.dump()<<'\n';return 0;}
 const unsigned width=request.at("width"),height=request.at("height");const double lw=request.value("logical_width",1431.),lh=request.value("logical_height",1010.);need(width&&height&&std::uint64_t(width)*height<=24000000,"reference dimensions");
 const auto camera=request.at("camera");const double zoom=camera.at("scale"),tx=camera.at("x"),ty=camera.at("y");need(std::isfinite(zoom)&&zoom>0,"reference camera");
 RuntimeScene scene;need(bool(scene.replace({store,generation.current()})),"reference full Scene");
 const render::FrameState frame{render::ViewId{1},render::CameraState{{float((lw*.5-tx)/zoom),float((lh*.5-ty)/zoom)},float(zoom),0,render::CameraGeneration{1}},{float(-tx/zoom),float(-ty/zoom),float((lw-tx)/zoom),float((lh-ty)/zoom)},{float(lw),float(lh),width,height,float(width/lw),1},generation.current(),SceneRevision{1},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{0,0,float(lw),float(lh)}};
 render::VisibilityResult visibility{generation.current(),frame.sceneReadToken,frame.worldViewport};std::vector<const RuntimeSceneRecord*> ordered;for(const auto& r:scene.records())ordered.push_back(&r);
 std::sort(ordered.begin(),ordered.end(),[](auto a,auto b){const auto ink=[](auto k){return k==semantic::ObjectKind::kVectorStroke||k==semantic::ObjectKind::kDabStroke;};if(ink(a->kind)!=ink(b->kind))return !ink(a->kind);if(a->hierarchyOrder&&b->hierarchyOrder)return a->hierarchyOrder<b->hierarchyOrder;if(a->placement.order_key!=b->placement.order_key)return a->placement.order_key<b->placement.order_key;return a->objectId<b->objectId;});
 for(auto r:ordered)visibility.backToFront.push_back(r->objectId);visibility.visibleRecords=visibility.backToFront.size();visibility.candidatesExamined=visibility.visibleRecords;
 auto references=render::DirectReferenceSource::build(frame,visibility,scene);need(bool(references),"reference source");
 runtime::MemoryResourceProvider resources;for(const auto& r:request.value("resources",json::array())){const auto idText=r.at("id").get<std::string>();foundation::ObjectId id;need(idText.size()==32,"resource id");for(unsigned i=0;i<16;++i)id.bytes[i]=std::stoul(idText.substr(i*2,2),nullptr,16);resources.publish(id,read(r.at("path")));}render::ImageResourceResolver images(resources);
 const auto plan=render::FramePlanBuilder::build(frame,references.value(),snapshot.background,false,true);need(bool(plan),"reference plan");const auto info=SkImageInfo::Make(width,height,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB());auto surface=SkSurfaces::Raster(info),oneSurface=SkSurfaces::Raster(info),boundarySurface=SkSurfaces::Raster(SkImageInfo::MakeA8(width,height));need(bool(surface)&&bool(oneSurface)&&bool(boundarySurface),"reference allocation");boundarySurface->getCanvas()->clear(SK_ColorTRANSPARENT);
 need(render::internal::drawReferencePlanToSkCanvas(*surface->getCanvas(),plan.value(),&images,nullptr,true,nullptr).code==render::BackendSubmissionCode::kAccepted,"uncached reference draw");std::vector<std::uint8_t> pixels(std::size_t(width)*height*4),one(pixels.size()),edge(std::size_t(width)*height);need(surface->readPixels(info,pixels.data(),width*4,0,0),"reference readback");
 for(const auto& entry:references.value().entries){
  {
   const auto& t=entry.record.transform;const auto& w=references.value().worldToView;auto matrix=SkMatrix::Scale(width/lw,height/lh);matrix.preConcat(SkMatrix::MakeAll(w.a,w.c,w.tx,w.b,w.d,w.ty,0,0,1));matrix.preConcat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));SkPaint edgePaint;edgePaint.setColor(SK_ColorWHITE);edgePaint.setStyle(SkPaint::kStroke_Style);edgePaint.setStrokeWidth(2);edgePaint.setAntiAlias(true);
   render::testing::shapeMaterialBoundaries(entry,[&](const SkPath& p){boundarySurface->getCanvas()->drawPath(p.makeTransform(matrix),edgePaint);});
  }
  if(const auto* compact=std::get_if<render::CompactStrokeReferenceCommand>(&entry.command)){
   const auto& t=entry.record.transform;const auto& w=references.value().worldToView;auto matrix=SkMatrix::Scale(width/lw,height/lh);matrix.preConcat(SkMatrix::MakeAll(w.a,w.c,w.tx,w.b,w.d,w.ty,0,0,1));matrix.preConcat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));SkPaint boundary;boundary.setColor(SK_ColorWHITE);boundary.setStyle(SkPaint::kStroke_Style);boundary.setStrokeWidth(2);boundary.setAntiAlias(true);const auto draw=[&](const SkPath& path){boundarySurface->getCanvas()->drawPath(path.makeTransform(matrix),boundary);};
   const auto& stroke=compact->content.get().stroke;const auto contour=ink::compileCompactContour(stroke.config,stroke.knots);for(std::size_t i=0;i+1<contour.patches.size();++i)draw(render::internal::compactContourRibbon(std::span(contour.patches).subspan(i,2)));for(const auto& cap:contour.caps){SkPathBuilder p;p.addCircle(cap.x,cap.y,cap.radius);draw(p.detach());}
   for(const auto& mask:entry.record.eraseMasks)if(const auto* f=std::get_if<semantic::FilledPathMask>(&mask.geometry))draw(render::internal::semanticPath(f->path));else for(const auto& part:std::get<semantic::SweptCircleMask>(mask.geometry).segments){SkPathBuilder p;p.moveTo(part.p0.position.x,part.p0.position.y);p.cubicTo(part.control1.x,part.control1.y,part.control2.x,part.control2.y,part.p1.position.x,part.p1.position.y);SkPaint paint;paint.setStyle(SkPaint::kStroke_Style);paint.setStrokeWidth(2*std::max(part.p0.radius,part.p1.radius));paint.setStrokeCap(SkPaint::kRound_Cap);draw(skpathutils::FillPathWithPaint(p.detach(),paint));}
  }
  auto isolated=references.value();isolated.entries={entry};isolated.canonicalBytes.clear();isolated.digest.clear();const auto p=render::FramePlanBuilder::build(frame,isolated,{},true,true);need(bool(p),"reference object plan");need(render::internal::drawReferencePlanToSkCanvas(*oneSurface->getCanvas(),p.value(),&images,nullptr,true,nullptr).code==render::BackendSubmissionCode::kAccepted,"reference object draw");need(oneSurface->readPixels(info,one.data(),width*4,0,0),"reference object readback");
  for(unsigned y=1;y+1<height;++y)for(unsigned x=1;x+1<width;++x){const auto i=std::size_t(y)*width+x;const auto alpha=one[i*4+3];if(alpha!=one[(i-1)*4+3]||alpha!=one[(i+1)*4+3]||alpha!=one[(i-width)*4+3]||alpha!=one[(i+width)*4+3])for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)edge[std::size_t(int(y)+dy)*width+int(x)+dx]=1;}
 }
 std::vector<std::uint8_t> geometric(edge.size());need(boundarySurface->readPixels(SkImageInfo::MakeA8(width,height),geometric.data(),width,0,0),"boundary readback");for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||geometric[i];write(prefix+".rgba",pixels);write(prefix+".edge",edge);
 std::vector<std::uint8_t> hud(std::size_t(width)*height);
 if(request.contains("hud_object")){
  const auto idText=request.at("hud_object").get<std::string>();const auto found=std::find_if(references.value().entries.begin(),references.value().entries.end(),[&](const auto& e){return hex(e.record.objectId)==idText;});need(found!=references.value().entries.end(),"HUD reference target");
  const auto& record=found->record;render::EditingOverlay overlay(frame);const auto& bounds=record.visualBounds;const auto& w=references.value().worldToView;std::array<render::AttachmentPointGeometry,4> ports{};std::span<const render::AttachmentPointGeometry> attachments;
  const bool hasPorts=record.kind==semantic::ObjectKind::kShape||record.kind==semantic::ObjectKind::kSticky||record.kind==semantic::ObjectKind::kImage;
  if(hasPorts){const std::array<foundation::WorldPoint,4> local{{{(bounds.left+bounds.right)*.5F,bounds.top},{bounds.right,(bounds.top+bounds.bottom)*.5F},{(bounds.left+bounds.right)*.5F,bounds.bottom},{bounds.left,(bounds.top+bounds.bottom)*.5F}}};for(unsigned n=0;n<4;++n){const auto& t=record.transform;const auto p=local[n];ports[n]={{float((t.a*p.x+t.c*p.y+t.tx)*w.a+w.tx),float((t.b*p.x+t.d*p.y+t.ty)*w.d+w.ty)},true,false};}attachments=ports;}
  std::vector<render::SnapGuideGeometry> guides;
  if(request.contains("snap_source")){const auto values=request.at("snap_source").get<std::array<float,4>>();const foundation::WorldRect source{values[0],values[1],values[2],values[3]};std::vector<interaction::SnapTarget> targets;for(const auto& target:references.value().entries){if(target.record.objectId==record.objectId)continue;const auto& b=target.record.worldBounds;if(b.right<source.left-6||b.left>source.right+6||b.bottom<source.top-6||b.top>source.bottom+6)continue;targets.push_back({target.record.objectId,{float(b.left*zoom+tx),float(b.top*zoom+ty),float(b.right*zoom+tx),float(b.bottom*zoom+ty)},false,false});}interaction::SnapResolver resolver;const auto snap=resolver.resolve({{float(source.left*zoom+tx),float(source.top*zoom+ty),float(source.right*zoom+tx),float(source.bottom*zoom+ty)}},targets,1.0F);for(const auto* g:{&snap.xGuide,&snap.yGuide})if(g->has_value()){const auto& v=**g;guides.push_back({v.axis==interaction::SnapGuideAxis::kVertical?render::SnapGuideAxis::kVertical:render::SnapGuideAxis::kHorizontal,v.axis==interaction::SnapGuideAxis::kVertical?render::ScreenPoint{v.position,v.spanStart}:render::ScreenPoint{v.spanStart,v.position},v.axis==interaction::SnapGuideAxis::kVertical?render::ScreenPoint{v.position,v.spanEnd}:render::ScreenPoint{v.spanEnd,v.position}});}}
  need(overlay.update(render::EditingOverlayInput{record.objectId,bounds,record.transform,true,false,true,{true,true,true},guides,attachments,hasPorts?60.0F:32.0F}),"reference HUD geometry");auto chrome=SkSurfaces::Raster(info);chrome->getCanvas()->clear(SK_ColorTRANSPARENT);render::internal::drawEditingOverlayToSkCanvas(*chrome->getCanvas(),frame.metrics,overlay);std::vector<std::uint8_t> rgbaHud(pixels.size());need(chrome->readPixels(info,rgbaHud.data(),width*4,0,0),"reference HUD readback");for(std::size_t i=0;i<hud.size();++i)hud[i]=rgbaHud[i*4+3]?1:0;const auto rotation=overlay.selectionOutline().rotationAnchor;const auto radius=7.0F;for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto px=(x+.5)/(width/lw),py=(y+.5)/(height/lh);if(std::hypot(px-rotation.x,py-rotation.y)<=radius)hud[std::size_t(y)*width+x]=1;}
  // 1.25px AA chrome only, use original public7px outline band to include its
  // antialias fringe and original controls. No object interior exclusion.
  auto expectedBounds=record.worldBounds;const std::array<double,4> screen{{expectedBounds.left*zoom+tx,expectedBounds.top*zoom+ty,expectedBounds.right*zoom+tx,expectedBounds.bottom*zoom+ty}};
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto px=x/(width/lw),py=y/(height/lh);const auto[l,t,r,b]=screen;if((px>=l-7&&px<=r+7&&std::min(std::abs(py-t),std::abs(py-b))<=7)||(py>=t-7&&py<=b+7&&std::min(std::abs(px-l),std::abs(px-r))<=7))hud[std::size_t(y)*width+x]=1;}
 }
 write(prefix+".hud",hud);
 json objects=json::array();for(const auto& o:snapshot.objects)objects.push_back({{"id",hex(o.id)},{"kind",unsigned(o.kind)},{"pose",{o.transform.a,o.transform.b,o.transform.c,o.transform.d,o.transform.tx,o.transform.ty}},{"masks",o.erase_masks.size()}});
 std::cout<<json{{"status","REFERENCE_RENDERED"},{"method","original OperationEngine sequential apply/full Scene/uncached CPU mapper/null prepared"},{"objects",objects},{"width",width},{"height",height},{"AA","original independent primitive boundary plus object alpha edges; <=2 exterior channel difference"}}.dump()<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

// Offline verification only. Fixed source-only contour/mask boundary, never derived from candidate differences.
#define main r2CornerPriorMain
#include "../../../runtime/render/tests/corner_join_test.cpp"
#undef main
namespace {
void rectWebOracle(const std::filesystem::path& snapshot,const std::filesystem::path& output,const render::CanonicalViewportTransform& camera){
 std::ifstream input(snapshot,std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
 auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);need(decoded.ok(),"actual Web snapshot decode");
 const auto& snapshotValue=*decoded.snapshot;const auto& objects=snapshotValue.objects;const auto projection=projectRuntimeScene(objects,semantic::SemanticGeneration{1});
 semantic::IndexedObjectStore store;semantic::DocumentRuntimeState state=semantic::DocumentRuntimeState::kLoading;semantic::ResourceManifest manifest;semantic::SemanticGenerationState generation{semantic::SemanticGeneration{1}};
 const semantic::SnapshotBootstrapper::CheckpointBinding binding{snapshotValue.document_id,snapshotValue.document_revision,snapshotValue.recovery_frontier,snapshotValue.checkpoint_digest};
 need(semantic::SnapshotBootstrapper::restore(snapshotValue,semantic::SnapshotCapability::kAllWithCompactStrokeV1,state,store,manifest,generation,binding).restored,"oracle semantic restore");
 const semantic::SemanticReadView view(store,generation.current());RuntimeScene scene;need(bool(scene.replace(view)),"oracle scene projection");
 render::FrameState frame{render::ViewId{1},render::CameraState{{(600-camera.translationX)/camera.scale,(400-camera.translationY)/camera.scale},camera.scale,0,render::CameraGeneration{1}},
  {0,0,1200,800},{1200,800,2400,1600,2,1},generation.current(),SceneRevision{1},render::SurfaceGeneration{1},render::MetricsGeneration{1},render::FrameId{1},{-camera.translationX/camera.scale,-camera.translationY/camera.scale,(1200-camera.translationX)/camera.scale,(800-camera.translationY)/camera.scale}};
 render::VisibilityResult visible{frame.sceneGeneration,frame.sceneReadToken,frame.worldViewport};for(const auto& obj:projection.records)visible.backToFront.push_back(obj.objectId);visible.visibleRecords=visible.backToFront.size();
 auto reference=render::DirectReferenceSource::build(frame,visible,scene);need(bool(reference),"oracle references");auto referenceList=reference.value();referenceList.worldToView={camera.scale,0,0,camera.scale,camera.translationX,camera.translationY};referenceList.viewportClip={0,0,1200,800};auto plan=render::FramePlanBuilder::build(frame,referenceList,decoded.snapshot->background,false,true);need(bool(plan),"oracle plan");
 auto surface=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(2400,1600));need(render::internal::drawReferencePlanToSkCanvas(*surface->getCanvas(),plan.value()).code==render::BackendSubmissionCode::kAccepted,"oracle full direct draw");
 auto edge=SkSurfaces::Raster(SkImageInfo::MakeA8(2400,1600));edge->getCanvas()->clear(SK_ColorTRANSPARENT);SkPaint stroke;stroke.setColor(SK_ColorWHITE);stroke.setAntiAlias(true);stroke.setStyle(SkPaint::kStroke_Style);stroke.setStrokeWidth(2);
 for(const auto& e:reference.value().entries){const auto& content=std::get<render::CompactStrokeReferenceCommand>(e.command).content.get().stroke;const auto contour=ink::compileCompactContour(content.config,content.knots);const auto& t=e.record.transform;
  SkMatrix matrix;matrix.setAll(float(2*camera.scale*t.a),float(2*camera.scale*t.c),float(2*(camera.scale*t.tx+camera.translationX)),float(2*camera.scale*t.b),float(2*camera.scale*t.d),float(2*(camera.scale*t.ty+camera.translationY)),0,0,1);
  for(auto p:paths(contour))edge->getCanvas()->drawPath(p.makeTransform(matrix),stroke);
  for(auto cap:contour.caps){SkPathBuilder builder;builder.addCircle(0,0,float(cap.radius));SkMatrix capMatrix;capMatrix.setTranslate(float(cap.x),float(cap.y));if(content.config.family==foundation::InkFamily::kMarker){capMatrix.preRotate(float(content.config.markerAngle*57.29577951308232));capMatrix.preScale(1,float(content.config.markerFlatness));}edge->getCanvas()->drawPath(builder.detach().makeTransform(capMatrix).makeTransform(matrix),stroke);}
  for(const auto& m:e.record.eraseMasks){if(const auto* f=std::get_if<semantic::FilledPathMask>(&m.geometry))edge->getCanvas()->drawPath(render::internal::semanticPath(f->path).makeTransform(matrix),stroke);
   else if(const auto* sweep=std::get_if<semantic::SweptCircleMask>(&m.geometry))for(const auto& p:sweep->segments){SkPathBuilder builder;builder.moveTo(float(p.p0.position.x),float(p.p0.position.y));builder.cubicTo(float(p.control1.x),float(p.control1.y),float(p.control2.x),float(p.control2.y),float(p.p1.position.x),float(p.p1.position.y));SkPaint mask;mask.setStyle(SkPaint::kStroke_Style);mask.setStrokeWidth(float(2*std::max(p.p0.radius,p.p1.radius)));mask.setStrokeCap(SkPaint::kRound_Cap);edge->getCanvas()->drawPath(skpathutils::FillPathWithPaint(builder.detach(),mask).makeTransform(matrix),stroke);}}
 }
 std::vector<std::uint8_t> rgba(2400*1600*4),edges(2400*1600);auto info=SkImageInfo::Make(2400,1600,kRGBA_8888_SkColorType,kPremul_SkAlphaType);need(surface->readPixels(info,rgba.data(),2400*4,0,0)&&edge->readPixels(edge->imageInfo(),edges.data(),2400,0,0),"oracle readback");
 std::ofstream(output.string()+".rgba",std::ios::binary).write(reinterpret_cast<const char*>(rgba.data()),rgba.size());std::ofstream(output.string()+".edges",std::ios::binary).write(reinterpret_cast<const char*>(edges.data()),edges.size());
 std::cout<<"actual snapshot direct CPU/independently projected 1pixel contour-boundary band, no candidate difference derived band\n";
}
}
int main(int argc,char**argv){try{need(argc==6,"snapshot output scale tx ty");rectWebOracle(argv[1],argv[2],{std::stof(argv[3]),std::stof(argv[4]),std::stof(argv[5])});return 0;}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}}

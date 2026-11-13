// Actual Render startup work regression. Full coverage eligibility stays intact.
#define main settledPriorCanonicalMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "canvas/ink/brush_package_catalog.hpp"
#include "../src/skia_navigation_tiles.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
namespace {
std::vector<ReferenceTraversalEntry> zoomSources(unsigned count,bool masked){
 std::vector<ReferenceTraversalEntry> result;
 for(unsigned line=0;line<count;++line){semantic::ObjectRecord object;object.id=ObjectId::fromUint64(500+line);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;
  semantic::CompactStrokeContent content;content.stroke.config=*ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4).package.compactConfig;content.stroke.seed=line+42;
  for(unsigned i=0;i<=1600;++i){const auto t=double(i)/1600;content.stroke.knots.push_back({600+900*std::sin(4*3.141592653589793*t),400+line*4+600*std::sin(6*3.141592653589793*t),32768});}
  object.content=content;if(masked)object.erase_masks={{ObjectId::fromUint64(800+line),semantic::SweptCircleMask{{{{{430,150},16},{{430,534},16},{430,278},{430,406}}}}}};
  auto scene=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});result.push_back({scene.records.front(),true,CompactStrokeReferenceCommand{content}});
 }return result;
}
FramePlan startupPlan(SkiaSurfaceProvider& target,const std::vector<ReferenceTraversalEntry>& entries,unsigned dpr){
 FrameState frame{ViewId{1},CameraState{{600,400},1,0,CameraGeneration{1}},{0,0,1200,800},{1200,800,1200*dpr,800*dpr,float(dpr),1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{target.generation()},MetricsGeneration{1},FrameId{1},{0,0,1200,800}};
 ReferenceDrawList draw{frame};draw.viewportClip={0,0,1200,800};draw.entries=entries;return FramePlan{frame,std::move(draw)};
}
void runStartup(SkiaSurfaceProvider& target,unsigned dpr,unsigned count,bool masked,nlohmann::json& rows){
 require(target.resize(1200*dpr,800*dpr).code==BackendSubmissionCode::kAccepted,"startup target");auto entries=zoomSources(count,masked);auto plan=startupPlan(target,entries,dpr);SkiaRenderer renderer;
 for(unsigned attempt=0;;++attempt){auto r=renderer.renderFrame(target,plan);if(r.code==BackendSubmissionCode::kAccepted)break;require(attempt<1024&&r.code==BackendSubmissionCode::kRetry,"startup canonical setup");}
 NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out,NavigationSourceDiagnostics& stamp){out=entries;stamp.sceneStamped=true;stamp.semanticGeneration=1;stamp.sceneRevision=1;return true;};
 const auto canonical=renderer.submissionCount(),beforePresents=target.presentCount();const auto scale=float(std::pow(.5,1./24));unsigned first=0;NavigationWork work;
 for(unsigned opportunity=1;opportunity<=64;++opportunity){auto r=renderer.renderNavigation(target,{scale,600*(1-scale),400*(1-scale)},plan.frame,query,false);require(r.code==BackendSubmissionCode::kAccepted,"startup navigation returned failure");work=renderer.navigationWork();
  if(work.presented){first=opportunity;require(work.coverageComplete,"presented unfinished exposure");break;}
  require(target.presentCount()==beforePresents,"unqualified exposure presented");
 }
 rows.push_back({{"count",count},{"masked",masked},{"dpr",dpr},{"first_opportunity",first},{"source_steps",work.sourceSteps},{"gpu_contours",work.gpuContourPrimitives},{"cpu_coarse_starts",work.coarseMaskStarts},{"initializations",work.initializations},{"queries",work.queries},{"tested",work.contourPrimitivesTested},{"drawn",work.contourPrimitivesDrawn},{"tiles",work.tilesBuilt},{"budget_bytes",work.retainedBytes}});
 std::cout<<rows.back().dump()<<std::endl;require(renderer.submissionCount()==canonical,"navigation created canonical receipt");require(first>0,"startup failed to converge");
 // A first complete scaled view should not replay each stroke for separate
 // speculative strips. There are at most four coarse cells for this viewport.
 require(work.sourceSteps<=4*count,"EXPOSURE_REPLAYED_SOURCES_FOR_SEPARATE_SPECULATIVE_STRIPS");
#if defined(__APPLE__)
 const auto acquired=target.acquire();const bool gpu=acquired.frame.surface->recordingContext()!=nullptr;target.release();
 if(gpu&&!masked)require(work.gpuContourPrimitives>0&&work.coarseMaskStarts==0,"PREPARED_UNMASKED_COARSE_EXCLUDED_FROM_GPU");
#endif
}
std::vector<std::uint8_t> readAll(SkiaSurfaceProvider& target,unsigned dpr){std::vector<std::uint8_t> bytes(1200*800*dpr*dpr*4);require(target.readbackRgba(bytes).code==BackendSubmissionCode::kAccepted,"full navigation pixels");return bytes;}

sk_sp<SkSurface> coldCoarseReference(SkiaSurfaceProvider& provider,const std::vector<ReferenceTraversalEntry>& entries,
    const CanonicalViewportTransform& camera,unsigned dpr,const sk_sp<SkImage>& retained){
 auto acquired=provider.acquire();require(acquired.code==SkiaSurfaceAcquireCode::kAcquired,"oracle target context");
 auto result=acquired.frame.surface->makeSurface(acquired.frame.surface->imageInfo());provider.release();require(bool(result),"oracle full surface");result->getCanvas()->clear(SK_ColorWHITE);
 const foundation::WorldRect visible{-camera.translationX/camera.scale,-camera.translationY/camera.scale,(1200-camera.translationX)/camera.scale,(800-camera.translationY)/camera.scale};
 TileGrid grid;int coarse=int(std::ceil(std::log2(camera.scale*dpr)))-1;while(std::ldexp(256.,-coarse)<std::max(visible.right-visible.left,visible.bottom-visible.top))--coarse;
 const auto keys=grid.coverage(visible,coarse,256);const auto density=std::ldexp(1.F,coarse);
 // Full cold source surfaces at the same absolute tile/grid and density.
 // No partial job regions, cache or old tile validity participates here.
 for(auto key:keys){auto b=grid.bounds(key);auto material=result->makeSurface(result->imageInfo().makeWH(260,260));auto coverage=result->makeSurface(SkImageInfo::MakeN32Premul(260,260));material->getCanvas()->clear(SK_ColorTRANSPARENT);material->getCanvas()->translate(2-b.left*density,2-b.top*density);material->getCanvas()->scale(density,density);
  for(const auto& entry:entries){if(const auto* compact=std::get_if<CompactStrokeReferenceCommand>(&entry.command)){auto* c=coverage->getCanvas();c->resetMatrix();c->clear(SK_ColorBLACK);c->setMatrix(material->getCanvas()->getTotalMatrix());const auto& t=entry.record.transform;c->concat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));drawCompactCoverageToSkCanvas(*c,compact->content.get().stroke.config,compact->content.get().stroke.knots);if(!entry.record.eraseMasks.empty())drawEraseMasksToSkCanvas(*c,entry);
    auto* m=material->getCanvas();m->save();m->concat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));compositeCompactCoverageToSkCanvas(*m,*coverage->makeImageSnapshot(),compact->content.get().stroke.config,compact->content.get().stroke.seed);m->restore();
   }else {auto* m=material->getCanvas();m->save();const auto& t=entry.record.transform;m->concat(SkMatrix::MakeAll(t.a,t.c,t.tx,t.b,t.d,t.ty,0,0,1));drawValidatedIsolatedEntryToSkCanvas(*m,entry,nullptr,nullptr,nullptr,false,nullptr,nullptr,nullptr);m->restore();}}
  const auto l=std::max(b.left,visible.left),t=std::max(b.top,visible.top),rr=std::min(b.right,visible.right),bb=std::min(b.bottom,visible.bottom);
  const auto src=SkRect::MakeLTRB((l-b.left)*density+2,(t-b.top)*density+2,(rr-b.left)*density+2,(bb-b.top)*density+2);
  const auto dst=SkRect::MakeLTRB((l*camera.scale+camera.translationX)*dpr,(t*camera.scale+camera.translationY)*dpr,(rr*camera.scale+camera.translationX)*dpr,(bb*camera.scale+camera.translationY)*dpr);
  result->getCanvas()->drawImageRect(material->makeImageSnapshot(),src,dst,SkSamplingOptions(SkFilterMode::kLinear),nullptr,SkCanvas::kFast_SrcRectConstraint);
 }
 if(retained){auto* c=result->getCanvas();c->save();c->clipRect(SkRect::MakeLTRB(camera.translationX*dpr,camera.translationY*dpr,(1200*camera.scale+camera.translationX)*dpr,(800*camera.scale+camera.translationY)*dpr));c->drawColor(SK_ColorWHITE,SkBlendMode::kSrc);c->translate(camera.translationX*dpr,camera.translationY*dpr);c->scale(camera.scale,camera.scale);c->drawImage(retained,0,0,SkSamplingOptions(SkFilterMode::kLinear));c->restore();}
 return result;
}
void pixelGuards(SkiaSurfaceProvider& target,SkiaSurfaceProvider& cold,SkiaSurfaceProvider& finalReference,unsigned dpr,bool gpu){
 auto entries=zoomSources(1,true);auto content=std::get<CompactStrokeReferenceCommand>(entries[0].command).content.get();content.stroke.config.opacity=.55;content.stroke.config.color={.12,.2,.5,1};content.stroke.config.family=foundation::InkFamily::kMarker;content.stroke.config.markerAngle=.7;content.stroke.config.markerFlatness=.25;entries[0].record.content=content;entries[0].command=CompactStrokeReferenceCommand{content};
 semantic::ObjectRecord background; background.id=ObjectId::fromUint64(900);background.kind=semantic::ObjectKind::kShape;background.kind_version=1;background.content=semantic::ShapeContent{1,3000,2200};background.transform.tx=-900;background.transform.ty=-700;background.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.3F,.7F,.4F,1}}}}};
 auto projected=projectRuntimeScene({&background,1},semantic::SemanticGeneration{1});ReferenceTraversalEntry backdrop{projected.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(background.content)}};
 auto foreground=backdrop;foreground.record.objectId=ObjectId::fromUint64(901);foreground.record.transform.tx=560;foreground.record.transform.ty=0;foreground.record.content=semantic::ShapeContent{1,40,800};foreground.command=ShapeReferenceCommand{std::get<semantic::ShapeContent>(foreground.record.content.get())};foreground.record.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.8F,.3F,.2F,1}}}}};
 entries.insert(entries.begin(),backdrop);entries.push_back(foreground);
 require(target.resize(1200*dpr,800*dpr).code==BackendSubmissionCode::kAccepted&&cold.resize(1200*dpr,800*dpr).code==BackendSubmissionCode::kAccepted&&finalReference.resize(1200*dpr,800*dpr).code==BackendSubmissionCode::kAccepted,"pixel providers");
 const auto current=startupPlan(target,entries,dpr);SkiaRenderer renderer;
 for(unsigned i=0;;++i){auto r=renderer.renderFrame(target,current);if(r.code==BackendSubmissionCode::kAccepted)break;require(i<512,"pixel canonical setup");}
 auto oldAcquire=target.acquire();auto oldImage=oldAcquire.frame.surface->makeImageSnapshot();target.release();const auto old=readAll(target,dpr);const auto receipts=renderer.submissionCount();
 NavigationSourceQuery query=[&](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>& out,NavigationSourceDiagnostics& stamp){out=entries;stamp.sceneStamped=true;stamp.semanticGeneration=1;stamp.sceneRevision=1;return true;};
 std::vector<CanonicalViewportTransform> cameras{{float(std::pow(.5,1./24)),float(600*(1-std::pow(.5,1./24))),float(400*(1-std::pow(.5,1./24)))}};
 for(const auto camera:cameras){
  auto expectedSurface=coldCoarseReference(cold,entries,camera,dpr,oldImage);std::vector<std::uint8_t> expected(1200*800*dpr*dpr*4);auto info=SkImageInfo::Make(1200*dpr,800*dpr,kRGBA_8888_SkColorType,kPremul_SkAlphaType);require(expectedSurface->readPixels(info,expected.data(),1200*dpr*4,0,0),"independent full grid read");
  for(unsigned i=0;i<256;++i){const auto previous=target.presentCount();auto r=renderer.renderNavigation(target,camera,current.frame,query);require(r.code==BackendSubmissionCode::kAccepted,"pixel navigation");const auto work=renderer.navigationWork();
   require(work.gpuContourPrimitives<=1000000,"invalid contour accounting");
   if(target.presentCount()>previous){require(work.coverageComplete,"incomplete navigation presented");const auto actual=readAll(target,dpr);
    unsigned changed=0,maxDelta=0;for(std::size_t p=0;p<actual.size();++p){const auto delta=unsigned(std::abs(int(actual[p])-int(expected[p])));changed+=delta>0;maxDelta=std::max(maxDelta,delta);}std::cout<<"samegrid comparison dpr="<<dpr<<" gpu="<<gpu<<" changedchannels="<<changed<<" max="<<maxDelta<<std::endl;
    for(std::size_t p=0;p<actual.size();++p)if(std::abs(int(actual[p])-int(expected[p]))>2){
      std::cerr<<"qualified grid/retained mismatch dpr="<<dpr<<" channel="<<p<<" actual="<<unsigned(actual[p])<<" expected="<<unsigned(expected[p])<<std::endl;
      require(false,"complete scaled viewport differs from independent cold-grid+sharp retained composition");}
   }else require(!work.presented,"present flag without provider");
   if(work.presented)break;require(i<255,"pixel navigation incomplete");
  }
  require(renderer.submissionCount()==receipts,"navigation generated canonical receipt");
 }
 const auto camera=cameras.back();auto frame=current.frame;ReferenceDrawList draw{frame};draw.entries=entries;draw.viewportClip={0,0,1200,800};draw.worldToView={camera.scale,0,0,camera.scale,camera.translationX,camera.translationY};FramePlan final{frame,std::move(draw)};
 RenderCacheOptions disabled;disabled.enabled=false;SkiaRenderer direct;direct.setRenderCacheOptions(disabled);
 for(unsigned i=0;;++i){auto r=renderer.renderFrame(target,final);if(r.code==BackendSubmissionCode::kAccepted)break;require(i<256,"final exact converge");}
 auto refFrame=startupPlan(finalReference,entries,dpr);ReferenceDrawList refDraw{refFrame.frame};refDraw.entries=entries;refDraw.viewportClip={0,0,1200,800};refDraw.worldToView={camera.scale,0,0,camera.scale,camera.translationX,camera.translationY};FramePlan independent{refFrame.frame,std::move(refDraw)};require(direct.renderFrame(finalReference,independent).code==BackendSubmissionCode::kAccepted,"final independent Direct");const auto actual=readAll(target,dpr),expected=readAll(finalReference,dpr);
 if(!gpu)require(actual==expected,"final exact Raster changed pixels");
 else {unsigned bad=0;for(std::size_t i=0;i<actual.size();++i)bad+=std::abs(int(actual[i])-int(expected[i]))>2;require(!bad,"final exact GPU prepared changed Direct pixels");}
 std::cout<<"pixel guards DPR"<<dpr<<" gpu="<<gpu<<" complete/masks/opacity/shapes/finalDirect\n";
}

}
int main(int argc,char** argv){try{nlohmann::json rows=nlohmann::json::array();
#if defined(__APPLE__)
 const auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"Metal context");NativeMetalTarget target(completion);
 runStartup(target,2,1,false,rows);runStartup(target,2,3,true,rows);NativeMetalTarget cold(completion),reference(completion);for(unsigned dpr:{1U,2U,4U})pixelGuards(target,cold,reference,dpr,true);(void)pool;
#else
 RasterSkiaSurfaceProvider target;runStartup(target,2,1,false,rows);runStartup(target,2,3,true,rows);
#endif
 RasterSkiaSurfaceProvider raster,coldRaster,refRaster;pixelGuards(raster,coldRaster,refRaster,1,false);
 if(argc>1)std::ofstream(argv[1])<<rows.dump(2)<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

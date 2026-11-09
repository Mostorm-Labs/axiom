#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/semantic/reference_object_store.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "object_store_mutator.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include <cassert>
#include <cstdio>
using namespace canvas;
using namespace canvas::render;
namespace {
struct Fixture {
 semantic::ReferenceObjectStore store;
 RuntimeScene scene;
 std::vector<ObjectId> ids;
 Fixture(){
  for(unsigned i=1;i<=2;++i){
   semantic::ObjectRecord r;r.id=ObjectId::fromUint64(i);r.kind=semantic::ObjectKind::kVectorStroke;r.kind_version=3;
   semantic::CompactStrokeContent c;const auto brush=ink::BrushPackageCatalog{}.loadDefault("round-handdrawn",4);assert(brush);
   c.stroke.config=*brush.package.compactConfig;c.stroke.config.size=28;c.stroke.config.opacity=.5;
   c.stroke.config.color={.1,.2,.7,1};c.stroke.seed=42;
   for(unsigned k=0;k<40;++k)c.stroke.knots.push_back({20.+k*5.,65.+i*5.,40000});
   r.content=std::move(c);r.placement.order_key=semantic::OrderKey({std::uint8_t(i)});
   if(i==2)r.transform={1,.12,.06,1,3,5};
   semantic::VectorPathGeometry path;path.commands={semantic::MoveTo{{88,20}},semantic::LineTo{{108,20}},semantic::LineTo{{108,115}},semantic::LineTo{{88,115}},semantic::ClosePath{}};
   r.erase_masks={{ObjectId::fromUint64(70+i),semantic::FilledPathMask{std::move(path)}}};
   assert(semantic::internal::ObjectStoreMutator::insertFresh(store,r));ids.push_back(r.id);
  }
  assert(scene.replace({store,semantic::SemanticGeneration{2}}));
 }
 FrameState frame(unsigned dpr,std::uint64_t surface)const{
  return {ViewId{1},CameraState{{128,64},1,0,CameraGeneration{2}},{0,0,256,128},
    {256,128,256*dpr,128*dpr,float(dpr),1},semantic::SemanticGeneration{2},SceneRevision{2},
    SurfaceGeneration{surface},MetricsGeneration{1},FrameId{2},{0,0,256,128}};
 }
 ReferenceDrawList references(const FrameState& f)const{
  VisibilityResult v{.sceneGeneration=f.sceneGeneration,.sceneReadToken=f.sceneReadToken,.queryWorldRect=f.worldViewport,.candidatesExamined=ids.size(),.visibleRecords=ids.size(),.backToFront=ids};
  const auto r=DirectReferenceSource::buildNavigation(f,v,scene);assert(r);assert(r.value().canonicalBytes.empty());return r.value();
 }
};
std::vector<std::uint8_t> oracle(const FramePlan& p){
 const auto& m=p.frame.metrics;const auto info=SkImageInfo::Make(m.physicalWidth,m.physicalHeight,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB());
 auto s=SkSurfaces::Raster(info);assert(s);assert(internal::drawReferencePlanToSkCanvas(*s->getCanvas(),p).code==BackendSubmissionCode::kAccepted);
 std::vector<std::uint8_t> pixels(m.physicalWidth*m.physicalHeight*4);assert(s->readPixels(info,pixels.data(),m.physicalWidth*4,0,0));return pixels;
}
}
int main(){
 for(unsigned dpr:{1U,2U,4U}){
  Fixture fixture;RasterSkiaSurfaceProvider provider;assert(provider.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted);
  const auto frame=fixture.frame(dpr,provider.generation());const auto refs=fixture.references(frame);
  foundation::DocumentBackground background;background.color={.3F,.5F,.4F,1};
  const auto plan=FramePlanBuilder::build(frame,refs,background);assert(plan);
  SkiaRenderer renderer;
  const NavigationSourceQuery query=[&](const auto&,auto& entries,NavigationSourceDiagnostics& d){entries=refs.entries;d.sceneStamped=true;d.semanticGeneration=2;d.sceneRevision=2;return true;};
  bool presented=false;
  for(unsigned i=0;i<8192&&!presented;++i){const auto r=renderer.renderNavigation(provider,{1,0,0},frame,query,false,nullptr,{},&plan.value());assert(r.code!=BackendSubmissionCode::kRejected);presented=renderer.navigationWork().presented;}
  assert(presented);const auto before=provider.presentCount();
  // A different plan identity cannot certify these pixels, even on the same GPU target.
  auto wrong=FramePlan{FrameState{ViewId{999},frame.camera,frame.worldViewport,frame.metrics,frame.sceneGeneration,frame.sceneReadToken,frame.surfaceGeneration,frame.metricsGeneration,frame.frameId,frame.viewportClip},refs,background};
  assert(renderer.renderNavigation(provider,{1,0,0},frame,query,false,nullptr,{},&wrong).code!=BackendSubmissionCode::kAccepted);
  assert(provider.presentCount()==before);
  for(bool transparent:{false,true}){
   const auto p=FramePlanBuilder::build(frame,refs,background,transparent,true);assert(p);SkiaRenderer independent;RenderCacheOptions noCache;noCache.enabled=false;independent.setRenderCacheOptions(noCache);
   assert(independent.renderFrame(provider,p.value()).code==BackendSubmissionCode::kAccepted);
   std::vector<std::uint8_t> actual(256*dpr*128*dpr*4);assert(provider.readbackRgba(actual).code==BackendSubmissionCode::kAccepted);assert(actual==oracle(p.value()));
   if(transparent){const auto count=provider.presentCount();assert(renderer.renderNavigation(provider,{1,0,0},frame,query,false,nullptr,{},&p.value()).code!=BackendSubmissionCode::kAccepted);assert(provider.presentCount()==count);}
  }
  std::printf("{\"dpr\":%u,\"current_composition_without_exact\":true,\"independent_canonical_pixels\":true,\"transparent_and_wrong_frame_rejected\":true}\n",dpr);
 }
}

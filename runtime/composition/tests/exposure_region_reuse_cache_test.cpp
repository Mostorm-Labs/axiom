#define main oldCanonicalMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "../../render/src/skia_ink_layers.hpp"
#include "../../render/tests/navigation-sharing-serialize.hpp"
#include "canvas/render/tile/tile_grid.hpp"
#include "include/core/SkColorSpace.h"
#include <nlohmann/json.hpp>
int main(int argc,char**argv){
 require(argc==2,"output");
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{512U*1024U*1024U,1024U*1024U*1024U});
 RenderCacheOptions options;options.maxEntries=2;
 PreparedGeometryCache prepared;auto e=source();auto body=prepared.get(e,options,budget,true);require(body&&body->ready(),"bodyReady");
 SkiaInkLayers layers;auto raster=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(68,68));auto*c=raster->getCanvas();c->scale(.125F,.125F);c->clear(SK_ColorBLACK);body->drawContour(*c);
 auto white=raster->makeImageSnapshot();layers.rememberNavigationBody(body,*c,white,options,budget);
 require(bool(layers.navigationBody(body,*c,options,budget)),"identicalCopyHit");
 c->translate(.5F,0);require(!layers.navigationBody(body,*c,options,budget),"fractionalPhaseMustMiss");c->translate(-.5F,0);
 c->save();c->clipIRect(SkIRect::MakeLTRB(0,0,50,68));require(!layers.navigationBody(body,*c,options,budget),"differentCropMustMiss");c->restore();
 auto format=SkSurfaces::Raster(SkImageInfo::MakeA8(68,68));format->getCanvas()->setMatrix(c->getTotalMatrix());
 require(!layers.navigationBody(body,*format->getCanvas(),options,budget),"targetFormatMustMiss");
 auto color=SkSurfaces::Raster(SkImageInfo::Make(68,68,kN32_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB()));color->getCanvas()->setMatrix(c->getTotalMatrix());
 require(!layers.navigationBody(body,*color->getCanvas(),options,budget),"colorSpaceMustMiss");
 auto other=e;auto content=std::get<semantic::CompactStrokeContent>(e.record.content.get());++content.stroke.seed;other.record.content=std::move(content);other.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(other.record.content)};auto second=prepared.get(other,options,budget,true);require(second&&second!=body,"fullContentIdentity");require(!layers.navigationBody(second,*c,options,budget),"seedMustMiss");
 SkPaint clear;clear.setBlendMode(SkBlendMode::kClear);c->drawCircle(100,65,20,clear);auto hit=layers.navigationBody(body,*c,options,budget);require(bool(hit),"immutableBodySurvivesMask");
 auto check=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(68,68));check->getCanvas()->drawImage(hit,0,0);SkPixmap expected,actual;require(white->peekPixels(&expected)&&check->peekPixels(&actual),"pixelsAvailable");require(std::memcmp(expected.addr(),actual.addr(),68*68*4)==0,"maskDoesNotContaminateBody");
 // Shared canonical producer must complete full known support before lookup;
 // navigation may explicitly project density/phase, but exact canonical cannot.
 auto autorelease=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto context=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};context->context=GrDirectContexts::MakeMetal(backend);require(bool(context->context),"mappingMetal");
 const auto gpuSurface=[&](int w,int h){return SkSurfaces::RenderTarget(context->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(w,h));};
 auto shared=std::make_shared<SkiaInkLayers>();auto supportTarget=SkSurfaces::RenderTarget(context->context.get(),skgpu::Budgeted::kNo,
    SkImageInfo::Make(800,180,kRGBA_8888_SkColorType,kPremul_SkAlphaType,SkColorSpace::MakeSRGB()));
 PreparationAllowance allowance;allowance.limits.maxCoverageSteps=1;
 CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork sharedWork;sk_sp<SkImage> preparedPixels;SkMatrix bodyToPixels;
 bool complete=false;for(unsigned n=0;n<4096&&!complete;++n){PreparationAllowance step;step.limits.maxCoverageSteps=1;
   complete=SkiaInkLayers::BodyPreparation::kReady==shared->prepareNavigationBody(e,body,SkMatrix::Scale(.5F,.5F),*supportTarget->getCanvas(),options,budget,targets,sharedWork,step,preparedPixels,bodyToPixels);
 }
 require(complete&&preparedPixels,"sharedProducerCompletesBounded");
 auto projected=gpuSurface(68,68);projected->getCanvas()->scale(.125F,.125F);projected->getCanvas()->clear(SK_ColorBLACK);
 require(shared->projectNavigationBody(body,*projected->getCanvas(),options,budget),"canonicalBodyConsumedByNavigation");
 // Nonzero crop and distinct object translations must use deviceX/Y,
 // never cancel the pixel mapping with the material helper's inverse CTM.
 auto read=[](SkSurface&surface){auto info=SkImageInfo::MakeN32Premul(surface.width(),surface.height());std::vector<std::uint8_t> v(info.computeMinByteSize());require(surface.readPixels(info,v.data(),info.minRowBytes(),0,0),"mappingRead");return v;};
 for(const auto offset:{SkPoint{31,19},SkPoint{76,33}}){
  auto material=gpuSurface(800,180);auto expectedMaterial=gpuSurface(800,180);
  auto matrix=SkMatrix::Scale(.5F,.5F);matrix.postTranslate(offset.x(),offset.y());
  sk_sp<SkImage> image;SkMatrix imageBasis;SkiaInkLayers::BodyPreparation status;
  for(unsigned n=0;;++n){PreparationAllowance slice;status=shared->prepareNavigationBody(e,body,matrix,*material->getCanvas(),options,budget,targets,sharedWork,slice,image,imageBasis);if(status==SkiaInkLayers::BodyPreparation::kReady)break;require(status==SkiaInkLayers::BodyPreparation::kPending&&n<4096,"translatedProducer");}
  auto*draw=material->getCanvas();draw->clear(SK_ColorTRANSPARENT);draw->setMatrix(matrix);
  const auto&stroke=std::get<CompactStrokeReferenceCommand>(e.command).content.get().stroke;
  compositeCompactCoverageToSkCanvas(*draw,*image,stroke.config,stroke.seed,int(std::round(matrix.getTranslateX()-imageBasis.getTranslateX())),int(std::round(matrix.getTranslateY()-imageBasis.getTranslateY())));
  auto whiteReference=gpuSurface(800,180);whiteReference->getCanvas()->clear(SK_ColorBLACK);whiteReference->getCanvas()->setMatrix(matrix);body->drawContour(*whiteReference->getCanvas());
  auto*reference=expectedMaterial->getCanvas();reference->clear(SK_ColorTRANSPARENT);reference->setMatrix(matrix);compositeCompactCoverageToSkCanvas(*reference,*whiteReference->makeImageSnapshot(),stroke.config,stroke.seed);
  auto a=read(*material),b=read(*expectedMaterial);std::size_t changed=0;unsigned max=0;for(std::size_t i=0;i<a.size();++i)if(a[i]!=b[i]){++changed;max=std::max(max,unsigned(std::abs(int(a[i])-int(b[i]))));}
  std::fprintf(stderr,"mapping offset=%g,%g changed=%zu max=%u\n",offset.x(),offset.y(),changed,max);
  require(a==b,"nonzeroCropTranslationMaterialPixels");
 }
 // Insertion/copy epochs keep evicted pixels charged through actual completion.
 SkiaInkLayers leases;auto epoch=budget->beginGpuEpoch();require(bool(epoch),"gpuEpochStart");
 leases.rememberNavigationBody(body,*c,white,options,budget);const auto during=budget->used();leases.clear();
 require(budget->used()==during&&budget->snapshot(resources::ResourceClass::kRaster).pendingRelease>0,"insertionLeaseRetiredEarly");
 require(budget->endGpuEpoch(*epoch)&&budget->completeGpuEpoch(*epoch)&&budget->used()<during,"completionLeaseNotReleased");
 for(unsigned i=0;i<5;++i){c->setMatrix(SkMatrix::Translate(float(i),0));leases.rememberNavigationBody(body,*c,white,options,budget);require(leases.navigationBodyWork().entries<=2,"bodyResultCombinedCountCap");}
 leases.bindSurfaceBasis(&leases,1,1,1,1);c->setMatrix(SkMatrix::I());leases.rememberNavigationBody(body,*c,white,options,budget);
 require(bool(leases.navigationBody(body,*c,options,budget)),"basisWarmHit");leases.bindSurfaceBasis(&leases,2,1,1,1);
 require(!leases.navigationBody(body,*c,options,budget),"resourceBasisStaleHit");
 shared->clear();targets.clear();projected.reset();supportTarget.reset();
 // Same parent/LOD, distinct fixed64 units must survive observers and map identity.
 TileGrid grid;TileKey a{0,0,0,1},b{0,0,0,2};require(a!=b&&grid.bounds(a).right==64&&grid.bounds(b).left==64,"fixedRegionIdentity");
 ns::Job j;j.x=0;j.y=0;j.scale=0;
 auto serialized=ns::jobJson(j);require(serialized["key"].size()==4,"observerRegionIdentity");
 auto used=budget->used();layers.clear();require(budget->used()<used,"cacheLeaseReleases");
 std::ofstream(argv[1])<<nlohmann::json{{"status","PASS"},{"exactCopy",true},{"phaseCropFormatColorSpace",true},{"maskImmutable",true},{"regionIdentity",true}}.dump(2)<<'\n';
}

#define main previousCacheTestMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/semantic/snapshot.hpp"
#include "../../render/src/skia_ink_layers.hpp"
#include <nlohmann/json.hpp>
namespace canvas::render::internal {struct PostNavigationBodyTestAccess {
 static void evictComplete(SkiaInkLayers& layers){for(auto it=layers.fullBodies_.begin();it!=layers.fullBodies_.end();)if(it->second.completeSupport){layers.navigationWork_.bytes-=it->second.bytes;it=layers.fullBodies_.erase(it);}else++it;}
 static std::size_t pending(const SkiaInkLayers& layers){return std::count_if(layers.fullBodies_.begin(),layers.fullBodies_.end(),[](const auto&row){return bool(row.second.raster);});}
};}
int main(int argc,char**argv){
 require(argc==3,"snapshot output");auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 auto dev=MTLCreateSystemDefaultDevice(),queue=message(dev,"newCommandQueue");auto context=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(dev),sk_cfp<GrMTLHandle>(queue)};context->context=GrDirectContexts::MakeMetal(backend);require(bool(context->context),"cropMetal");
 std::ifstream f(argv[1],std::ios::binary);std::vector<std::uint8_t> data{std::istreambuf_iterator<char>(f),{}};auto decoded=semantic::SnapshotCodec::decode(data,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"cropSnapshot");
 auto projected=projectRuntimeScene(decoded.snapshot->objects,semantic::SemanticGeneration{1});ReferenceTraversalEntry entry;bool found=false;
 for(const auto&r:projected.records)if(const auto*c=std::get_if<semantic::CompactStrokeContent>(&r.content.get());c&&c->stroke.config.algorithmVersion==4&&c->stroke.knots.size()==7199){entry.record=r;entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(r.content)};found=true;break;}require(found,"cropFixtureLongBody");
 RenderCacheOptions options;options.maxBytes=512U*1024*1024;options.sharedMaxBytes=1024U*1024*1024;options.maxEntryBytes=128U*1024*1024;
 auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{options.maxBytes,options.sharedMaxBytes});PreparedGeometryCache geometry;auto body=geometry.get(entry,options,budget,true);require(body&&body->ready(),"cropReady");
 auto surface=SkSurfaces::RenderTarget(context->context.get(),skgpu::Budgeted::kNo,SkImageInfo::MakeN32Premul(5724,4040));require(bool(surface),"cropSurface");
 SkiaInkLayers shared;CompactCoverageTargets targets;targets.budget=budget;RenderCacheWork work;
 auto prepare=[&](SkMatrix matrix){sk_sp<SkImage> image;SkMatrix basis;unsigned calls=0;for(;;){PreparationAllowance a;auto result=shared.prepareNavigationBody(entry,body,matrix,*surface->getCanvas(),options,budget,targets,work,a,image,basis);context->context->flushAndSubmit(GrSyncCpu::kYes);context->context->checkAsyncWorkCompletion();require(++calls<8192,"cropProgress");if(result==SkiaInkLayers::BodyPreparation::kReady)return std::tuple{image,basis,calls};require(result==SkiaInkLayers::BodyPreparation::kPending,"cropUnavailable");}};
 auto initial=SkMatrix::Scale(4,4);initial.postTranslate(-800,-480);auto first=prepare(initial);auto before=work.preparationCoverageSteps;
 auto zoom=SkMatrix::Scale(9,9);zoom.postTranslate(-4200,-3000);auto [image,basis,calls]=prepare(zoom);
 auto pixels=std::uint64_t(image->width())*image->height();auto bounds=zoom.mapRect(body->support());bounds.outset(4,4);SkIRect full;bounds.roundOut(&full);auto complete=std::uint64_t(full.width())*full.height();
 std::ofstream(argv[2])<<nlohmann::json{{"pixels",pixels},{"fullSupportPixels",complete},{"coverageSteps",work.preparationCoverageSteps-before},{"calls",calls}}.dump(2)<<'\n';
 require(pixels<complete/4,"POST_ZOOM_REPEATS_FULL_OFFSCREEN_BODY");
 // Croppedimage stays exact at its originalintegerdevicegrid; fullhistory
 // independentGPUcoverage atmatchingmatrix verifies nonzero crop/phase.
 auto actual=surface->makeSurface(surface->imageInfo());auto expected=surface->makeSurface(surface->imageInfo());actual->getCanvas()->clear(SK_ColorBLACK);expected->getCanvas()->clear(SK_ColorBLACK);
 auto*c=actual->getCanvas();c->resetMatrix();c->drawImage(image,float(std::round(zoom.getTranslateX()-basis.getTranslateX())),float(std::round(zoom.getTranslateY()-basis.getTranslateY())));
 expected->getCanvas()->setMatrix(zoom);body->drawContour(*expected->getCanvas());auto info=SkImageInfo::MakeN32Premul(5724,4040);std::vector<std::uint8_t>a(info.computeMinByteSize()),b(a.size());require(actual->readPixels(info,a.data(),info.minRowBytes(),0,0)&&expected->readPixels(info,b.data(),info.minRowBytes(),0,0),"cropRead");
 std::size_t bad=0;for(std::size_t i=0;i<a.size();++i)bad+=std::abs(int(a[i])-int(b[i]))>2;require(bad==0,"croppedCoverageChangedPixels");
 // An admitted partial crop survives eviction of its optional complete
 // precursor. Work stays on exactly one pending job and same range.
 auto next=zoom;next.postTranslate(16,12);sk_sp<SkImage> resumed;SkMatrix resumedBasis;PreparationAllowance one;one.limits.maxCoverageSteps=1;
 auto status=shared.prepareNavigationBody(entry,body,next,*surface->getCanvas(),options,budget,targets,work,one,resumed,resumedBasis);
 require(status==SkiaInkLayers::BodyPreparation::kPending&&PostNavigationBodyTestAccess::pending(shared)==1,"partialPendingAdmission");
 PostNavigationBodyTestAccess::evictComplete(shared);std::size_t attempts=0;
 do{PreparationAllowance slice;slice.limits.maxCoverageSteps=32;status=shared.prepareNavigationBody(entry,body,next,*surface->getCanvas(),options,budget,targets,work,slice,resumed,resumedBasis);require(++attempts<128,"evictedBasisResetPendingCrop");require(PostNavigationBodyTestAccess::pending(shared)<=1,"pendingCropDuplicated");}while(status==SkiaInkLayers::BodyPreparation::kPending);
 require(status==SkiaInkLayers::BodyPreparation::kReady&&std::uint64_t(resumed->width())*resumed->height()<complete/4,"partialResumeBecameFullSupport");
 // Partial source cannot fill an unrelated destination; reflection and shear
 // are conservatively contained, never clamped to pretend outside blank.
 auto outside=surface->makeSurface(SkImageInfo::MakeN32Premul(256,256));outside->getCanvas()->setMatrix(SkMatrix::Translate(-10000,-10000));
 require(!shared.projectNavigationBody(body,*outside->getCanvas(),options,budget),"partialOutsideRangeAccepted");
 shared.clear();require(PostNavigationBodyTestAccess::pending(shared)==0,"cancelPendingNotCleared");
 std::printf("demandcrop pixels=%llu full=%llu steps=%llu calls=%u\n",(unsigned long long)pixels,(unsigned long long)complete,(unsigned long long)(work.preparationCoverageSteps-before),calls);return 0;
}

#define main temporaryWaitPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/gpu_resource_retirement.hpp"
#include "../src/resource_reclaim.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using nlohmann::json;
namespace canvas::render::internal {
struct TemporaryRetirementWaitProbe {
 static auto state(const SkiaRenderCache& c){return std::tuple(c.preparationCursor_,c.admissions_.size(),c.bodyDependencies_.size(),c.stagedDemand_,c.stagedBodies_,c.streamFrame_,c.admissionReady_,c.composite_.get(),c.pending_.get());}
 static bool acquire(SkiaRenderCache& c,SkCanvas& canvas,sk_sp<SkSurface>& surface,resources::ResourceLease& lease,bool mandatory=true){return c.acquireWorkSurface(canvas,surface,lease,mandatory);}
};
}
namespace {
#if defined(__APPLE__)
FramePlan waitPlan(){
 auto e=source();auto content=std::get<CompactStrokeReferenceCommand>(e.command).content.get();content.stroke.config.opacity=.55;
 for(std::size_t i=0;i<content.stroke.knots.size();++i){auto& k=content.stroke.knots[i];k.x=20.+200.*i/7198;k.y=64+30*std::sin(i*.023);}
 semantic::ObjectRecord object;object.id=ObjectId::fromUint64(1);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=content;
 semantic::SweptCircleMask history;history.segments={{{{70,25},4},{{70,105},4},{70,45},{70,85}}};object.erase_masks={{ObjectId::fromUint64(500),history}};
 e.record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
 auto base=renderPlan(e,1,1);FrameState frame{ViewId{1},CameraState{{256,256},1,0,CameraGeneration{1}},{0,0,512,512},{512,512,512,512,1,1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{1},{0,0,512,512}};ReferenceDrawList draw{frame};draw.viewportClip={0,0,512,512};draw.entries=base.referenceDrawList.entries;
 for(unsigned n=0;n<7;++n){auto shape=draw.entries.front();shape.record.objectId=ObjectId::fromUint64(100+n);shape.record.transform.tx=n*3;draw.entries.insert(draw.entries.begin()+n,std::move(shape));}
 for(unsigned i=1;i<40;++i){auto next=e;next.record.objectId=ObjectId::fromUint64(i+1);if(i>=13&&i<28){next.record.transform.tx=(i-12)*.75;next.record.transform.ty=(i-12)*.25;}if(i>=28)next.record.eraseMasks.clear();draw.entries.push_back(std::move(next));}
 return {frame,std::move(draw),{},false,true};
}
struct CacheFixture {
 NativeMetalTarget target;
 FramePlan plan;
 std::shared_ptr<resources::ResourceBudgetManager> budget;
 std::shared_ptr<PreparedGeometryCache> geometry=std::make_shared<PreparedGeometryCache>();
 std::shared_ptr<SourceRetentionPool> pool=std::make_shared<SourceRetentionPool>();
 SkiaRenderCache cache;
 SkSurface* surface=nullptr;
 std::shared_ptr<const PreparedNavigationGeometry> body;
 CacheFixture(std::shared_ptr<NativeMetalCompletion> completion):target(std::move(completion)),plan(waitPlan()),budget(std::make_shared<resources::ResourceBudgetManager>()){
  require(target.resize(512,512).code==BackendSubmissionCode::kAccepted,"actual Metal target");surface=target.acquire().frame.surface;surface->getCanvas()->clear(SK_ColorMAGENTA);
  cache.budget=budget;cache.sourcePool=pool;cache.preparedGeometry=geometry;geometry->setRetentionPool(pool);cache.options.maxEntries=64;
  cache.begin(plan,&target,*surface,0);
  for(unsigned n=0;;++n){PreparationAllowance a;auto status=cache.prepareBodies(*surface->getCanvas(),plan,a);if(status==SkiaRenderCache::Preparation::kReady)break;require(status==SkiaRenderCache::Preparation::kPending&&n<2048,"body shared preparation");}
  require(!cache.stagedBodies(),"baseline body demand must fit normally");
  RenderPreparationBudget one;one.maxGroups=1;
  for(unsigned n=0;!cache.hasPixels(plan.referenceDrawList.entries[8]);++n){PreparationAllowance a(one);auto status=cache.prepare(*surface->getCanvas(),plan,one,nullptr,nullptr,&a);require(status!=SkiaRenderCache::Preparation::kRejected&&n<4096,"material prefix failed");}
  body=cache.readyBody(plan.referenceDrawList.entries[8],PreparedConsumer::kCanonical);require(bool(body),"real shared ready body");
  require(std::get<2>(TemporaryRetirementWaitProbe::state(cache))==40,"actual forty body dependencies missing");
  require(geometry->statistics().builds==1&&geometry->statistics().compileInputs==7199,"body must compile once");
 }
 json finish(){
  RenderPreparationBudget slice;
  for(unsigned n=0;;++n){PreparationAllowance a(slice);auto status=cache.prepare(*surface->getCanvas(),plan,slice,nullptr,nullptr,&a);if(status==SkiaRenderCache::Preparation::kReady)break;require(status!=SkiaRenderCache::Preparation::kRejected&&n<8192,"cache failed to finish");}
  require(cache.drawTransientScene(*surface->getCanvas(),plan,nullptr,nullptr,true),"consume actual completed materials");
  auto info=SkImageInfo::Make(512,512,kRGBA_8888_SkColorType,kPremul_SkAlphaType);std::vector<std::uint8_t> bytes(512*512*4);require(surface->readPixels(info,bytes.data(),info.minRowBytes(),0,0),"actual Metal pixels");
  unsigned long long digest=1469598103934665603ULL;for(auto b:bytes)digest=(digest^b)*1099511628211ULL;
  return {{"pixel_digest",std::to_string(digest)},{"compile",geometry->statistics().compileInputs},{"builds",geometry->statistics().builds},{"coverage",cache.work.preparationCoverageSteps},{"GPU_tiles",cache.work.gpuCoverageTiles},{"rasterized",cache.work.rasterizedGroups},{"body_pixel_builds",cache.work.bodyPixelBuilds},{"body_pixel_hits",cache.work.bodyPixelHits},{"presents",target.presentCount()}};
 }
 ~CacheFixture(){body.reset();cache.clear();geometry->clear();target.release();}
};
json temporaryPressure(std::shared_ptr<NativeMetalCompletion> completion){
 CacheFixture f(completion);auto& c=f.cache;const auto before=TemporaryRetirementWaitProbe::state(c);const auto stats=f.geometry->statistics();const auto work=c.work;
 const std::size_t hard=96U*1024U*1024U,plane=512*512*4;
 const auto epoch=f.budget->beginGpuEpoch(resources::resourceClassMask(resources::ResourceClass::kNavigation),0);require(bool(epoch),"real GPU epoch");
 const auto heldBytes=hard-f.budget->used()-plane/2;auto token=f.budget->reserve(resources::ResourceClass::kNavigation,heldBytes,true);require(bool(token),"held lease actual admission");resources::ResourceLease pressure(f.budget,*token);
 require(f.budget->endGpuEpoch(*epoch),"end pressure epoch");pressure.reset();
 const auto used=f.budget->used(),pending=f.budget->snapshot(resources::ResourceClass::kNavigation).pendingRelease;
 require(pending==heldBytes&&used>hard-plane,"actual released-but-uncompleted capacity inequality");
 sk_sp<SkSurface> extra;resources::ResourceLease lease;json attempts=json::array();bool preserved=true;
 for(unsigned n=0;n<3;++n){const bool acquired=TemporaryRetirementWaitProbe::acquire(c,*f.surface->getCanvas(),extra,lease);const bool same=TemporaryRetirementWaitProbe::state(c)==before;
  attempts.push_back({{"retry",n},{"actual_acquire",acquired},{"used",f.budget->used()},{"pending",f.budget->snapshot(resources::ResourceClass::kNavigation).pendingRelease},{"state_preserved",same},{"staged",c.stagedBodies()}});preserved&=!acquired&&!extra&&same&&c.hasPixels(f.plan.referenceDrawList.entries[8])&&c.readyBody(f.plan.referenceDrawList.entries[8],PreparedConsumer::kCanonical)==f.body;
 }
 json report={{"held_bytes",heldBytes},{"used",used},{"hard",hard},{"requested",plane},{"pending_release",pending},{"epoch",*epoch},{"retry_rows",attempts},{"progress_preserved",preserved}};std::cout<<report.dump()<<'\n';
 require(preserved,"TEMPORARY_RETIREMENT_DESTROYED_PROGRESS_OR_LATCHED_STAGED");
 require(c.work.preparationCoverageSteps==work.preparationCoverageSteps&&f.geometry->statistics().compileInputs==stats.compileInputs,"waiting performed geometry work");
 require(f.budget->completeGpuEpoch(*epoch),"qualified actual completion");
 require(TemporaryRetirementWaitProbe::acquire(c,*f.surface->getCanvas(),extra,lease)&&extra,"actual reserve failed after true completion");require(f.budget->used()<=hard,"quota overcommit");extra.reset();lease.reset();
 report["finished"]=f.finish();return report;
}
json noPressure(std::shared_ptr<NativeMetalCompletion> completion){CacheFixture f(completion);sk_sp<SkSurface> extra;resources::ResourceLease lease;require(TemporaryRetirementWaitProbe::acquire(f.cache,*f.surface->getCanvas(),extra,lease),"no-pressure work surface");extra.reset();lease.reset();return f.finish();}
#endif
}
int main(int argc,char** argv){try{
#if defined(__APPLE__)
 auto autorelease=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
 const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"REQUIRED_ACTUAL_METAL_CONTEXT_UNAVAILABLE");
 auto pressure=temporaryPressure(completion);auto control=noPressure(completion);require(pressure["finished"]==control,"temporary wait increased replay or changed final pixels");json report={{"actual_Metal",true},{"temporary_pressure",pressure},{"no_pressure_control",control},{"exact_work_pixel_parity",true}};
 if(argc>1)std::ofstream(argv[1])<<report.dump(2)<<'\n';std::cout<<report.dump()<<'\n';completion.reset();message(autorelease,"drain");return 0;
#else
 std::cerr<<"actual Metal environment required\n";return 2;
#endif
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

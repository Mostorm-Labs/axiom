#include <iostream>
#define main g5OriginalCanonicalTestMain
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/skia_render_cache_internal.hpp"
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/src/skia_gpu_lifetime.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
int main(){try{
 auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();
 GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"Metal context");
 for(unsigned w:{600U,1431U})for(bool large:{false,true}){
  const unsigned h=900,dpr=2,pw=w*dpr,ph=h*dpr;NativeMetalTarget target(completion);require(target.resize(pw,ph).code==BackendSubmissionCode::kAccepted,"surface");
  auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{std::size_t(large?256:32)*1024*1024,std::size_t(large?512:96)*1024*1024});
  {internal::SkiaRenderCache cache;cache.budget=budget;cache.options.maxBytes=std::size_t(large?256:32)*1024*1024;cache.options.sharedMaxBytes=std::size_t(large?512:96)*1024*1024;cache.options.maxEntryBytes=std::size_t(large?64:8)*1024*1024;
   internal::GpuResourceRetirement retire;
   auto shape=[](unsigned id,double x){semantic::ObjectRecord o;o.id=ObjectId::fromUint64(id);o.kind=semantic::ObjectKind::kShape;o.kind_version=1;o.content=semantic::ShapeContent{1,90,52};o.transform.tx=x;o.transform.ty=100;o.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{.2F,.4F,.8F,1}}}}};auto p=projectRuntimeScene({&o,1},semantic::SemanticGeneration{1});return ReferenceTraversalEntry{p.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(o.content)}};};
   auto a=shape(10,30),b=shape(20,180),c=shape(30,340);nlohmann::json timings=nlohmann::json::array();
   for(unsigned m=0;m<3;++m){b.record.transform.tx+=2;
    FrameState f{ViewId{1},CameraState{{0,0},1,0,CameraGeneration{1}},{0,0,float(w),float(h)},{float(w),float(h),pw,ph,2,1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{m+1},{0,0,float(w),float(h)}};ReferenceDrawList draw{f};draw.viewportClip=f.viewportClip;draw.entries={a,b,c};FramePlan plan{f,std::move(draw),{},false,false};
    auto t=target.acquire();auto start=std::chrono::steady_clock::now();{
     RenderCacheWork work;internal::RenderExecutionScope execution(internal::ExecutionPurpose::kFullTransientEdit,plan,*t.frame.surface,&target,0,true,cache.options.sharedMaxBytes,work,target.describe().kind);
     internal::SkiaGpuLifetime gpu(retire,budget,target,completion->context.get());require(gpu.ready(),"GPU budget");gpu.touch();
     cache.begin(plan,&target,*t.frame.surface,0,b.record.objectId,true);execution.bindTargets(cache.executionTargets());
     require(cache.drawTransientScene(*t.frame.surface->getCanvas(),plan,nullptr,nullptr,true,false,nullptr,true),"scene draw");
    }target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"present");retire.service();timings.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
   }
   auto v=cache.objectWork;std::cout<<nlohmann::json{{"logical_width",w},{"logical_height",h},{"DPR",2},{"large_budget",large},{"optionalMiB",large?256:32},{"sharedMiB",large?512:96},{"entryMiB",large?64:8},{"static_builds",v.builds},{"static_hits",v.hits},{"static_denials",v.denials},{"full_scene_draws",v.fullSceneDraws},{"active_draws",v.activeDraws},{"leased_bytes",v.leaseBytes},{"draw_present_sync_ms",timings}}.dump()<<std::endl;
   cache.clear();completion->context->flushAndSubmit(GrSyncCpu::kYes);retire.service();
  }require(budget->used()==0,"leak");
 }
 return 0;
}catch(std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}}

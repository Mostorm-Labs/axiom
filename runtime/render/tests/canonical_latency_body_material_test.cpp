#define main canonicalLatencyPriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#include <nlohmann/json.hpp>
using nlohmann::json;
#undef main
#include <iostream>
namespace {
#if defined(__APPLE__)
FramePlan reusePlan(unsigned copies,unsigned tails,unsigned dpr=1,std::uint64_t generation=1,bool unique=false){
    auto e=source();auto content=std::get<CompactStrokeReferenceCommand>(e.command).content.get();
    content.stroke.config.opacity=.55;
    for(std::size_t i=0;i<content.stroke.knots.size();++i){auto& k=content.stroke.knots[i];k.x=20.+200.*i/7198;k.y=64+30*std::sin(i*.023);}
    semantic::ObjectRecord object;object.id=ObjectId::fromUint64(1);object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=content;
    semantic::SweptCircleMask history;history.segments={{{{70,25},4},{{70,105},4},{70,45},{70,85}}};object.erase_masks={{ObjectId::fromUint64(500),history}};
    e.record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
    auto base=renderPlan(e,1,dpr,generation);auto draw=base.referenceDrawList;
    for(unsigned i=0;i<copies;++i){auto next=e;next.record.objectId=ObjectId::fromUint64(i+1);if(unique){next.record.transform.tx=i*.03125;next.record.transform.ty=i*.015625;}
        if(tails){semantic::SweptCircleMask active;for(unsigned j=0;j<tails;++j)active.segments.push_back({{{110.+j*4,25},5},{{110.+j*4,105},5},{110.+j*4,45},{110.+j*4,85}});
            next.presentationEraseActive=true;next.presentationEraseMasks={{ObjectId::fromUint64(800+i),active}};}
        if(i==0)draw.entries[1]=next;else draw.entries.push_back(std::move(next));
    }
    return {base.frame,std::move(draw),{},false,true};
}
std::vector<std::uint8_t> reusePixels(NativeMetalTarget& target){auto acquired=target.acquire();auto* s=acquired.frame.surface;std::vector<std::uint8_t> pixels(s->width()*s->height()*4);target.release();require(target.readbackRgba(pixels).code==BackendSubmissionCode::kAccepted,"read pixels");return pixels;}
void drainCanonical(SkiaRenderer& renderer,NativeMetalTarget& target,const FramePlan& plan,const RenderPreparationBudget& budget){
    for(unsigned i=0;i<12000;++i){const auto r=renderer.renderFrame(target,plan,nullptr,&budget);if(r.code==BackendSubmissionCode::kAccepted)return;require(r.code==BackendSubmissionCode::kRetry,"canonical rejected");renderer.serviceResourceRetirements();}
    require(false,"canonical no progress");
}

FramePlan maskPlan(unsigned copies,unsigned variant){
 auto base=reusePlan(copies,0);auto draw=base.referenceDrawList;
 for(std::size_t i=1;i<draw.entries.size();++i){auto& e=draw.entries[i];
  auto mask=std::as_const(e.record.eraseMasks).front();auto& segment=std::get<semantic::SweptCircleMask>(mask.geometry).segments.front();
  segment.p0.position.x+=i*8;segment.p1.position.x+=i*8;segment.control1.x+=i*8;segment.control2.x+=i*8;
  mask.id=ObjectId::fromUint64(1000+i);e.record.eraseMasks={mask};
  if(variant==1){auto added=mask;added.id=ObjectId::fromUint64(2000+i);auto& s=std::get<semantic::SweptCircleMask>(added.geometry).segments.front();s.p0.position.x+=30;s.p1.position.x+=30;s.control1.x+=30;s.control2.x+=30;e.record.eraseMasks.push_back(added);}
  if(variant==2)e.record.eraseMasks.clear();
  if(variant==3){auto changed=mask;std::get<semantic::SweptCircleMask>(changed.geometry).segments.front().p0.radius=10;auto second=mask;second.id=ObjectId::fromUint64(3000+i);std::get<semantic::SweptCircleMask>(second.geometry).segments.front().p0.radius=9;e.record.eraseMasks={changed,second};}
  if(variant==4){auto changed=mask;std::get<semantic::SweptCircleMask>(changed.geometry).segments.front().p0.radius=10;auto second=mask;second.id=ObjectId::fromUint64(3000+i);std::get<semantic::SweptCircleMask>(second.geometry).segments.front().p0.radius=9;e.record.eraseMasks={second,changed};}
 }
 return {base.frame,std::move(draw),{},false,true};
}
void bodyLeaseIdentity(){
    const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);
    NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"body lease target");auto acquired=target.acquire();auto* canvas=acquired.frame.surface->getCanvas();
    auto budget=std::make_shared<resources::ResourceBudgetManager>();auto prepared=std::make_shared<PreparedGeometryCache>();auto pool=std::make_shared<SourceRetentionPool>();prepared->setRetentionPool(pool);
    SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=pool;cache.preparedGeometry=prepared;cache.options.maxEntries=16;RenderPreparationBudget slice;
    const auto finish=[&](const FramePlan& plan,std::uint64_t resource=0){for(unsigned n=0;n<10000;++n){cache.begin(plan,&target,*acquired.frame.surface,resource);PreparationAllowance allowance(slice);auto r=cache.prepare(*canvas,plan,slice,nullptr,nullptr,&allowance);if(r==SkiaRenderCache::Preparation::kReady)return;require(r==SkiaRenderCache::Preparation::kPending||r==SkiaRenderCache::Preparation::kResourcePending,"body lease prepare rejected");}require(false,"body lease stalled");};
    auto first=maskPlan(8,0);finish(first);require(cache.work.bodyPixelBytes>0&&cache.work.bodyPixelBuilds>0,"new body artifact not resident");auto masked=maskPlan(8,1);auto steps=cache.work.preparationCoverageSteps;finish(masked);require(cache.work.preparationCoverageSteps==steps&&cache.work.bodyPixelHits>0,"new body slot not actually hit");
    for(unsigned variant=0;variant<4;++variant){auto draw=masked.referenceDrawList;auto& entry=draw.entries.back();
        if(variant==0)entry.record.transform.tx+=.125;
        if(variant==1){entry.record.content.update([](auto& c){++std::get<semantic::CompactStrokeContent>(c).stroke.seed;});entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};}
        if(variant==2)draw.worldToView.tx+=1;
        FramePlan changed{masked.frame,std::move(draw),{},false,true};steps=cache.work.preparationCoverageSteps;finish(changed,variant==3?1:0);require(cache.work.preparationCoverageSteps>steps,"body pose/seed/camera/resource invalidation reused stale coverage");
    }
    // Source removal need not evict a legal optional body. Last clear must not
    // free its active GPU read before completion or retain it afterwards.
    auto emptyDraw=first.referenceDrawList;emptyDraw.entries.clear();FramePlan empty{first.frame,std::move(emptyDraw),{},false,true};finish(empty,1);require(cache.work.bodyPixelBytes>0,"source removal test did not retain body");
    const auto epoch=budget->beginGpuEpoch();require(bool(epoch),"body epoch");cache.clear();prepared->clear();require(budget->snapshot(resources::ResourceClass::kRaster).pendingRelease>0,"body GPU bytes retired before completion");require(budget->endGpuEpoch(*epoch)&&budget->completeGpuEpoch(*epoch),"body real completion");require(budget->used()==0,"body last source/clear leaked charged bytes");target.release();
}
void workAndPixels(const std::string& output){
 const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);
 NativeMetalTarget target(completion),reference(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted&&reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"body pixel targets");
 SkiaRenderer candidate,oracle;RenderCacheOptions limits;limits.maxEntries=16;candidate.setRenderCacheOptions(limits);RenderCacheOptions disabled;disabled.enabled=false;oracle.setRenderCacheOptions(disabled);RenderPreparationBudget slice;slice.maxCoverageSteps=1;
 const auto initial=maskPlan(8,0);drainCanonical(candidate,target,initial,slice);const auto cold=candidate.renderCacheWork();json report={{"cold8copies_steps",cold.preparationCoverageSteps},{"rows",json::array()}};
 bool workPass=true;
 for(unsigned variant=1;variant<=4;++variant){const auto plan=maskPlan(8,variant);const auto before=candidate.renderCacheWork();drainCanonical(candidate,target,plan,slice);const auto after=candidate.renderCacheWork();
  require(oracle.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"original no cache body mask reference");auto a=reusePixels(target),b=reusePixels(reference);std::vector<bool> edges(256*128,false);
  for(const auto& e:plan.referenceDrawList.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){auto edge=workingSetEdges(e,1);for(std::size_t i=0;i<edges.size();++i)edges[i]=edges[i]||edge[i];}
  std::size_t failures=0;for(std::size_t i=0;i<a.size();++i)if(!edges[i/4]&&std::abs(int(a[i])-int(b[i]))>2)++failures;
  const auto steps=after.preparationCoverageSteps-before.preparationCoverageSteps;report["rows"].push_back({{"variant",variant},{"coverage_steps_delta",steps},{"GPU_tiles_delta",after.gpuCoverageTiles-before.gpuCoverageTiles},{"nonAAfailures",failures}});require(!failures,"body mask pixel order/opacity mismatch");workPass&=steps==0;
 }
 report["mask_only_work_pass"]=workPass;std::ofstream(output)<<report.dump(2)<<'\n';std::cout<<report.dump()<<'\n';require(workPass,"CANONICAL_MASK_CHANGE_REPLAYS_UNCHANGED_BODY_COVERAGE");
}
#endif
}
int main(int argc,char** argv){try{require(argc==2,"output");
#if defined(__APPLE__)
workAndPixels(argv[1]);bodyLeaseIdentity();
#endif
return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

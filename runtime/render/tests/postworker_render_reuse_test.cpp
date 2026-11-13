// Actual renderer work regression. The independent null-cache renderer retains
// the original primitive/mask/alpha/order evaluation as pixel oracle.
#define main renderReusePriorFixtureMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include <nlohmann/json.hpp>
#include <iostream>
#include <filesystem>
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
void materialLifetimes(const std::filesystem::path& output){
    const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();
    GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);
    NativeMetalTarget target(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"lease target");auto acquired=target.acquire();
    auto budget=std::make_shared<resources::ResourceBudgetManager>();auto prepared=std::make_shared<PreparedGeometryCache>();auto pool=std::make_shared<SourceRetentionPool>();prepared->setRetentionPool(pool);
    SkiaRenderCache cache;cache.budget=budget;cache.sourcePool=pool;cache.preparedGeometry=prepared;cache.options.maxEntries=8;
    const auto plan=reusePlan(8,0);RenderPreparationBudget slice;auto* canvas=acquired.frame.surface->getCanvas();
    const auto complete=[&](const FramePlan& p,std::uint64_t resource=0){
        for(unsigned n=0;n<1000;++n){cache.begin(p,&target,*acquired.frame.surface,resource);PreparationAllowance allowance(slice);
            auto status=cache.prepare(*canvas,p,slice,nullptr,nullptr,&allowance);if(status==SkiaRenderCache::Preparation::kReady)return;
            require(status==SkiaRenderCache::Preparation::kPending||status==SkiaRenderCache::Preparation::kResourcePending,"cache refused material");}
        require(false,"cache material stalled");
    };
    complete(plan);const auto pixels=cache.work.retainedBytes;require(pixels>0&&cache.work.entries==8,"alias material fixture");
    const auto coverage=cache.work.preparationCoverageSteps;
    auto oneDraw=plan.referenceDrawList;oneDraw.entries.erase(oneDraw.entries.begin()+1,oneDraw.entries.end()-1);
    FramePlan one{plan.frame,std::move(oneDraw),{},false,true};complete(one);
    require(cache.hasPixels(one.referenceDrawList.entries.back())&&cache.work.retainedBytes==pixels&&cache.work.preparationCoverageSteps==coverage,"deleting source aliases retired still-live pixels");
    const auto epoch=budget->beginGpuEpoch();require(bool(epoch),"lease epoch");cache.clear();prepared->clear();
    require(budget->used(resources::ResourceClass::kRaster)>0&&budget->snapshot(resources::ResourceClass::kRaster).pendingRelease>0,"last alias released active GPU pixels early");
    require(budget->endGpuEpoch(*epoch)&&budget->completeGpuEpoch(*epoch),"lease completion");require(budget->used()==0,"last alias/source lease leaked");
    // Real semantic IDs remain distinct. Start from a warm aliased material
    // for every existing variant, then assert both invalidation work and pixels.
    nlohmann::json rows=nlohmann::json::array();
    NativeMetalTarget reference(completion);require(reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"invalidation reference");
    SkiaRenderer oracle;RenderCacheOptions disabled;disabled.enabled=false;oracle.setRenderCacheOptions(disabled);
    for(unsigned variant=0;variant<7;++variant){
        auto originalDraw=plan.referenceDrawList;
        if(variant==5)for(auto& e:originalDraw.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(e.command)){
            auto second=std::as_const(e.record.eraseMasks).front();second.id=ObjectId::fromUint64(902);
            auto& segment=std::get<semantic::SweptCircleMask>(second.geometry).segments.front();segment.p0.radius=7;segment.p1.radius=7;e.record.eraseMasks.push_back(second);
        }
        const FramePlan original{plan.frame,std::move(originalDraw),{},false,true};complete(original);
        const auto before=cache.work.preparationCoverageSteps;auto draw=original.referenceDrawList;auto& e=draw.entries.back();
        if(variant==0){auto mask=std::as_const(e.record.eraseMasks).front();mask.id=ObjectId::fromUint64(900);e.record.eraseMasks.push_back(mask);}
        if(variant==1)e.record.eraseMasks.clear();
        if(variant==2){auto mask=std::as_const(e.record.eraseMasks).front();std::get<semantic::SweptCircleMask>(mask.geometry).segments.front().p0.radius=8;e.record.eraseMasks={mask};}
        if(variant==3)e.record.transform.tx+=.125;
        if(variant==4){e.record.content.update([](auto& c){++std::get<semantic::CompactStrokeContent>(c).stroke.seed;});e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};}
        if(variant==5)std::swap(e.record.eraseMasks[0],e.record.eraseMasks[1]);
        if(variant==6){e.record.content=semantic::ObjectContent{e.record.content.get()};e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};}
        FramePlan changed{plan.frame,std::move(draw),{},false,true};complete(changed);
        const auto delta=cache.work.preparationCoverageSteps-before;
        require(variant==6?delta==0:delta>0,"changed material reused stale alias or equal immutable allocation rerasterized");
        require(drawReferencePlanToSkCanvas(*canvas,changed,nullptr,nullptr,true,&cache).code==BackendSubmissionCode::kAccepted,"cache material draw");
        require(oracle.renderFrame(reference,changed).code==BackendSubmissionCode::kAccepted,"independent changed material");
        const auto actual=reusePixels(target),expected=reusePixels(reference);std::vector<bool> edge(256*128,false);
        for(const auto& entry:changed.referenceDrawList.entries)if(std::holds_alternative<CompactStrokeReferenceCommand>(entry.command)){
            const auto local=workingSetEdges(entry,1);for(std::size_t i=0;i<edge.size();++i)edge[i]=edge[i]||local[i];}
        std::size_t errors=0,exact=0;for(std::size_t i=0;i<actual.size();++i){exact+=actual[i]!=expected[i];if(!edge[i/4]&&std::abs(int(actual[i])-int(expected[i]))>2)++errors;}
        require(errors==0,"material invalidation changed original independent pixels");
        rows.push_back({{"variant",variant},{"coverage_steps_delta",delta},{"exact_channel_differences",exact},{"non_AA_failures",errors}});cache.clear();prepared->clear();
    }
    target.release();std::ofstream(output)<<nlohmann::json{{"single_pixel_lease",true},{"last_alias_survived_source_removal",true},{"GPU_release_after_completion",true},{"variants",rows}}.dump(2)<<'\n';
}
void mechanism(const std::string& mode,const std::filesystem::path& output){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    nlohmann::json result;bool pass=true;
    {
        const auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");auto completion=std::make_shared<NativeMetalCompletion>();GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(backend);require(bool(completion->context),"Metal");
        if(mode=="canonical"){
            std::uint64_t one=0;for(unsigned copies:{1U,8U}){
                NativeMetalTarget target(completion),reference(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted&&reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"targets");
                SkiaRenderer renderer,oracle;RenderCacheOptions options;options.maxEntries=copies; // Force original whole-body job rather than per-object tile layers.
                renderer.setRenderCacheOptions(options);RenderCacheOptions disabled;disabled.enabled=false;oracle.setRenderCacheOptions(disabled);
                const auto plan=reusePlan(copies,0);RenderPreparationBudget slice;slice.maxCoverageSteps=1;drainCanonical(renderer,target,plan,slice);
                require(oracle.renderFrame(reference,plan).code==BackendSubmissionCode::kAccepted,"independent original reference");auto actual=reusePixels(target),expected=reusePixels(reference);auto edges=workingSetEdges(plan.referenceDrawList.entries[1],1);std::size_t errors=0;for(std::size_t j=0;j<actual.size();++j)if(!edges[j/4]&&std::abs(int(actual[j])-int(expected[j]))>2)++errors;const bool equal=errors==0;const auto w=renderer.renderCacheWork();
                if(copies==1)one=w.preparationCoverageSteps;
                result["rows"].push_back({{"copies",copies},{"coverage_steps",w.preparationCoverageSteps},{"body_builds",w.preparedBodyBuilds},{"pixels_equal",equal},{"CPU_tiles",w.cpuCoverageTiles}});
                require(equal,"canonical original pixels changed");if(copies==8)pass=w.preparationCoverageSteps<=one+8;
            }
            result["failure"]="IDENTICAL_CANONICAL_MATERIAL_REPLAYED_PER_OBJECT";
        }else if(mode=="warm"){
            NativeMetalTarget target(completion),reference(completion);require(target.resize(256,128).code==BackendSubmissionCode::kAccepted&&reference.resize(256,128).code==BackendSubmissionCode::kAccepted,"targets");
            SkiaRenderer renderer,oracle;RenderCacheOptions limited;limited.maxEntries=2;renderer.setRenderCacheOptions(limited);RenderCacheOptions disabled;disabled.enabled=false;oracle.setRenderCacheOptions(disabled);
            const auto plan=reusePlan(2,0);RenderPreparationBudget slice;drainCanonical(renderer,target,plan,slice);const auto before=renderer.renderCacheWork();const auto erase=reusePlan(2,1);EditingOverlay overlay(erase.frame);
            for(unsigned i=0;i<12000;++i){auto r=renderer.renderTransientOverlay(target,erase,&overlay);if(r.code==BackendSubmissionCode::kAccepted)break;require(r.code==BackendSubmissionCode::kRetry&&i<11999,"warm transient no progress");renderer.serviceResourceRetirements();}
            require(oracle.renderTransientOverlay(reference,erase,&overlay).code==BackendSubmissionCode::kAccepted,"transient oracle");const auto after=renderer.renderCacheWork();const bool equal=reusePixels(target)==reusePixels(reference);
            result={{"coverage_tiles",after.gpuCoverageTiles-before.gpuCoverageTiles},{"body_builds",after.preparedBodyBuilds-before.preparedBodyBuilds},{"pixels_equal",equal},{"failure","LIVE_ERASE_DISCARDED_QUALIFIED_CANONICAL_PIXELS"}};
            require(equal,"warm erase original pixels changed");pass=after.gpuCoverageTiles==before.gpuCoverageTiles;
        }else if(mode=="cold"){
            NativeMetalTarget target(completion);SkiaRenderer renderer;require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"warm body target");RenderPreparationBudget slice;drainCanonical(renderer,target,reusePlan(1,0),slice);const auto presents=target.presentCount();require(target.resize(1024,512).code==BackendSubmissionCode::kAccepted,"target");const auto plan=reusePlan(32,1,4,target.generation(),true);EditingOverlay overlay(plan.frame);
            const auto initial=reusePixels(target);const auto before=renderer.renderCacheWork();const auto r=renderer.renderTransientOverlay(target,plan,&overlay);const auto w=renderer.renderCacheWork();
            result={{"status",unsigned(r.code)},{"coverage_tiles",w.gpuCoverageTiles},{"coverage_steps",w.preparationCoverageSteps},{"provider_unchanged",reusePixels(target)==initial},{"failure","COLD_TRANSIENT_BYPASSED_AGGREGATE_ALLOWANCE"}};
            pass=r.code==BackendSubmissionCode::kRetry&&target.presentCount()==presents&&reusePixels(target)==initial&&w.preparationCoverageSteps-before.preparationCoverageSteps<=4096;
            unsigned attempts=1;for(;attempts<12000;++attempts){renderer.serviceResourceRetirements();const auto next=renderer.renderTransientOverlay(target,plan,&overlay);if(next.code==BackendSubmissionCode::kAccepted)break;require(next.code==BackendSubmissionCode::kRetry,"cold retry rejected");}
            result["completed_attempts"]=attempts;result["completed_present_count"]=target.presentCount();require(attempts<12000&&target.presentCount()==presents+1,"cold transient never converged");
        }else require(false,"mode");
    }
    message(pool,"drain");result["work_regression_pass"]=pass;std::ofstream(output)<<result.dump(2)<<'\n';std::cout<<result.dump()<<'\n';require(pass,result["failure"].get<std::string>().c_str());
}
#endif
}
int main(int argc,char** argv){try{require(argc==3,"mode output");
#if defined(__APPLE__)
if(std::string_view(argv[1])=="leases")materialLifetimes(argv[2]);else mechanism(argv[1],argv[2]);
#endif
return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

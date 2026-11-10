// Structural renderer regression. Metal synchronization is a test observation,
// not the asynchronous Web performance oracle.
#define main g5OriginalCanonicalTestMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/transient_display_state.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <iostream>

namespace {
ReferenceTraversalEntry movingBody(){
    auto e=source();e.record.kind=semantic::ObjectKind::kVectorStroke;e.record.kindVersion=3;
    e.record.content.update([](auto& value){auto& c=std::get<semantic::CompactStrokeContent>(value);c.stroke.knots.clear();for(unsigned i=0;i<32;++i)c.stroke.knots.push_back({20.+i*5,64,32768});});
    e.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(e.record.content)};
    return e;
}
void saveBuffer(const std::filesystem::path& path,const std::vector<std::uint8_t>& bytes){std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());require(bool(f),"persist moving pixels");}
semantic::EraseMaskRecord movingMask(unsigned count) {
    semantic::SweptCircleMask sweep;
    for (unsigned i=0;i<count;++i) {
        const double x=44.+i*.7;
        sweep.segments.push_back({{{x,64},4},{{x+.7,64},4},{x+.2,64},{x+.5,64}});
    }
    return {ObjectId::fromUint64(990),std::move(sweep)};
}
FramePlan movingPlan(const ReferenceTraversalEntry& body,unsigned prefix,std::uint64_t frame) {
    const auto base=renderPlan(body,frame,1).frame;
    FrameState state{base.viewId,base.camera,{0,0,256,128},{256,128,256,128,1,1},base.sceneGeneration,base.sceneReadToken,base.surfaceGeneration,base.metricsGeneration,base.frameId,{0,0,256,128}};
    ReferenceDrawList draw{state};draw.viewportClip=state.viewportClip;
    draw.entries.clear();
    for (unsigned i=0;i<8;++i) {
        auto entry=body;
        entry.record.objectId=ObjectId::fromUint64(i+1);
        entry.record.transform.tx=i*2.;
        entry.presentationEraseActive=prefix!=0;
        if(prefix)entry.presentationEraseMasks={movingMask(prefix)};
        draw.entries.push_back(std::move(entry));
    }
    return {state,std::move(draw),{},false,true};
}
#if defined(__APPLE__)
using DisplayTarget=NativeMetalTarget;
std::vector<std::uint8_t> pixels(DisplayTarget& target,const FramePlan& plan){std::vector<std::uint8_t> value(std::size_t(plan.frame.metrics.physicalWidth)*plan.frame.metrics.physicalHeight*4);require(target.readbackRgba(value).code==BackendSubmissionCode::kAccepted,"moving actual target read");return value;}
void settle(SkiaRenderer& renderer,DisplayTarget& target,const FramePlan& plan,const std::shared_ptr<NativeMetalCompletion>& completion){EditingOverlay overlay{plan.frame};for(unsigned i=0;i<2048;++i){auto result=renderer.renderTransientOverlay(target,plan,&overlay);completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();if(result.code==BackendSubmissionCode::kAccepted)return;require(result.code==BackendSubmissionCode::kRetry,"fixed moving hold rejected");}require(false,"fixed moving hold did not converge");}
bool movingProgress(const std::filesystem::path& output) {
    std::filesystem::create_directories(output);
    auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
    auto completion=std::make_shared<NativeMetalCompletion>();
    GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
    completion->context=GrDirectContexts::MakeMetal(backend);
    require(bool(completion->context),"moving real Metal context");
    DisplayTarget target(completion);
    require(target.resize(256,128).code==BackendSubmissionCode::kAccepted,"moving target");
    const auto body=movingBody();
    const auto initial=movingPlan(body,0,1);
    auto acquired=target.acquire();
    require(drawReferencePlanToSkCanvas(*acquired.frame.surface->getCanvas(),initial).code==BackendSubmissionCode::kAccepted,"moving initial reference");
    target.release();require(target.present().code==BackendSubmissionCode::kAccepted,"moving initial present");
    SkiaRenderer renderer;
    RenderCacheOptions options;
    options.maxEntries=1; // Structural forcing only, original time/step/byte limits.
    renderer.setRenderCacheOptions(options);
    unsigned accepted=0,changed=0,retries=0;
    for(unsigned prefix=1;prefix<=60;++prefix) {
        const auto plan=movingPlan(body,prefix,prefix+1);
        const auto before=pixels(target,plan);const auto presents=target.presentCount();
        EditingOverlay overlay{plan.frame};
        const auto result=renderer.renderTransientOverlay(target,plan,&overlay);
        completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
        const auto after=pixels(target,plan);
        if(result.code==BackendSubmissionCode::kAccepted){++accepted;changed+=before!=after;}
        else {require(result.code==BackendSubmissionCode::kRetry,"moving rejected feasible prefix");++retries;require(before==after&&target.presentCount()==presents,"moving Retry changed actual provider bytes");}
        const auto work=renderer.renderCacheWork();
        std::cout<<nlohmann::json{{"prefix",prefix},{"accepted",result.code==BackendSubmissionCode::kAccepted},{"changed",before!=after},{"coverage_steps",work.preparationCoverageSteps},{"body_builds",work.preparedBodyBuilds},{"GPU_tiles",work.gpuCoverageTiles},{"CPU_tiles",work.cpuCoverageTiles}}.dump()<<'\n';
    }
    const auto fixed=movingPlan(body,60,100);
    settle(renderer,target,fixed,completion);
    const auto work=renderer.renderCacheWork();
    saveBuffer(output/"held-final.rgba",pixels(target,fixed));
    std::cout<<nlohmann::json{{"moving_summary",true},{"forced_max_entries",1},{"accepted_during_moves",accepted},{"changed_during_moves",changed},{"retries",retries},{"fixed_hold_completed",true},{"body_builds",work.preparedBodyBuilds},{"CPU_tiles",work.cpuCoverageTiles}}.dump()<<std::endl;
    require(work.cpuCoverageTiles==0,"moving CPU coverage fallback");
    require(work.preparedBodyBuilds<=2,"moving repeatedly compiled same immutable body");
    const bool moving=accepted>=4&&changed>=4;
    renderer.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);renderer.serviceResourceRetirements();
    if(!moving)std::cerr<<"MP01_MOVING_PREFIX_STARVATION_WITH_FEASIBLE_FIXED_HOLD\n";
    return moving;
}
#endif
}
int main(int argc,char** argv){try{
#if defined(__APPLE__)
    const auto out=argc>1?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"axiom-moving-erase";
    return movingProgress(out)?0:1;
#else
    throw std::runtime_error("actual GPU moving-prefix test requires Metal on this host");
#endif
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}

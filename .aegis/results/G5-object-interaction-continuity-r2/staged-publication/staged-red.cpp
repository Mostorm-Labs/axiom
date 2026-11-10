#define main g5IncludedDisplayTestMain
#include "/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/runtime/render/tests/canonical_prepared_body_test.cpp"
#undef main
#include <filesystem>
#include <nlohmann/json.hpp>
#include <iostream>

namespace {
ReferenceTraversalEntry continuityShape(std::uint64_t id,double x,double y,float red,float green,float blue) {
    semantic::ObjectRecord object;object.id=ObjectId::fromUint64(id);object.kind=semantic::ObjectKind::kShape;object.kind_version=1;
    object.content=semantic::ShapeContent{1,90,52};object.transform.tx=x;object.transform.ty=y;
    object.properties.entries={{0x100U,semantic::FillStyleValue{semantic::SolidFill{{red,green,blue,1}}}}};
    const auto projection=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1});
    return {projection.records.front(),true,ShapeReferenceCommand{std::get<semantic::ShapeContent>(object.content)}};
}
FramePlan densePlan(const ReferenceTraversalEntry& line,ReferenceTraversalEntry active,
                    unsigned dpr,unsigned frame,double offset) {
    const auto base=renderPlan(line,frame,dpr).frame;
    FrameState state{base.viewId,base.camera,{0,0,1431,1010},
        {1431,1010,1431*dpr,1010*dpr,float(dpr),1},
        semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},
        MetricsGeneration{1},FrameId{frame},{0,0,1431,1010}};
    ReferenceDrawList draw{state};draw.viewportClip=state.viewportClip;
    for(unsigned i=0;i<19;++i) {
        auto copy=line;copy.record.objectId=ObjectId::fromUint64(100+i);
        copy.record.transform.ty=100+i*17;
        copy.record.worldBounds={0,float(130+i*17),1431,float(200+i*17)};
        draw.entries.push_back(std::move(copy));
        if(i==9) {
            active.record.transform.tx=280+offset;active.record.transform.ty=120;
            active.record.worldBounds={float(280+offset),120,float(370+offset),172};
            draw.entries.push_back(active);
        }
    }
    return {state,std::move(draw),{},false,true};
}
}
int main(int argc,char** argv) {
#if defined(__APPLE__)
    try {
        const auto out=std::filesystem::path(argc>1?argv[1]:"out/staged-publication-red");
        std::filesystem::create_directories(out);
        auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
        {
            auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
            auto completion=std::make_shared<NativeMetalCompletion>();
            GrMtlBackendContext backend{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};
            completion->context=GrDirectContexts::MakeMetal(backend);
            require(bool(completion->context),"staged Metal context");
            auto line=source();line.record.kind=semantic::ObjectKind::kVectorStroke;line.record.kindVersion=3;
            line.record.content.update([](auto& value){std::get<semantic::CompactStrokeContent>(value).stroke.config.opacity=.55;});
            line.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(line.record.content)};
            for(unsigned i=0;i<2;++i) {
                semantic::SweptCircleMask mask;const double x=90+75*i;
                mask.segments.push_back({{{x,20},7},{{x,105},7},{x,40},{x,80}});
                line.record.eraseMasks.push_back({ObjectId::fromUint64(900+i),mask});
            }
            auto active=continuityShape(777,280,120,.9,.25,.15);
            NativeMetalTarget target(completion);require(target.resize(1431,1010).code==BackendSubmissionCode::kAccepted,"staged target");
            auto acquired=target.acquire();acquired.frame.surface->getCanvas()->clear(SK_ColorMAGENTA);target.release();
            SkiaRenderer candidate;RenderCacheOptions options;options.maxEntries=1;candidate.setRenderCacheOptions(options);
            std::ofstream trace(out/"attempts.jsonl");unsigned accepted=0,retry=0,hold=0;
            for(unsigned i=0;i<60;++i) {
                auto plan=densePlan(line,active,1,i+1,i);
                EditingOverlay overlay(plan.frame);
                require(overlay.update({.selectedObject=active.record.objectId,.worldBounds=plan.referenceDrawList.entries[10].record.worldBounds,
                    .objectTransform=plan.referenceDrawList.entries[10].record.transform,.selected=true,.active=true}),"staged overlay");
                const auto result=candidate.renderTransientOverlay(target,plan,&overlay);
                completion->context->flushAndSubmit(GrSyncCpu::kYes);candidate.serviceResourceRetirements();
                accepted+=result.code==BackendSubmissionCode::kAccepted;retry+=result.code==BackendSubmissionCode::kRetry;
                const auto work=candidate.renderCacheWork();
                trace<<nlohmann::json{{"move",i},{"status",int(result.code)},{"message",result.message},{"presents",target.presentCount()},
                    {"rasterized",work.rasterizedGroups},{"coverage_steps",work.preparationCoverageSteps},
                    {"body_builds",work.preparedBodyBuilds},{"body_compile",work.preparedCompileInputs},
                    {"static_builds",candidate.objectInteractionWork().builds},{"static_hits",candidate.objectInteractionWork().hits}}.dump()<<std::endl;
                require(result.code!=BackendSubmissionCode::kRejected,"staged rejected");
            }
            auto fixed=densePlan(line,active,1,1000,59);EditingOverlay overlay(fixed.frame);
            require(overlay.update({.selectedObject=active.record.objectId,.worldBounds=fixed.referenceDrawList.entries[10].record.worldBounds,
                .objectTransform=fixed.referenceDrawList.entries[10].record.transform,.selected=true,.active=true}),"hold overlay");
            for(;hold<512;++hold) {
                const auto result=candidate.renderTransientOverlay(target,fixed,&overlay);
                completion->context->flushAndSubmit(GrSyncCpu::kYes);candidate.serviceResourceRetirements();
                require(result.code!=BackendSubmissionCode::kRejected,"fixed hold rejected");
                if(result.code==BackendSubmissionCode::kAccepted)break;
            }
            const auto work=candidate.renderCacheWork();
            nlohmann::json result{{"moves",60},{"moving_accepted",accepted},{"moving_retry",retry},{"hold_attempts",hold+1},
                {"hold_recovered",hold<512},{"presents",target.presentCount()},{"body_builds",work.preparedBodyBuilds},
                {"static_builds",candidate.objectInteractionWork().builds},{"static_hits",candidate.objectInteractionWork().hits}};
            std::ofstream(out/"summary.json")<<result.dump(2)<<'\n';std::cout<<result.dump()<<std::endl;
            candidate.clearRenderCache();completion->context->flushAndSubmit(GrSyncCpu::kYes);candidate.serviceResourceRetirements();
            require(hold<512,"fixed hold failed to recover");
            require(accepted>0,"STAGED_MOVING_PUBLICATION_STARVED");
        }
        message(pool,"drain");return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
#else
    (void)argc;(void)argv;return 2;
#endif
}

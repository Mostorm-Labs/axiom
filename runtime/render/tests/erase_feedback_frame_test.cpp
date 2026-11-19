#define main effPreparedReferenceMain
#include "canonical_prepared_body_test.cpp"
#undef main
#include "../src/skia_erase_mask.hpp"
#include "../src/transient_display_state.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include <nlohmann/json.hpp>
#include <iostream>

#if defined(__APPLE__)
namespace {
FramePlan effPlan(unsigned prefix,std::uint64_t frame,unsigned dpr,bool replacement=false){
    auto entry=source();semantic::ObjectRecord object;object.id=entry.record.objectId;
    object.kind=semantic::ObjectKind::kVectorStroke;object.kind_version=3;object.content=entry.record.content.get();
    auto& stroke=std::get<semantic::CompactStrokeContent>(object.content).stroke;
    stroke.config.opacity=.55;stroke.knots.clear();
    for(unsigned i=0;i<700;++i)stroke.knots.push_back({5+i*.35,64+35*std::sin(i*.055),40000});
    entry.record=projectRuntimeScene({&object,1},semantic::SemanticGeneration{1}).records.front();
    entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};
    if(prefix){
        std::vector<foundation::WorldPoint> trace;
        for(unsigned i=0;i<=prefix;++i)trace.push_back({float(20+i*.4),float(64+27*std::sin(i*.032))});
        if(replacement)for(auto& p:trace)p.y=128-p.y;
        auto mask=interaction::partialEraseGeometry(trace,entry.record.transform,6);require(bool(mask),"feedback mask");
        entry.presentationEraseActive=true;entry.presentationEraseMasks.push_back({{},std::move(*mask)});
    }
    auto p=renderPlan(entry,frame,dpr);auto draw=p.referenceDrawList;
    for(unsigned i=0;i<3;++i){auto copy=entry;copy.record.objectId=ObjectId::fromUint64(2+i);copy.presentationOpacity=.65F+.1F*i;draw.entries.push_back(std::move(copy));}
    return {p.frame,std::move(draw),p.background,p.transparentBackground,p.isolateEraseMasks};
}
void effRun(const char* output,bool requireReduction,const char* baseline=nullptr){
    auto pool=message(message(reinterpret_cast<id>(objc_getClass("NSAutoreleasePool")),"alloc"),"init");
    nlohmann::json rows=nlohmann::json::array();
    {
        auto completion=std::make_shared<NativeMetalCompletion>();auto device=MTLCreateSystemDefaultDevice(),queue=message(device,"newCommandQueue");
        GrMtlBackendContext context{sk_cfp<GrMTLHandle>(device),sk_cfp<GrMTLHandle>(queue)};completion->context=GrDirectContexts::MakeMetal(context);require(bool(completion->context),"feedback Metal");
        for(unsigned dpr:{1U,2U,4U}){
            NativeMetalTarget target(completion);require(target.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"feedback target");
            SkiaRenderer renderer;RenderCacheOptions opts;opts.maxBytes=512U*1048576;opts.sharedMaxBytes=1024U*1048576;opts.maxEntryBytes=128U*1048576;renderer.setRenderCacheOptions(opts);
            auto canonical=effPlan(0,1,dpr);
            for(unsigned i=0;;++i){auto result=renderer.renderFrame(target,canonical);require(i<1024&&result.code!=BackendSubmissionCode::kRejected,"feedback canonical warm");if(result.code==BackendSubmissionCode::kAccepted)break;}
            auto owner=std::make_shared<int>(1);TransientDisplayIdentity identity{owner,1,1};std::uint64_t id=2;
            for(auto [prefix,replaced]:{std::pair{400U,false},{402U,false},{404U,false},{408U,false},{404U,true},{410U,true}}){
                auto plan=effPlan(prefix,id++,dpr,replaced);EditingOverlay overlay{plan.frame};++identity.inputRevision;
                const auto before=eraseFeedbackDrawWork.sweptDraws;const auto previous=renderer.renderCacheWork();const auto presentBefore=target.presentCount();
                auto result=renderer.renderTransientOverlay(target,plan,&overlay,identity);require(result.code==BackendSubmissionCode::kAccepted,"warm feedback pending/rejected");
                require(target.presentCount()==presentBefore+1,"feedback omitted frame");
                const auto draws=eraseFeedbackDrawWork.sweptDraws-before;const auto work=renderer.renderCacheWork();
                NativeMetalTarget reference(completion);require(reference.resize(256*dpr,128*dpr).code==BackendSubmissionCode::kAccepted,"oracle target");
                SkiaRenderer independent;RenderCacheOptions noCache;noCache.enabled=false;independent.setRenderCacheOptions(noCache);
                require(independent.renderTransientOverlay(reference,plan,&overlay).code==BackendSubmissionCode::kAccepted,"independent full mask oracle");
                std::vector<std::uint8_t> actual(256*128*dpr*dpr*4),expected(actual.size());require(target.readbackRgba(actual).code==BackendSubmissionCode::kAccepted&&reference.readbackRgba(expected).code==BackendSubmissionCode::kAccepted,"feedback readback");
                // Both use the same GPU body policy in this test when cache is
                // disabled: qualify erase interiors independently of body AA.
                auto geometry=plan.referenceDrawList.entries[1];
                for(const auto& mask:geometry.presentationEraseMasks)geometry.record.eraseMasks.push_back(mask);
                const auto edges=workingSetEdges(geometry,dpr);
                std::size_t mismatch=0,edgeDifferences=0;for(std::size_t i=0;i<actual.size();++i)if(std::abs(int(actual[i])-int(expected[i]))>2){
                    if(edges[i/4]){++edgeDifferences;continue;}++mismatch;
                    if(mismatch<12)std::cerr<<"diff dpr="<<dpr<<" prefix="<<prefix<<" at="<<(i/4)%(256*dpr)<<","<<(i/4)/(256*dpr)<<" channel="<<i%4<<" values="<<unsigned(actual[i])<<","<<unsigned(expected[i])<<"\n";
                }
                rows.push_back({{"dpr",dpr},{"prefix",prefix},{"replaced",replaced},{"swept_draws",draws},{"full_replay_draws",4*prefix},{"damage_frames",work.eraseDamageFrames-previous.eraseDamageFrames},{"nonAA_channels_over2",mismatch},{"independently_classified_AA_channels_over2",edgeDifferences}});
                std::ofstream(output)<<nlohmann::json{{"rows",rows}}.dump(2)<<'\n';
                require(mismatch==0,"feedback differs full sequential oracle outside fixed independent AA geometry");
                if(!requireReduction){const auto dest=std::string(output)+"-"+std::to_string(dpr)+"-"+std::to_string(prefix)+"-"+std::to_string(replaced)+".rgba";
                    std::ofstream(dest,std::ios::binary).write(reinterpret_cast<const char*>(actual.data()),actual.size());
                }else if(baseline){const auto base=std::string(baseline)+"-"+std::to_string(dpr)+"-"+std::to_string(prefix)+"-"+std::to_string(replaced)+".rgba";
                    std::ifstream old(base,std::ios::binary);std::vector<std::uint8_t> original{std::istreambuf_iterator<char>(old),{}};
                    require(original==actual,"optimized feedback differs exact saved B pixels");
                }
                if(requireReduction&&prefix==402)require(draws<4*prefix/4,"FEEDBACK_REPLAYS_OFFDAMAGE_COMPLETE_MASK_HISTORY");
            }
        }
    }
    message(pool,"drain");
}
}
#endif
int main(int argc,char** argv)try{
#if defined(__APPLE__)
 require(argc>=2,"output path");effRun(argv[1],argc<3||std::string_view(argv[2])!="--observe-only",argc>=4?argv[3]:nullptr);
#else
 (void)argc;(void)argv;
#endif
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}

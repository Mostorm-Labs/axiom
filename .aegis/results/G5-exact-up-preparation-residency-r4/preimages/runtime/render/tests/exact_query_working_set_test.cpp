// Regresses the dependency of semantic exact queries on draw-history flattening.
// The fixture is the captured public-Web 16-transform +24copy canonical state.
#define main qwExistingFixtureMain
#include "ur2d_selection_up_test.cpp"
#undef main
#include "canvas/semantic/snapshot.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "../src/skia_mask_history.hpp"
#include <set>
#include <cstdlib>

namespace {
std::vector<RuntimeSceneRecord> captured(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);require(bool(input),"actual canonical fixture absent");
    std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(input),{}};
    auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
    require(decoded.ok(),"actual canonical fixture decode");std::vector<RuntimeSceneRecord> records;
    for(const auto& object:decoded.snapshot->objects) {
        RuntimeSceneRecord record;record.objectId=object.id;record.kind=object.kind;record.kindVersion=object.kind_version;
        record.content=object.content;record.transform=object.transform;record.properties=object.properties;record.eraseMasks=object.erase_masks;
        records.push_back(std::move(record));
    }
    require(records.size()==40,"actual forty canonical objects");return records;
}
semantic::EraseMaskGeometry queryFor(const RuntimeSceneRecord& record,unsigned moves,unsigned round=0) {
    std::vector<foundation::WorldPoint> trace{{float(420-round*18),160}};
    for(unsigned i=1;i<=moves;++i)trace.push_back({float(420-round*18+7*std::sin(i*.17)),float(160+170.*i/moves)});
    auto mask=interaction::partialEraseGeometry(trace,record.transform,16);require(bool(mask),"fixture invertible transform");return *mask;
}
json workDelta(const EraseMaskPathCache::Work& before,const EraseMaskPathCache::Work& after) {
    return {{"memo_hits",after.equivalentQueryHits-before.equivalentQueryHits},
        {"draw_primitive_builds",after.history.primitiveBuilds-before.history.primitiveBuilds},
        {"history_denied",after.history.denied-before.history.denied},{"path_builds",after.builds-before.builds},
        {"query_unions",after.history.queryUnionOps-before.history.queryUnionOps},
        {"history_unions",after.history.historyUnionOps-before.history.historyUnionOps},
        {"body_uses",after.exact.bodyUses-before.exact.bodyUses},
        {"body_compile",after.exact.compileCalls-before.exact.compileCalls},
        {"prepared_compile",after.exact.preparedCompileCalls-before.exact.preparedCompileCalls},
        {"local_bytes",after.bytes}};
}
}
int main(int argc,char** argv) {
    try {
        require(argc==5,"usage mode initial.axsnap growth-final.axsnap output.json");
        const std::string mode=argv[1];const bool oracle=mode=="--oracle",verify=mode=="--verify";
        auto initial=captured(argv[2]),history=captured(argv[3]);
        std::set<std::array<double,6>> transforms;for(const auto& record:initial){const auto& t=record.transform;transforms.insert({t.a,t.b,t.c,t.d,t.tx,t.ty});}
        require(transforms.size()==16,"actual sixteen local transforms");
        json output={{"mode",mode},{"objects",40},{"transforms",16},{"cases",json::array()}};
        bool structural=true;
        for(unsigned scenario=0;scenario<2;++scenario) {
            auto& records=scenario?history:initial;
            std::size_t equivalentPairs=0;
            for(std::size_t i=0;i<records.size();++i)for(std::size_t j=0;j<i;++j){
                const auto& a=records[i];const auto& b=records[j];
                if(a.transform!=b.transform||a.kindVersion!=b.kindVersion||a.content!=b.content||a.eraseMasks.size()!=b.eraseMasks.size())continue;
                bool same=true;for(std::size_t h=0;h<a.eraseMasks.size();++h)if(a.eraseMasks[h].geometry!=b.eraseMasks[h].geometry)same=false;
                if(same){++equivalentPairs;break;}
            }
            auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
            EraseMaskPathCache cache;PreparedGeometryCache bodies;RenderCacheOptions options;
            options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;
            std::vector<std::shared_ptr<MaskHistoryPlan>> drawPins;
            if(!oracle) {
                // Match the live renderer: admitted draw plans may remain
                // pinned by resident canonical/edit pixels while Up queries run.
                for(const auto& record:records)if(auto plan=cache.history(record.eraseMasks,budget,8U*1024U*1024U)) {
                    // A bounded active draw working set; stale resident plans
                    // remain reclaimable. Fully pinned exhaustion is a distinct
                    // correctness fallback case, not an impossible reuse claim.
                    const auto maxPins=std::getenv("QW_DRAW_PINS")?std::strtoul(std::getenv("QW_DRAW_PINS"),nullptr,10):8;
                    if(drawPins.size()<maxPins)drawPins.push_back(std::move(plan));
                }
                for(const auto& record:records){ReferenceTraversalEntry entry;entry.record.content=record.content;
                    entry.command=CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(record.content)};
                    const auto body=bodies.get(entry,options,budget,true,PreparedConsumer::kExact);require(body&&body->ready(),"warm actual exact body");}
            }
            const unsigned moves=scenario?160U:1600U;auto before=cache.work();json decisions=json::array();
#if defined(AXIOM_EXACT_QUERY_WORKING_SET)
            cache.beginCohort();
#endif
            const auto start=std::chrono::steady_clock::now();
            for(const auto& record:records){const auto mask=queryFor(record,moves,scenario?5:0);
                decisions.push_back(unsigned(oracle?intersectEraseGeometryComponentOracle(record,mask):cache.query(record,mask,budget,8U*1024U*1024U,&bodies,&options)));}
#if defined(AXIOM_EXACT_QUERY_WORKING_SET)
            cache.endCohort();
#endif
            const auto after=cache.work();const auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
            output["cases"].push_back({{"scenario",scenario?"actual-growth-history":"actual1600"},{"moves",moves},
                {"decisions",decisions},{"work",workDelta(before,after)},{"ms",elapsed},{"draw_pins",drawPins.size()},{"equivalent_repeated_bases",equivalentPairs}});
            if(scenario)structural=after.history.primitiveBuilds==before.history.primitiveBuilds &&
                // Captured six-round canonical history has13 repeated complete
                // bases; no-op rounds leave other copies with different history.
                // The prechange capture independently proves that cardinality.
                after.equivalentQueryHits-before.equivalentQueryHits>=equivalentPairs;
            require(after.bytes<=8U*1024U*1024U&&budget->used()<=512U*1024U*1024U,"working set exceeded existing quotas");
            drawPins.clear();cache.clear();bodies.clear();require(budget->used()==0,"actual working set leaked leases");
        }
        output["exact_queries_independent_of_draw_flattening"]=structural;
#if defined(AXIOM_EXACT_QUERY_WORKING_SET)
        if(mode=="--dynamic"){
            auto records=initial;auto budget=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{256U*1024U*1024U,512U*1024U*1024U});
            EraseMaskPathCache cache;PreparedGeometryCache bodies;RenderCacheOptions options;options.maxBytes=256U*1024U*1024U;options.sharedMaxBytes=512U*1024U*1024U;options.maxEntryBytes=64U*1024U*1024U;
            std::vector<std::shared_ptr<MaskHistoryPlan>> pins(records.size());json rounds=json::array();
            const auto draw=[&]{for(std::size_t i=0;i<records.size();++i)pins[i]=cache.history(records[i].eraseMasks,budget,8U*1024U*1024U,pins[i]);};
            draw();
            for(unsigned round=0;round<6;++round)for(unsigned tiny=0;tiny<2;++tiny){
                const auto before=cache.work();const auto start=std::chrono::steady_clock::now();json decisions=json::array();cache.beginCohort();
                for(std::size_t i=0;i<records.size();++i){
                    auto& record=records[i];auto mask=queryFor(record,160,round);
                    if(tiny){const float x=420-round*18+7*std::sin(160*.17),y=330;const std::vector<foundation::WorldPoint> trace{{x+1,y+1},{x+2,y+4}};mask=*interaction::partialEraseGeometry(trace,record.transform,16);}
                    const auto hit=cache.query(record,mask,budget,8U*1024U*1024U,&bodies,&options);decisions.push_back(unsigned(hit));
                    require(hit!=SelectionGeometryHit::kInvalid,"dynamic query invalid");
                    if(hit==SelectionGeometryHit::kHit)record.eraseMasks.push_back({ObjectId::fromUint64(100000+round*1000+tiny*100+i),std::move(mask)});
                }
                cache.endCohort();const auto after=cache.work();rounds.push_back({{"round",round},{"tiny",tiny},{"decisions",decisions},{"ms",std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()},{"work",workDelta(before,after)}});draw();
            }
            output["dynamic6"]=rounds;pins.clear();cache.clear();bodies.clear();require(budget->used()==0,"dynamic leases");
        }
        if(!oracle){
            auto budget=std::make_shared<resources::ResourceBudgetManager>();
            EraseMaskPathCache paths;PreparedGeometryCache bodies;RenderCacheOptions options;
            auto record=source(512,2);const auto mask=queryFor(record,80);
            paths.beginCohort();
            const auto expected=intersectEraseGeometryComponentOracle(record,mask);
            require(paths.query(record,mask,budget,8*1024*1024,&bodies,&options)==expected,"cohort exact oracle");
            const auto before=paths.work();
            auto alias=record;for(auto& old:alias.eraseMasks)old.id=ObjectId::fromUint64(1000+old.id.bytes[0]);
            require(paths.query(alias,mask,budget,8*1024*1024,&bodies,&options)==expected,"canonical ID fork decision");
            require(paths.work().equivalentQueryHits==before.equivalentQueryHits+1,"complete geometry ID fork missed memo");
            auto fork=alias;std::get<semantic::SweptCircleMask>(fork.eraseMasks.back().geometry).segments.back().control2.x+=.1;
            const auto memoBefore=paths.work().equivalentQueryHits;
            require(paths.query(fork,mask,budget,8*1024*1024,&bodies,&options)==intersectEraseGeometryComponentOracle(fork,mask),"history content fork");
            require(paths.work().equivalentQueryHits==memoBefore,"same ID geometry fork aliased");
            paths.endCohort();
            // Query geometry/proof fits but the old full-path estimate does
            // not. A proven result must remain reusable without a SkPath pin.
            paths.clear();auto pressureRecord=source(512,2);const auto longQuery=queryFor(pressureRecord,160);
            paths.beginCohort();const auto pressureExpected=intersectEraseGeometryComponentOracle(pressureRecord,longQuery);
            require(paths.query(pressureRecord,longQuery,budget,128*1024,&bodies,&options)==pressureExpected,"path denial reference");
            const auto deniedBefore=paths.work();
            require(paths.query(pressureRecord,longQuery,budget,128*1024,&bodies,&options)==pressureExpected,"denied path repeat");
            require(paths.work().equivalentQueryHits==deniedBefore.equivalentQueryHits+1&&paths.work().builds==deniedBefore.builds,
                    "complete result still depends on path residency");paths.endCohort();paths.clear();
            // clear with an externally held draw plan must not double-charge
            // its source when the same allocation is discovered again.
            auto held=paths.history(record.eraseMasks,budget,8*1024*1024);require(bool(held),"held draw plan");
            paths.clear();const auto retained=budget->used();auto repeated=paths.history(record.eraseMasks,budget,8*1024*1024);
            require(repeated&&repeated->localSource==held->localSource,"clear duplicated local source charge");
            paths.beginCohort();paths.clear();paths.endCohort();held.reset();repeated.reset();bodies.clear();
            require(budget->used()==0,"cohort clear/unwind source lease leak");
            (void)retained;
            auto tiny=std::make_shared<resources::ResourceBudgetManager>(resources::BudgetLimits{1,1});
            paths.beginCohort();require(paths.query(record,mask,tiny,128,&bodies,&options)==expected,"insufficient quota changed reference result");paths.endCohort();paths.clear();bodies.clear();require(tiny->used()==0,"tiny quota leaked");
            output["lifecycle"]={{"canonical_ID_fork",true},{"geometry_fork",true},{"held_source_clear_shared",true},{"tiny_reference_parity",true},{"result_reuse_without_path_residency",true},{"all_leases_released",true}};
        }
#endif
        std::ofstream(argv[4])<<output.dump(2)<<'\n';std::cout<<output.dump()<<'\n';
        // Old production fails here because query() rebuilds the draw plan.
        if(verify&&!structural){std::cerr<<"exact query rebuilt draw-history primitives\n";return 1;}
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}

// Measurement only: the original cpu_pending_runtime_test long input and owners.
// The synchronous native kernel surrogate does not measure Web/Worker transport.
#define main cpuTailPriorFixtureMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "../../semantic/src/sha256_internal.hpp"
#include "../../interaction/src/cpu_pending_erase_state.hpp"
#include "../../render/src/cpu_worker_protocol.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <new>

// Keep the accepted long test's allocation route (no injected failures).
void* operator new(std::size_t n) { if (auto p=std::malloc(n?n:1)) return p; throw std::bad_alloc(); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }

using json=nlohmann::json;
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess { public:
    static void profile(interaction::CanvasEditingRuntime& r) {
        render::RenderCacheOptions o;
        o.maxBytes=256U*1024U*1024U; o.sharedMaxBytes=512U*1024U*1024U;
        o.maxEntryBytes=64U*1024U*1024U; r.skiaRenderer_.setRenderCacheOptions(o);
    }
    static auto work(const interaction::CanvasEditingRuntime& r) { return r.skiaRenderer_.renderCacheWork(); }
    static const auto& state(const interaction::CanvasEditingRuntime& r) { return *r.cpuPending_; }
};
}
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t ns(Clock::time_point from) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-from).count();
}
std::string hash(std::span<const std::uint8_t> bytes) {
    const auto digest=semantic::internal::sha256(bytes); std::ostringstream out;
    for (auto v:digest) out<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(v);
    return out.str();
}
std::string objectHex(const foundation::ObjectId& id) {
    std::ostringstream out;
    for (auto v:id.bytes) out<<std::hex<<std::setw(2)<<std::setfill('0')<<unsigned(v);
    return out.str();
}
void save(const std::filesystem::path& path,const json& value) {
    std::ofstream out(path); require(bool(out),"output open"); out<<value.dump(2)<<'\n';
    require(bool(out),"output write");
}
json factsJson(const interaction::CpuPendingEraseFacts& f) {
    json j;
#define FACT(name) j[#name]=f.name
    FACT(admitted); FACT(committed); FACT(failed); FACT(noops); FACT(pending); FACT(reserved);
    FACT(jobs); FACT(geometryReuse); FACT(exactCalls); FACT(opportunities);
    FACT(maxOwnerNs); FACT(maxExactNs); FACT(prepareNs); FACT(importNs); FACT(exactNs);
    FACT(commitNs); FACT(workerWaitNs); FACT(targetPrepareNs); FACT(sceneStampPrepareNs);
    FACT(historyPrepareNs); FACT(publicationPrepareNs); FACT(publishNs); FACT(maximumCommitStageNs);
    FACT(preparedBeforeUp); FACT(suffixAfterUp); FACT(checkpointBytes); FACT(allUnionSteps);
    FACT(newQueryUnions); FACT(historyUnions); FACT(finishedImports); FACT(finishedHits); FACT(error);
#undef FACT
    j["terminals"]=json::array();
    for (const auto& t:f.terminals) j["terminals"].push_back({{"serial",t.serial},{"session",t.session},
        {"jobs",t.jobs},{"operationOrdinal",t.operationOrdinal},{"status",t.status},{"generation",t.generation}});
    j["jobLedger"]=json::array();
    for (const auto& v:f.jobLedger) j["jobLedger"].push_back({{"token",v.token},{"historical",v.historical},
        {"inputBytes",v.inputBytes},{"outputBytes",v.outputBytes},{"issuedNs",v.issuedNs},
        {"returnedNs",v.returnedNs},{"importNs",v.importNs},{"status",v.status}});
    return j;
}
json cursor(const interaction::CanvasEditingRuntime& editor) {
    const auto& s=ink_playground::InkPlaygroundHistoryTestAccess::state(editor);
    const auto* p=s.requestOwner()?s.requestOwner():s.head();
    if (!p) return {{"phase",nullptr}};
    return {{"phase",unsigned(p->phase)},{"serial",p->serial},{"source",p->session},
        {"next",p->next},{"reuseScan",p->reuseScan},{"activeJob",p->activeJob},{"job",p->identity[5]}};
}
}

int main(int argc,char** argv) {
    json jobs=json::array(),opportunities=json::array(),preparedOperations=json::array();
    std::filesystem::path directory;
    try {
        require(argc==3,"fixed40 output-directory"); directory=argv[2];
        std::filesystem::create_directories(directory);
        std::ofstream streamed(directory/"job-events.ndjson"); require(bool(streamed),"stream open");
        const auto setupAt=Clock::now();
        std::ifstream file(argv[1],std::ios::binary);
        std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
        require(hash(bytes)=="977b6c6b6b605dba4cb52d712ceb0772397b862a81e909da8c92e723c2c8f26c","fixture SHA256");
        auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);
        require(decoded.ok()&&decoded.snapshot->objects.size()==40,"original Fixed40 decode");
        const auto& snapshot=*decoded.snapshot;
        runtime::RuntimeCompositionCore core; require(core.openDocument(snapshot.document_id),"open");
        require(core.restoreSnapshotBeforeSurface(snapshot,{snapshot.document_id,snapshot.document_revision,
            snapshot.recovery_frontier,snapshot.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
        auto& editor=core.qualificationRuntime(); ink_playground::InkPlaygroundHistoryTestAccess::profile(editor);
        require(core.bindSurface(1431,1010)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"bind");
        const auto workerFloor=editor.reserveCpuWorkerGrant(1,64U*1024U*1024U);
        require(workerFloor&&core.bindCpuPendingExecutor(1),"bind executor");
        auto& facade=core.controlFacade(); std::uint64_t requestId=0,sequence=0;
        const auto control=[&](runtime::CanvasControlPayload payload) {
            runtime::CanvasControlRequest q{}; q.clientId=72; q.requestId=++requestId;
            q.target=facade.mountedCanvasTargets().front(); q.payload=payload;
            require(facade.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");
        };
        runtime::CanvasControlPayload c{}; c.kind=runtime::CanvasControlPayloadKind::kSelectTool;
        c.tool=runtime::CanvasToolKind::kEraser; control(c); c={};
        c.kind=runtime::CanvasControlPayloadKind::kEraserOptions;
        c.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);
        c.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(32); control(c);
        const auto pointer=[&](input::PointerPhase phase,float x,float y) {
            input::PlatformPointerSample p{}; p.source=71; p.pointer=1; p.sequence=++sequence;
            p.timestampNs=90000000000+sequence*8000000; p.x=x; p.y=y; p.pressure=.5; p.phase=phase;
            require(core.submit({{p}}),"input");
        };
        const auto setupNs=ns(setupAt);
        const auto inputAt=Clock::now();
        pointer(input::PointerPhase::kDown,420,160);
        for (unsigned i=1;i<=7200;++i) pointer(input::PointerPhase::kMove,
            float(420+7*std::sin(i*.17)),float(160+170.*i/7200));
        const auto before=ink_playground::InkPlaygroundHistoryTestAccess::work(editor);
        const auto generation=editor.semanticGeneration().value();
        const auto upAt=Clock::now();
        pointer(input::PointerPhase::kUp,999,999); const auto upNs=ns(upAt);
        require(editor.semanticGeneration().value()==generation&&core.cpuPendingFacts().pending==1&&!core.platformKey(71,1),"Up independence");
        require(ink_playground::InkPlaygroundHistoryTestAccess::work(editor).maskHistory.queryUnionOps==before.maskHistory.queryUnionOps,"new query in Up");
        require(!core.close()&&!editor.canUndo(),"pending close/Undo");
        semantic::Operation forbidden{semantic::OperationId{foundation::ObjectId::fromUint64(880001)},snapshot.document_id,
            1,1,semantic::SetDocumentBackgroundOp{foundation::DocumentBackground{}}};
        require(editor.applyExternalOperation(forbidden,semantic::ApplySource::kRemoteSync).disposition!=semantic::ApplyDisposition::kApplied&&editor.semanticGeneration().value()==generation,"remote overtook");
        // Original native-long B: precisely Down/one Move/Up, not Web's eight Moves.
        pointer(input::PointerPhase::kDown,350,210); pointer(input::PointerPhase::kMove,360,230);
        pointer(input::PointerPhase::kUp,900,900);
        require(core.cpuPendingFacts().pending==2&&editor.semanticGeneration().value()==generation,"two pending intents");
        const auto inputNs=ns(inputAt);
        const auto drainAt=Clock::now();
        std::vector<semantic::EraseMaskGeometry> uniqueGeometry;
        std::uint64_t ownerNs=0,takeNs=0,decodeNs=0,createNs=0,advanceNs=0,exportNs=0,queueNs=0,observationNs=0;
        unsigned turns=0; std::uint64_t operationSerial=0;
        while (core.cpuPendingFacts().pending&&turns++<1024) {
            auto observedAt=Clock::now(); const auto beforeCursor=cursor(editor);
            const auto& state=ink_playground::InkPlaygroundHistoryTestAccess::state(editor);
            if (const auto* p=state.head();p&&p->preparedErase&&p->preparedErase->ready()&&p->serial!=operationSerial) {
                operationSerial=p->serial;
                preparedOperations.push_back({{"serial",p->serial},{"encoded_sha256",hash(p->preparedErase->encoded())},
                    {"encoded_bytes",p->preparedErase->encoded().size()}});
            }
            observationNs+=ns(observedAt);
            auto at=Clock::now(); editor.serviceCpuPendingErases(); const auto serviceNs=ns(at); ownerNs+=serviceNs;
            observedAt=Clock::now(); const auto afterCursor=cursor(editor);
            opportunities.push_back({{"turn",turns},{"before",beforeCursor},{"after",afterCursor},{"service_wall_ns",serviceNs}});
            const auto* pending=state.requestOwner();
            json metadata;
            if (pending) {
                metadata={{"serial",pending->serial},{"source",pending->session},{"historical",pending->historyJob},
                    {"active_prefix",pending->activeJob},{"job_prefix",pending->jobPrefix}};
                if (pending->target) { const auto& t=pending->target->transform;
                    metadata["target"]=objectHex(pending->target->objectId);
                    metadata["transform"]=json::array({t.a,t.b,t.c,t.d,t.tx,t.ty});
                }
            }
            observationNs+=ns(observedAt);
            at=Clock::now(); auto request=core.takeCpuPendingRequest(); const auto took=ns(at); takeNs+=took;
            if (request.empty()) continue;
            const auto jobAt=Clock::now();
            at=Clock::now(); auto value=render::internal::cpuDecodeRequest(request); const auto decodedNs=ns(at); decodeNs+=decodedNs;
            require(bool(value),"owner request decode");
            observedAt=Clock::now();
            std::size_t group=0;
            for (;group<uniqueGeometry.size();++group) if (uniqueGeometry[group]==value->geometry) break;
            if (group==uniqueGeometry.size()) uniqueGeometry.push_back(value->geometry);
            const auto* sweep=std::get_if<semantic::SweptCircleMask>(&value->geometry);
            require(sweep,"original long sweep only");
            const auto count=sweep->segments.size();
            metadata["request_sha256"]=hash(request);
            metadata["geometry_sha256"]=hash(std::span<const std::uint8_t>(request).subspan(128,count*80));
            metadata["geometry_group_by_full_value"]=group;
            observationNs+=ns(observedAt);
            at=Clock::now();
            auto job=std::make_unique<render::internal::CpuExactPathJob>(value->geometry,value->prefixCount,value->prefixPath);
            const auto createdNs=ns(at); createNs+=createdNs;
            std::uint64_t compute=0,maximum=0,steps=0,unions=0;
            while (!job->ready()) {
                const auto old=job->cursor(); at=Clock::now();
                require(job->advance(8,2000),"kernel bounded progress"); const auto elapsed=ns(at);
                compute+=elapsed; maximum=std::max(maximum,elapsed); ++steps; unions+=job->cursor()-old;
            }
            advanceNs+=compute;
            at=Clock::now(); const auto n=job->path().writeToMemory(nullptr);
            std::vector<std::uint8_t> result(128+n); std::copy_n(request.begin(),128,result.begin());
            const auto word=[&](unsigned p,std::uint32_t v) { for(unsigned i=0;i<4;++i) result[p+i]=v>>(i*8); };
            word(0,0x31525043); word(4,1); word(96,n); word(100,2);
            for (unsigned k=104;k<128;++k) result[k]=0;
            job->path().writeToMemory(result.data()+128); const auto exportedNs=ns(at); exportNs+=exportedNs;
            auto wrong=value->identity.words; ++wrong[1];
            require(!core.queueCpuPendingCompletion(wrong,result,true),"foreign basis");
            at=Clock::now(); require(core.queueCpuPendingCompletion(value->identity.words,result,true),"matching completion");
            const auto queuedNs=ns(at); queueNs+=queuedNs;
            require(!core.queueCpuPendingCompletion(value->identity.words,result,true),"duplicate completion");
            const auto jobWall=ns(jobAt);
            observedAt=Clock::now();
            metadata.update({{"index",jobs.size()},{"token",value->identity.words},{"segments",count},
                {"prefix_segments",value->prefixCount},{"union_steps",unions},{"advance_calls",steps},
                {"request_bytes",request.size()},{"output_bytes",result.size()},{"take_ns",took},
                {"decode_ns",decodedNs},{"create_including_native_input_copy_ns",createdNs},
                {"advance_ns",compute},{"max_step_ns",maximum},{"max_union_ns",job->maxUnionNanoseconds()},
                {"export_ns",exportedNs},{"queue_completion_ns",queuedNs},{"job_surrogate_wall_ns",jobWall},
                {"path_points",job->path().countPoints()},{"path_verbs",job->path().countVerbs()},
                {"path_approximate_bytes",job->path().approximateBytesUsed()},{"output_sha256",hash(result)}});
            jobs.push_back(metadata); streamed<<metadata.dump()<<'\n'; streamed.flush();
            observationNs+=ns(observedAt);
        }
        const auto drainNs=ns(drainAt),upThroughDrainNs=ns(upAt);
        const auto facts=core.cpuPendingFacts();
        const auto work=ink_playground::InkPlaygroundHistoryTestAccess::work(editor);
        json summary={{"schema","axiom-native-cpu-tail-attribution-v1"},{"status","MEASURED"},
            {"measurement","synchronous native Runtime + CpuExactPathJob surrogate; no frame present"},
            {"fixture_sha256",hash(bytes)},{"input",{{"A_segments",7200},{"B_moves",1},
                {"A_up",json::array({999,999})},{"B_up",json::array({900,900})},
                {"paced",false},{"native_B_differs_from_historical_Web_B_moves",8}}},
            {"setup_ns",setupNs},{"input_ns",inputNs},{"A_up_ns",upNs},
            {"drain_wall_ns",drainNs},{"A_up_through_drain_wall_ns",upThroughDrainNs},
            {"owner_service_wall_ns",ownerNs},{"take_total_ns",takeNs},{"decode_total_ns",decodeNs},
            {"create_total_ns",createNs},{"advance_total_ns",advanceNs},{"export_total_ns",exportNs},
            {"queue_completion_total_ns",queueNs},{"explicit_observation_total_ns",observationNs},
            {"owner_service_opportunities",turns},{"unique_job_geometry_groups",uniqueGeometry.size()},
            {"facts",factsJson(facts)},{"prepared_operations",preparedOperations},
            {"main_new_query_union_delta",work.maskHistory.queryUnionOps-before.maskHistory.queryUnionOps},
            {"owner_counter_semantics","commitNs includes stage counters; workerWaitNs overlaps synchronous surrogate and is not additive"},
            {"not_measured",{{"JS_transport",nullptr},{"Worker_message_queue",nullptr},{"Worker_yield",nullptr},
                {"RAF",nullptr},{"canonical_render",nullptr},{"GPU_present",nullptr},{"browser_up_to_settled",nullptr}}}};
        require(!facts.pending&&facts.failed==0&&facts.committed==2&&facts.noops==0,"two canonical applies");
        require(summary["main_new_query_union_delta"]==0,"owner rebuilt new query");
        std::uint64_t counted=0; for(const auto& row:jobs) counted+=row.at("union_steps").get<std::uint64_t>();
        require(counted==facts.allUnionSteps,"union accounting mismatch");
        require(jobs.size()==facts.jobs&&facts.jobs==64&&facts.geometryReuse==48,"same native long cardinality");
        require(preparedOperations.size()==2&&facts.terminals.size()==2,"operation/terminal accounting");
        require(editor.releaseCpuWorkerGrant(1,workerFloor)&&editor.cpuWorkerGrantBytes()==0,"lease closed");
        require(core.close(),"final close"); summary["cleanup"]={{"closed",true},{"worker_grant_bytes",0}};
        save(directory/"job-ledger.json",{{"jobs",jobs},{"owner_opportunities",opportunities}});
        save(directory/"attribution.json",summary);
        std::cout<<json{{"status","MEASURED"},{"jobs",facts.jobs},{"union_steps",counted},
            {"owner_ms",ownerNs/1e6},{"kernel_ms",advanceNs/1e6},{"drain_ms",drainNs/1e6}}.dump()<<'\n';
        return 0;
    } catch(const std::exception& e) {
        if(!directory.empty()) {
            save(directory/"failure.json",{{"error",e.what()},{"jobs",jobs},{"owner_opportunities",opportunities}});
        }
        std::cerr<<e.what()<<'\n'; return 1;
    }
}

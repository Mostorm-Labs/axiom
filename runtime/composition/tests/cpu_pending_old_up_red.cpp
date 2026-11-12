// Actual Runtime input/OperationEngine oracle, no Worker stub or fake owner.
#define main cpuPendingPriorFixtureMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using json=nlohmann::json;
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static void profile(interaction::CanvasEditingRuntime& r){render::RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;r.skiaRenderer_.setRenderCacheOptions(o);}
 static auto work(const interaction::CanvasEditingRuntime& r){return r.skiaRenderer_.renderCacheWork();}
};}
int main(int argc,char**argv){try{
 require(argc==3,"fixed40 output");std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
 auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"fixture");const auto& s=*decoded.snapshot;
 runtime::RuntimeCompositionCore core;require(core.openDocument(s.document_id),"open");require(core.restoreSnapshotBeforeSurface(s,{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
 auto& editor=core.qualificationRuntime();ink_playground::InkPlaygroundHistoryTestAccess::profile(editor);
 require(core.bindSurface(1431,1010)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"bind");
 auto& f=core.controlFacade();std::uint64_t req=0,sequence=0;
 auto control=[&](runtime::CanvasControlPayload p){runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++req;q.target=f.mountedCanvasTargets().front();q.payload=p;require(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");};
 runtime::CanvasControlPayload c{};c.kind=runtime::CanvasControlPayloadKind::kSelectTool;c.tool=runtime::CanvasToolKind::kEraser;control(c);c={};c.kind=runtime::CanvasControlPayloadKind::kEraserOptions;c.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);c.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(32);control(c);
 auto pointer=[&](input::PointerPhase phase,float x,float y){input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++sequence;p.timestampNs=90000000000+sequence*8000000;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;require(core.submit({{p}}),"input");};
 pointer(input::PointerPhase::kDown,420,160);for(unsigned i=1;i<=1600;++i)pointer(input::PointerPhase::kMove,float(420+7*std::sin(i*.17)),float(160+170.*i/1600));
 const auto before=ink_playground::InkPlaygroundHistoryTestAccess::work(editor);const auto generation=editor.semanticGeneration().value();const auto ops=editor.submittedOperationCount();const auto start=std::chrono::steady_clock::now();pointer(input::PointerPhase::kUp,999,999);const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();const auto after=ink_playground::InkPlaygroundHistoryTestAccess::work(editor);
 json result{{"status","RED"},{"actual_input",true},{"Up_ms",ms},{"newquery_unions_in_Up",after.maskHistory.queryUnionOps-before.maskHistory.queryUnionOps},{"generation_before",generation},{"generation_after",editor.semanticGeneration().value()},{"operations_before",ops},{"operations_after",editor.submittedOperationCount()},{"active_contacts_checked_by_existing_dispatch",true}};
 std::ofstream(argv[2])<<result.dump(2)<<'\n';std::cout<<result.dump()<<std::endl;
 require(result["newquery_unions_in_Up"]==0&&editor.semanticGeneration().value()==generation,"ACTUAL_UP_COMPUTES_AND_COMMITS_BEFORE_PENDING_EXECUTOR");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

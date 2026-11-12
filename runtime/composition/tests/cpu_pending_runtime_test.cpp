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
 static bool table(interaction::CanvasEditingRuntime& r,const runtime::TableRequest& q){return r.createTableObject(q).applied;}
};}
#include "../../render/src/cpu_worker_protocol.hpp"
#include "../../render/src/cpu_exact_path_kernel.hpp"
int main(int argc,char**argv){try{
 require(argc>=3,"fixed40 output [mode]");const std::string mode=argc>3?argv[3]:"twocommit";const unsigned length=mode=="long"?7200:1600;std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
 auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"fixture");const auto& s=*decoded.snapshot;
 runtime::RuntimeCompositionCore core;require(core.openDocument(s.document_id),"open");require(core.restoreSnapshotBeforeSurface(s,{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
 auto& editor=core.qualificationRuntime();ink_playground::InkPlaygroundHistoryTestAccess::profile(editor);
 require(core.bindSurface(1431,1010)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"bind");
 const auto frame=[&]{runtime::RuntimeFrameOpportunity work;require(core.serviceFrameOpportunity(false,work),"actualframeopportunity");if(work.previewWork)require(core.finishFrameOpportunity({work.previewWork,true}),"previewack");if(work.canonicalWork)require(core.receivePlatformPresented({*work.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"canonicalack");};
 const auto workerFloor=editor.reserveCpuWorkerGrant(1,64U*1024U*1024U);require(workerFloor&&core.bindCpuPendingExecutor(1),"bind actual pendingexecutor");
 auto& f=core.controlFacade();std::uint64_t req=0,sequence=0;
 auto control=[&](runtime::CanvasControlPayload p){runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++req;q.target=f.mountedCanvasTargets().front();q.payload=p;require(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");};
 runtime::CanvasControlPayload c{};c.kind=runtime::CanvasControlPayloadKind::kSelectTool;c.tool=runtime::CanvasToolKind::kEraser;control(c);c={};c.kind=runtime::CanvasControlPayloadKind::kEraserOptions;c.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);c.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(32);control(c);
 auto pointer=[&](input::PointerPhase phase,float x,float y){input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++sequence;p.timestampNs=90000000000+sequence*8000000;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;require(core.submit({{p}}),"input");};
 if(mode=="overflow"){
  pointer(input::PointerPhase::kDown,420,160);for(unsigned i=1;i<8193;++i)pointer(input::PointerPhase::kMove,420,160);
  input::PlatformPointerSample over{};over.source=71;over.pointer=1;over.sequence=++sequence;over.timestampNs=90000000000+sequence*8000000;over.x=420;over.y=160;over.phase=input::PointerPhase::kMove;
  require(!core.submit({{over}}),"oversize silentlyaccepted");require(!core.platformKey(71,1)&&core.cpuPendingFacts().pending==0,"oversizepointerorintentleak");
  pointer(input::PointerPhase::kDown,420,160);pointer(input::PointerPhase::kCancel,420,160);require(editor.semanticGeneration().value()==0,"overflowprefixcommitted");require(editor.releaseCpuWorkerGrant(1,workerFloor)&&editor.cpuWorkerGrantBytes()==0,"overflowlease");require(core.close(),"overflowclose");return 0;
 }
 pointer(input::PointerPhase::kDown,420,160);for(unsigned i=1;i<=length;++i)pointer(input::PointerPhase::kMove,float(420+7*std::sin(i*.17)),float(160+170.*i/length));
 const auto before=ink_playground::InkPlaygroundHistoryTestAccess::work(editor);const auto generation=editor.semanticGeneration().value();const auto start=std::chrono::steady_clock::now();pointer(input::PointerPhase::kUp,999,999);const auto upMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 require(editor.semanticGeneration().value()==generation&&core.cpuPendingFacts().pending==1&&!core.platformKey(71,1),"Up pending/contact independently retired");
 require(ink_playground::InkPlaygroundHistoryTestAccess::work(editor).maskHistory.queryUnionOps==before.maskHistory.queryUnionOps,"new query union inUp");
 require(!core.close()&&!editor.canUndo(),"pending close/Undo must Busy");
 const auto unchanged=editor.semanticGeneration().value();
 semantic::Operation forbidden{semantic::OperationId{foundation::ObjectId::fromUint64(880001)},s.document_id,1,1,semantic::SetDocumentBackgroundOp{foundation::DocumentBackground{}}};
 require(editor.applyExternalOperation(forbidden,semantic::ApplySource::kRemoteSync).disposition!=semantic::ApplyDisposition::kApplied&&editor.semanticGeneration().value()==unchanged,"remote overtookpending");

 if(mode=="radius")require(editor.setEraserOptions(2,64),"change toolafterfrozenA");
 if(mode=="guards"){
  runtime::TableRequest t{};t.rows=1;t.columns=1;t.generation=generation;t.x=10;t.y=10;
  require(!ink_playground::InkPlaygroundHistoryTestAccess::table(editor,t)&&editor.semanticGeneration().value()==generation,"pendingtableovertake");
  require(!editor.setDocumentBackground(foundation::DocumentBackground{})&&editor.semanticGeneration().value()==generation,"pendingbackgroundovertake");
 }
 pointer(input::PointerPhase::kDown,mode=="noop"?430:350,mode=="noop"?240:210);pointer(input::PointerPhase::kMove,mode=="noop"?435:360,mode=="noop"?250:230);pointer(input::PointerPhase::kUp,900,900);
 require(core.cpuPendingFacts().pending==2&&editor.semanticGeneration().value()==generation,"B cannot overtakeA");
 if(mode=="sourcecancel"){
  pointer(input::PointerPhase::kDown,10,10);pointer(input::PointerPhase::kMove,11,11);
  ++sequence;require(core.unregisterSource(71,sequence,90000000000+sequence*8000000),"sourcecancel");
  require(core.cpuPendingFacts().pending==2,"sourcelostdiscardedsealedUp");
 }
 json jobs=json::array();unsigned opportunities=0;bool faulted=false,contextTested=false,rebound=false;
 while(core.cpuPendingFacts().pending&&opportunities++<1024){
  if(mode=="frame"||mode=="edgeframe")frame();else editor.serviceCpuPendingErases();
  if(mode=="fatal"&&faulted&&!rebound&&core.cpuPendingFacts().state!="WaitingWorker"){
   editor.suspendCpuPendingExecutor();const auto issued=core.cpuPendingFacts().jobs;for(unsigned i=0;i<4;++i)editor.serviceCpuPendingErases();require(core.cpuPendingFacts().jobs==issued,"suspendedissuedoldexecutorjob");
   require(core.bindCpuPendingExecutor(2),"rebindnextreadyexecutor");rebound=true;
  }
  auto request=core.takeCpuPendingRequest();if(request.empty())continue;
  auto value=render::internal::cpuDecodeRequest(request);require(bool(value),"ownerencoded exactrequest");
  if(mode=="radius"&&!core.cpuPendingFacts().historyJob){const auto& sweep=std::get<semantic::SweptCircleMask>(value->geometry);require(sweep.segments.front().p0.radius==(value->identity.words[7]==1?16:32),"capturedradiuschanged");}
  if((mode=="failure"||mode=="fatal")&&!faulted){faulted=true;auto wrong=value->identity.words;++wrong[4];require(!core.queueCpuPendingCompletion(wrong,{},false),"foreignfailedbasis accepted");require(core.queueCpuPendingCompletion(value->identity.words,{},false),"failedterminal");jobs.push_back(value->identity.words);continue;}
render::internal::CpuExactPathJob job(value->geometry);while(!job.ready())require(job.advance(8,2000),"kernel bounded progress");
  const auto n=job.path().writeToMemory(nullptr);std::vector<std::uint8_t> result(128+n);std::copy_n(request.begin(),128,result.begin());
  const auto word=[&](unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)result[p+i]=v>>(i*8);};word(0,0x31525043);word(96,n);word(100,2);job.path().writeToMemory(result.data()+128);
  auto wrong=value->identity.words;++wrong[1];require(!core.queueCpuPendingCompletion(wrong,result,true),"foreignbasis accepted");
  if(mode=="context"&&!contextTested){contextTested=true;const auto gen=editor.semanticGeneration();require(core.loseSurface(),"contextlost");require(core.queueCpuPendingCompletion(value->identity.words,result,true),"holdresultwhilelost");editor.serviceCpuPendingErases();require(editor.semanticGeneration()==gen&&core.cpuPendingFacts().pending==2,"lost intent mutatedorvanished");require(core.rebindSurface({1431,1010,1431,1010,1,1}),"rebind");}
  else require(core.queueCpuPendingCompletion(value->identity.words,result,true),"matchingcompletion");require(!core.queueCpuPendingCompletion(value->identity.words,result,true),"duplicatecompletion");jobs.push_back(value->identity.words);
 }
 if(mode=="frame"||mode=="edgeframe")for(unsigned n=0;n<4096&&core.canonicalPresentationDirty();++n)frame();
 const auto facts=core.cpuPendingFacts();json result{{"Up_ms",upMs},{"pending",facts.pending},{"committed",facts.committed},{"failed",facts.failed},{"noops",facts.noops},{"error",facts.error},{"jobs",jobs},{"opportunities",opportunities},{"generation",editor.semanticGeneration().value()},{"geometry_reuses",facts.geometryReuse},{"max_owner_ns",facts.maxOwnerNs},{"max_exact_ns",facts.maxExactNs},{"newquery_unions_main",ink_playground::InkPlaygroundHistoryTestAccess::work(editor).maskHistory.queryUnionOps-before.maskHistory.queryUnionOps}};
 std::ofstream(argv[2])<<result.dump(2)<<'\n';std::cout<<result.dump()<<std::endl;
 require(!facts.pending&&(((mode=="failure"||mode=="fatal")&&facts.failed==1&&facts.committed==1)||(mode!="failure"&&mode!="fatal"&&facts.failed==0&&((mode=="noop"&&facts.committed==1&&facts.noops==1)||(mode!="noop"&&facts.committed==2&&facts.noops==0)))),"ordered pending A/B didnotcommit");require(result["newquery_unions_main"]==0,"import consumer rebuiltnewquery");require(editor.releaseCpuWorkerGrant(1,workerFloor)&&editor.cpuWorkerGrantBytes()==0,"leaseclosed");require(core.close(),"finalclose");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

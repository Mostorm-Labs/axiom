#define main receiptPriorMain
#include "cpu_pending_receipt_test.cpp"
#undef main
#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "../../runtime/interaction/src/cpu_pending_erase_state.hpp"
#include "canvas/foundation/internal/worker_status_metrics.hpp"
#include <fstream>
#include <chrono>
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static void seed(interaction::CanvasEditingRuntime& r,unsigned terminals,unsigned jobs){
  auto& s=*r.cpuPending_;s.terminals.clear();s.jobLedger.clear();
  for(unsigned i=0;i<terminals;++i){interaction::CpuPendingEraseTerminal t;t.serial=100+i;t.status="NoOp";t.session=1000+i;t.generation=0;t.token={1,1,2,0,1,100+i,1000+i,1};s.terminals.push_back(t);}
  for(unsigned i=0;i<jobs;++i){interaction::CpuPendingJobFact j;j.token={1,1,2,0,1,100+i,1000+i,1};j.status="Ready";s.jobLedger.push_back(j);}
 }
 static auto& state(interaction::CanvasEditingRuntime& r){return *r.cpuPending_;}
};}
int main(int argc,char**argv){try{
 require(argc==2,"status output");namespace m=foundation::internal::worker_status_metrics;
 sdk::Session session(std::make_unique<RasterHost>());const auto call=[&](sdk::Json j){const auto raw=session.call(j.dump());auto v=sdk::Json::parse(raw);require(v.value("ok",false)||v.value("code","")=="PENDING_RECEIPT_EXPIRED",raw.c_str());return v;};
 call({{"command","open"},{"documentId",std::string(32,'9')},{"builtinFonts",false}});
 const auto grant=runtime::RuntimePlatformAccess::reserveCpuWorkerGrant(*session.composition(),1,64U*1024U*1024U);require(grant,"status floor");call({{"command","cpuPending"},{"action","bind"},{"executor","1"}});
 auto& core=runtime::RuntimePlatformAccess::core(*session.composition());auto& editor=core.qualificationRuntime();const auto owner=runtime::RuntimePlatformAccess::pendingLocalValue(*session.composition()).owner;
 sdk::Json rows=sdk::Json::array();bool zero=true;std::string statusBaseline;
 for(const auto count:{0U,16U,64U}){
  ink_playground::InkPlaygroundHistoryTestAccess::seed(editor,count,count*2);
  for(const auto* shape:{"state","frame","status","closePreflight","pendingReceipt"}){
   sdk::Json request;if(std::string_view(shape)=="closePreflight")request={{"command","cpuPending"},{"action",shape}};
   else if(std::string_view(shape)=="status")request={{"command","cpuPending"},{"action","status"}};
   else if(std::string_view(shape)=="pendingReceipt")request={{"command",shape},{"runtimeInstance",std::to_string(owner[0])},{"lifetimeGeneration",std::to_string(owner[1])},{"serial",count?"100":"999"}};
   else request={{"command",shape}};
   for(unsigned i=0;i<20;++i)(void)call(request);m::counts={};sdk::Json samples=sdk::Json::array();std::string response;
   for(unsigned i=0;i<100;++i){const auto start=std::chrono::steady_clock::now();response=call(request).dump();samples.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());}
   const auto c=m::counts;zero=zero&&!c.facts&&!c.terminalRows&&!c.jobRows&&!c.jsonTerminalRows&&!c.jsonJobRows;
   if(std::string_view(shape)=="status"){if(statusBaseline.empty())statusBaseline=response;else require(response==statusBaseline,"status depends on retained ledger");}
   rows.push_back({{"terminals",count},{"jobs",count*2},{"shape",shape},{"iterations",100},{"full_facts",c.facts},{"copied_terminal_rows",c.terminalRows},{"copied_job_rows",c.jobRows},{"json_terminal_rows",c.jsonTerminalRows},{"json_job_rows",c.jsonJobRows},{"single_terminal_copies",c.terminalCopies},{"response_bytes",response.size()},{"samples_us",samples}});
  }
  m::counts={};const auto facts=call({{"command","cpuPending"},{"action","facts"}});require(facts.at("terminals").size()==count&&facts.at("jobLedger").size()==2*count,"explicit detail lost rows");
  for(unsigned i=0;i<count;++i){const auto& t=facts.at("terminals")[i];require(t.at("serial")==std::to_string(100+i)&&t.at("session")==std::to_string(1000+i)&&t.at("status")=="NoOp"&&t.at("token")[5]==std::to_string(100+i),"detailed terminal field/order changed");}
  for(unsigned i=0;i<count*2;++i){const auto& j=facts.at("jobLedger")[i];require(j.at("status")=="Ready"&&j.at("token")[5]==std::to_string(100+i)&&j.at("inputBytes")==0,"detailed job field/order changed");}
  if(count){auto changed=facts;changed["terminals"][0]["status"]="foreign";changed["jobLedger"][0]["status"]="foreign";require(call({{"command","cpuPending"},{"action","facts"}})==facts,"detail return aliased owner");}
 }
 auto& state=ink_playground::InkPlaygroundHistoryTestAccess::state(editor);
 // Serial0 reserves admission but is not pending. The live request owner
 // overrides an earlier frozen head; all eight token words stay exact.
 auto active=std::make_unique<interaction::CpuPendingEraseState::Intent>();active->activeSerial=900;
 state.slots[0]=std::move(active);
 auto value=editor.cpuPendingStatus();require(value.reserved==1&&value.pending==0,"active reserved counted pending");
 auto head=std::make_unique<interaction::CpuPendingEraseState::Intent>();head->serial=7;head->phase=interaction::CpuPendingEraseState::Phase::Frozen;state.slots[1]=std::move(head);
 auto request=std::make_unique<interaction::CpuPendingEraseState::Intent>();request->serial=8;request->phase=interaction::CpuPendingEraseState::Phase::Waiting;
 request->identity={UINT64_MAX,UINT64_MAX-1,3,4,5,6,7,UINT64_MAX-2};state.slots[2]=std::move(request);
 value=editor.cpuPendingStatus();require(value.reserved==3&&value.pending==2&&value.head==8&&value.requestReady&&value.token==state.slots[2]->identity,"requestOwner/head/token status drift");
 state.slots[2]->requestTaken=true;require(!editor.cpuPendingStatus().requestReady,"taken request still ready");
 editor.suspendCpuPendingExecutor();require(editor.cpuPendingStatus().suspended,"suspend status stale");editor.failCpuPendingExecutor();value=editor.cpuPendingStatus();require(!value.suspended&&value.error=="CPU_PENDING_EXECUTOR_UNAVAILABLE","unavailable status stale");
 auto terminal=editor.cpuPendingTerminal(100);require(bool(terminal),"singleterminal missing");terminal->status="foreign";require(editor.cpuPendingTerminal(100)->status=="NoOp"&&!editor.cpuPendingTerminal(99999),"singleterminal aliases owner/foreignmatch");
 for(auto& slot:state.slots)slot.reset();state.unavailable=false;state.executorSuspended=false;state.error.clear();
 // All legacy terminal dispositions retain receipt semantics and full
 // runtime/lifetime validation, independent of unrelated retained jobs.
 for(const auto* status:{"Committed","NoOp","Failed","DependencyFailed"}){
  state.terminals[0].status=status;sdk::Json receipt{{"command","pendingReceipt"},{"runtimeInstance",std::to_string(owner[0])},{"lifetimeGeneration",std::to_string(owner[1])},{"serial","100"}};
  auto result=sdk::Json::parse(session.call(receipt.dump()));require(result.at("done").get<bool>()&&result.at("code")==status,"terminal disposition changed");
  require(result.at("ok").get<bool>()==(std::string_view(status)=="Committed"||std::string_view(status)=="NoOp"),"terminal failure admission changed");
  receipt["runtimeInstance"]="999999";result=sdk::Json::parse(session.call(receipt.dump()));require(!result.at("ok").get<bool>()&&result.at("code")=="STALE_PENDING_RECEIPT","foreign receipt runtime accepted");
 }
 // Register eight real deferred facade controls against the same selected
 // immutable target; completion still belongs to CanvasControlService.
 state.terminals.clear();state.jobLedger.clear();
 const auto created=call({{"command","table"},{"action","create"},{"generation",std::to_string(editor.semanticGeneration().value())},{"rows",1},{"columns",1},{"x",40},{"y",40}});
 require(created.contains("pendingToken"),"control target creation ticket");
 for(unsigned i=0;editor.cpuPendingStatus().pending&&i<32;++i){editor.serviceCpuPendingErases();call({{"command","frame"}});}
 std::vector<sdk::Json> controls;
 for(unsigned i=0;i<8;++i){const auto receipt=call({{"command","control"},{"kind","transform"},{"matrix",sdk::Json::array({1,0,0,1,40.+i,40.})}});require(receipt.contains("pendingToken"),"deferred transform ticket");controls.push_back(receipt.at("pendingToken"));}
 require(editor.cpuPendingStatus().pending==8,"eight control slots not reserved");
 auto foreign=controls.front();foreign["command"]="pendingReceipt";foreign["serial"]="999999";
 auto denied=sdk::Json::parse(session.call(foreign.dump()));require(!denied.at("ok").get<bool>()&&denied.at("code")=="FOREIGN_CONTROL_RECEIPT","foreign control serial accepted");
 foreign=controls.front();foreign["command"]="pendingReceipt";foreign["control"]["requestId"]="999999";denied=sdk::Json::parse(session.call(foreign.dump()));require(!denied.at("ok").get<bool>()&&denied.at("code")=="CONTROL_RECEIPT_EXPIRED","expired control accepted");
 foreign=controls.front();foreign["command"]="pendingReceipt";foreign["lifetimeGeneration"]="999999";denied=sdk::Json::parse(session.call(foreign.dump()));require(!denied.at("ok").get<bool>()&&denied.at("code")=="STALE_PENDING_RECEIPT","foreign control lifetime accepted");
 for(auto& slot:state.slots)if(slot){state.terminals.push_back({slot->serial,slot->session,0,0,{},"NoOp",editor.semanticGeneration().value()});const auto executor=slot->grantExecutor,token=slot->traceGrant;slot.reset();require(editor.releaseCpuWorkerGrant(executor,token),"seeded terminal trace grant release");}
 m::counts={};call({{"command","frame"}});require(m::counts.terminalCopies==8&&m::counts.terminalQueries==8&&m::counts.facts==0,"facade copied unrelated terminal ledger");
 for(auto receipt:controls){receipt["command"]="pendingReceipt";require(call(receipt).at("done").get<bool>(),"deferred control not completed by its owner");}
 m::counts={};call({{"command","frame"}});require(m::counts.terminalQueries==0,"empty controls queried terminals");
 runtime::RuntimePlatformAccess::releaseCpuWorkerGrant(*session.composition(),1,grant);call({{"command","close"}});
 sdk::Json result{{"rows",rows},{"zero_detail_hotpath",zero},{"actual_Session_owner",true},{"timing_qualification","Session call plus test requestdump/JSONparse/responsedump; not pure ownerCPU"}};std::ofstream(argv[1])<<result.dump(2)<<'\n';require(zero,"ORDINARY_STATUS_COPIES_RETAINED_HISTORY");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

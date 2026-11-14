// Actual Runtime input/OperationEngine oracle, no Worker stub or fake owner.
#define main cpuPendingPriorFixtureMain
#include "../../render/tests/canonical_prepared_body_test.cpp"
#undef main
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/snapshot.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/scene/incremental_runtime_coordinator.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <new>
namespace {bool cpuFailNextAllocation=false;canvas::interaction::CanvasEditingRuntime* cpuFaultRuntime=nullptr;std::uint64_t cpuFaultGeneration=UINT64_MAX;}
void* operator new(std::size_t n){if(cpuFailNextAllocation){cpuFailNextAllocation=false;if(cpuFaultRuntime)cpuFaultGeneration=cpuFaultRuntime->semanticGeneration().value();throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
#include "host_font_resources.hpp"
using json=nlohmann::json;
#include "../../interaction/src/cpu_pending_erase_state.hpp"
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static void profile(interaction::CanvasEditingRuntime& r){render::RenderCacheOptions o;o.maxBytes=256U*1024U*1024U;o.sharedMaxBytes=512U*1024U*1024U;o.maxEntryBytes=64U*1024U*1024U;r.skiaRenderer_.setRenderCacheOptions(o);}
 static std::size_t displays(const interaction::CanvasEditingRuntime& r){return std::count_if(r.displayRecords_.begin(),r.displayRecords_.end(),[](const auto& d){return d.serial!=0;});}
 static void dependency(interaction::CanvasEditingRuntime& r,std::uint64_t child,std::uint64_t parent){for(auto& p:r.cpuPending_->slots)if(p&&p->serial==child)p->dependencies.push_back(parent);}
 static auto* sink(interaction::CanvasEditingRuntime& r){return r.localOperationSink_;}
 static auto* head(interaction::CanvasEditingRuntime& r){return r.cpuPending_->head();}
 static void step(interaction::CanvasEditingRuntime& r,std::chrono::steady_clock::time_point began=std::chrono::steady_clock::now()){r.serviceCpuPendingEraseStep(began);}
 static bool publishReady(interaction::CanvasEditingRuntime& r){const auto* p=r.cpuPending_->head();return p&&p->phase==interaction::CpuPendingEraseState::Phase::CommitPreparing&&p->publicationPrepared;}
 static auto& compiler(interaction::CanvasEditingRuntime& r){return *r.sceneCompiler_;}
 static auto full(interaction::CanvasEditingRuntime& r){return r.sceneCompiler_->compileFull(semantic::SemanticReadView(r.semanticObjects_,r.semanticGeneration_.current()));}
 static auto current(interaction::CanvasEditingRuntime& r){return r.runtimeSceneHost_->read();}
 static auto work(const interaction::CanvasEditingRuntime& r){return r.skiaRenderer_.renderCacheWork();}
 static bool feedback(const interaction::CanvasEditingRuntime& r,render::ReferenceDrawList& l){return r.applyPendingPresentation(l)&&r.applyPartialErasePreview(l);}
 static bool table(interaction::CanvasEditingRuntime& r,const runtime::TableRequest& q){return r.createTableObject(q).applied;}
};}
#include "../../interaction/src/pending_scene_preparation.hpp"
#include "../../render/src/cpu_worker_protocol.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "../../render/src/cpu_exact_path_kernel.hpp"
class PostPublicationFaultSink final:public interaction::LocalOperationSink {
 public:
 interaction::LocalOperationSink* inner;bool armed=false;
 explicit PostPublicationFaultSink(interaction::LocalOperationSink* sink):inner(sink){}
 bool preparePublication(std::span<const semantic::Operation> ops) noexcept override{return !inner||inner->preparePublication(ops);}
 bool prepareEncodedPublication(const semantic::Operation& op,std::span<const std::uint8_t> bytes)noexcept override{const bool ok=!inner||inner->prepareEncodedPublication(op,bytes);if(ok&&!armed){armed=true;cpuFailNextAllocation=true;}return ok;}
 void abortPreparedPublication()noexcept override{if(inner)inner->abortPreparedPublication();}
 void applied(const semantic::Operation& op,const semantic::CanonicalCommitRecord& record)noexcept override{if(inner)inner->applied(op,record);}
 interaction::AxisOperationStatus status(const semantic::OperationId& op,const semantic::CanonicalCommitStamp& stamp)const noexcept override{return inner?inner->status(op,stamp):interaction::AxisOperationStatus{};}
};
int main(int argc,char**argv){try{
 require(argc>=3,"fixed40 output [mode]");const std::string mode=argc>3?argv[3]:"twocommit";const unsigned length=mode=="long"?7200:((mode.starts_with("progress")||mode=="mixedcommit"||mode=="mixedframe"||mode=="activecancel"||mode=="mixedsticky"||mode=="mixedtable"||mode=="mixedbrush"||mode=="historybarrier"||mode=="mixedimage"||mode=="timeline"||mode=="failedcreate"||mode=="dependencychain"||mode=="failuremixed"||mode=="quota8"||mode=="metadatapressure"||mode=="postpublishfault"||mode=="activebasis")?24:1600);std::ifstream file(argv[1],std::ios::binary);std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(file),{}};
 auto decoded=semantic::SnapshotCodec::decode(bytes,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(decoded.ok(),"fixture");auto s=*decoded.snapshot;
 if(mode.starts_with("progress")){
  const auto first=std::find_if(s.objects.begin(),s.objects.end(),[](const auto& o){return std::holds_alternative<semantic::CompactStrokeContent>(o.content);});require(first!=s.objects.end(),"progress actualstroke");auto base=*first;s.objects.clear();
  for(unsigned n=0;n<3;++n){auto o=base;o.id=foundation::ObjectId::fromUint64(970000+n);o.transform={1,0,0,1,0,0};o.erase_masks.clear();auto& c=std::get<semantic::CompactStrokeContent>(o.content);c.stroke.knots.clear();for(unsigned i=0;i<256;++i)c.stroke.knots.push_back({350.+i*.6,240.+n*3+20*std::sin(i*.05),32768});s.objects.push_back(std::move(o));}
  require(semantic::SnapshotCodec::populateRequirements(s)&&semantic::SnapshotCodec::bindDigests(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1),"progress realfixturebind");
  if(mode=="progressfixture"){
   auto encoded=semantic::SnapshotCodec::encode(s,semantic::SnapshotCapability::kAllWithCompactStrokeV1);require(encoded.ok(),"fixture encode");std::ofstream stream(std::string(argv[2])+".axsnap",std::ios::binary);stream.write(reinterpret_cast<const char*>(encoded.bytes.data()),encoded.bytes.size());std::ofstream(argv[2])<<json{{"documentId",s.document_id.value().bytes},{"metadata",{{"documentRevision",std::to_string(s.document_revision)},{"recoveryFrontier",s.recovery_frontier},{"checkpointDigest",s.checkpoint_digest},{"resourceBinding","1"}}},{"objects",s.objects.size()},{"knots",256},{"fixture_source","Fixed40 bound config,3strokes same nativeB work"}}.dump(2)<<'\n';return 0;
  }

 }

 runtime::RuntimeCompositionCore core;require(core.openDocument(s.document_id),"open");require(core.restoreSnapshotBeforeSurface(s,{s.document_id,s.document_revision,s.recovery_frontier,s.checkpoint_digest},1)==runtime::DocumentRestoreStatus::kRestored,"restore");
 auto& editor=core.qualificationRuntime();ink_playground::InkPlaygroundHistoryTestAccess::profile(editor);
 PostPublicationFaultSink faultSink(ink_playground::InkPlaygroundHistoryTestAccess::sink(editor));if(mode=="postpublishfault"){cpuFaultRuntime=&editor;editor.setLocalOperationSink(&faultSink);}
 require(core.bindSurface(1431,1010)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"bind");
 const auto frame=[&]{runtime::RuntimeFrameOpportunity work;require(core.serviceFrameOpportunity(false,work),"actualframeopportunity");if(work.previewWork)require(core.finishFrameOpportunity({work.previewWork,true}),"previewack");if(work.canonicalWork)require(core.receivePlatformPresented({*work.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"canonicalack");};
 if(mode=="stampabort"||mode=="stampfilled"){
  auto& compiler=ink_playground::InkPlaygroundHistoryTestAccess::compiler(editor);
  const auto found=std::find_if(s.objects.begin(),s.objects.end(),[](const auto& o){return std::holds_alternative<semantic::CompactStrokeContent>(o.content);});require(found!=s.objects.end(),"stamp stroke");
  const auto before=editor.pendingSceneRecord(found->id);require(bool(before),"stamp proof");std::vector<RuntimeSceneRecord> proofs{*before};
  semantic::SweptCircleMask sweep;for(unsigned i=0;i<128;++i)sweep.segments.push_back({{{float(i),3},2},{{float(i+1),4},2},{float(i),3},{float(i+1),4}});
  semantic::FilledPathMask path;path.path.commands={semantic::MoveTo{{1,1}},semantic::LineTo{{20,1}},semantic::QuadTo{{30,4},{20,15}},semantic::CubicTo{{10,20},{3,18},{1,1}},semantic::ClosePath{}};
  semantic::EraseMaskGeometry firstGeometry=mode=="stampfilled"?semantic::EraseMaskGeometry{path}:semantic::EraseMaskGeometry{sweep};
  semantic::Operation first{semantic::OperationId{foundation::ObjectId::fromUint64(930001)},s.document_id,1,1,semantic::AddEraseMasksOp{{{found->id,{{foundation::ObjectId::fromUint64(930002),firstGeometry}}}}}};
  interaction::PendingSceneStampPreparation cursor;auto firstOwner=std::make_shared<const semantic::Operation>(first);
  if(mode=="stampabort")require(!interaction::preparePendingMaskStamps(compiler,proofs,firstOwner,cursor,1,8000),"must suspend stamp");
  else while(!interaction::preparePendingMaskStamps(compiler,proofs,firstOwner,cursor,1,8000)){}
  const auto beforeFull=ink_playground::InkPlaygroundHistoryTestAccess::full(editor);
  auto second=first;second.id=semantic::OperationId{foundation::ObjectId::fromUint64(930003)};auto& mask=std::get<semantic::AddEraseMasksOp>(second.payload).items[0].masks[0];mask.id=foundation::ObjectId::fromUint64(930004);
  if(auto* geometry=std::get_if<semantic::SweptCircleMask>(&mask.geometry)){geometry->segments[0].p0.radius=7;geometry->segments.resize(8192,geometry->segments.back());}else std::get<semantic::LineTo>(std::get<semantic::FilledPathMask>(mask.geometry).path.commands[1]).end.x=50;
  auto secondOwner=std::make_shared<const semantic::Operation>(second);unsigned turns=1;while(!interaction::preparePendingMaskStamps(compiler,proofs,secondOwner,cursor,1,8000))++turns;if(mode=="stampabort")require(turns>=256,"retry reused aborted operation progress");
  const auto applied=editor.applyExternalOperation(second,semantic::ApplySource::kRemoteSync);require(applied.disposition==semantic::ApplyDisposition::kApplied,"stamp second apply");
  const auto expected=ink_playground::InkPlaygroundHistoryTestAccess::full(editor);require(bool(expected),"independent full scene");const auto current=ink_playground::InkPlaygroundHistoryTestAccess::current(editor);
  for(const auto& record:expected.value().records){const auto* actual=current.find(record.objectId);require(actual&&actual->contentRevision==record.contentRevision,"prepared stamp disagrees with independent full scene");}
  if(mode=="stampfilled"){
   auto third=second;third.id=semantic::OperationId{foundation::ObjectId::fromUint64(930005)};auto& value=std::get<semantic::AddEraseMasksOp>(third.payload).items[0].masks[0];std::get<semantic::LineTo>(std::get<semantic::FilledPathMask>(value.geometry).path.commands[1]).end.x=90;
   // Same ID and command count, new path values after Undo/reapply.
   semantic::Operation remove{semantic::OperationId{foundation::ObjectId::fromUint64(930006)},s.document_id,1,1,semantic::RemoveEraseMasksOp{{{found->id,{value.id}}}}};require(editor.applyExternalOperation(remove,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"remove");
   require(editor.applyExternalOperation(third,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"third");const auto other=ink_playground::InkPlaygroundHistoryTestAccess::full(editor);require(bool(other),"third full");
   auto expectedRecord=std::find_if(expected.value().records.begin(),expected.value().records.end(),[&](const auto& r){return r.objectId==found->id;});auto otherRecord=std::find_if(other.value().records.begin(),other.value().records.end(),[&](const auto& r){return r.objectId==found->id;});require(expectedRecord!=expected.value().records.end()&&otherRecord!=other.value().records.end()&&expectedRecord->contentRevision!=otherRecord->contentRevision,"FilledPath scene identity ignored command values");
  }return 0;
 }
 if(mode=="focus"||mode=="focuschanged"){
  require(minimal_runtime_host::bindHostTextFonts(core,std::string(AXIOM_SOURCE_ROOT)+"/assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf",std::string(AXIOM_SOURCE_ROOT)+"/assets/fonts/imported"),"focus fonts");
 }
 const auto workerFloor=editor.reserveCpuWorkerGrant(1,64U*1024U*1024U);require(workerFloor&&core.bindCpuPendingExecutor(1),"bind actual pendingexecutor");
 if(mode=="hierarchy"){
  semantic::ObjectRecord parent;parent.id=foundation::ObjectId::fromUint64(940001);parent.kind=semantic::ObjectKind::kGroup;parent.kind_version=1;parent.placement.order_key=semantic::OrderKey{{240}};parent.transform.tx=100;parent.content=semantic::GroupContent{};
  semantic::ObjectRecord child;child.id=foundation::ObjectId::fromUint64(940002);child.kind=semantic::ObjectKind::kShape;child.kind_version=1;child.placement.parent_id=parent.id;child.placement.order_key=semantic::OrderKey{{10}};child.transform.tx=30;child.content=semantic::ShapeContent{1,120,80};
  semantic::ObjectRecord top=child;top.id=foundation::ObjectId::fromUint64(940003);top.placement.parent_id.reset();top.placement.order_key=semantic::OrderKey{{250}};
  semantic::Operation add{semantic::OperationId{foundation::ObjectId::fromUint64(940004)},s.document_id,1,1,semantic::InsertObjectsOp{{parent,child,top}}};require(editor.applyExternalOperation(add,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"hierarchy fixture");
  require(editor.enqueuePendingTransform({{{parent.id,{1,0,0,1,170,0}}}}),"parent drag admit");const auto projected=editor.pendingSceneRecord(child.id);require(projected&&projected->transform.tx==200,"pending parent did not move canonical child");
  render::ReferenceDrawList list;for(const auto id:{parent.id,child.id,top.id}){auto record=editor.pendingSceneRecord(id);auto command=render::DirectReferenceSource::presentationCommand(*record);require(bool(command),"hierarchy command");render::ReferenceTraversalEntry e;e.record=*record;e.command=std::move(command.value());list.entries.push_back(std::move(e));}require(ink_playground::InkPlaygroundHistoryTestAccess::feedback(editor,list),"hierarchy feedback");require(list.entries[0].record.objectId==parent.id&&list.entries[1].record.objectId==child.id&&list.entries[2].record.objectId==top.id,"pending suffix flattened hierarchy zorder");return 0;
 }
 auto& f=core.controlFacade();std::uint64_t req=0,sequence=0;
 if(mode=="focus"||mode=="focuschanged"){
  semantic::ObjectRecord object;object.id=foundation::ObjectId::fromUint64(950001);object.kind=semantic::ObjectKind::kSticky;object.kind_version=1;object.content=semantic::StickyContent{200,200};require(editor.enqueuePendingCreate({object},true),"focus create");
  if(mode=="focuschanged")require(editor.setSelectionMode(false)&&editor.selectTool(interaction::CanvasEditingRuntime::ToolMode::kBrush),"focus changed tool");
  editor.serviceCpuPendingErases();require(core.cpuPendingFacts().pending==0,"focus commit");const auto text=f.readTextEditor();require(text.phase==(mode=="focus"?runtime::TextPhase::kEditing:runtime::TextPhase::kClosed),"late pending creation focus stolen or missing");return 0;
 }

 auto control=[&](runtime::CanvasControlPayload p){runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++req;q.target=f.mountedCanvasTargets().front();q.payload=p;require(f.submitCanvasControl(q).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"control");};
 runtime::CanvasControlPayload c{};c.kind=runtime::CanvasControlPayloadKind::kSelectTool;c.tool=runtime::CanvasToolKind::kEraser;control(c);c={};c.kind=runtime::CanvasControlPayloadKind::kEraserOptions;c.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);c.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(32);control(c);
 auto pointer=[&](input::PointerPhase phase,float x,float y){input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++sequence;p.timestampNs=90000000000+sequence*8000000;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;require(core.submit({{p}}),"input");};
 if(mode=="overflow"){
  pointer(input::PointerPhase::kDown,420,160);for(unsigned i=1;i<8193;++i)pointer(input::PointerPhase::kMove,420,160);
  input::PlatformPointerSample over{};over.source=71;over.pointer=1;over.sequence=++sequence;over.timestampNs=90000000000+sequence*8000000;over.x=420;over.y=160;over.phase=input::PointerPhase::kMove;
  require(!core.submit({{over}}),"oversize silentlyaccepted");require(!core.platformKey(71,1)&&core.cpuPendingFacts().pending==0,"oversizepointerorintentleak");
  pointer(input::PointerPhase::kDown,420,160);pointer(input::PointerPhase::kCancel,420,160);require(editor.semanticGeneration().value()==0,"overflowprefixcommitted");require(editor.releaseCpuWorkerGrant(1,workerFloor)&&editor.cpuWorkerGrantBytes()==0,"overflowlease");require(core.close(),"overflowclose");return 0;
 }
 if(mode.starts_with("camera")){for(unsigned i=0;i<512;++i){frame();if(!core.canonicalPresentationDirty())break;require(i<511,"camera initialcanonical");}}
 pointer(input::PointerPhase::kDown,420,160);for(unsigned i=1;i<=length;++i)pointer(input::PointerPhase::kMove,float(420+7*std::sin(i*.17)),float(160+170.*i/length));
 if(mode=="activepaced"){
  // Active input no longer eagerly unions sixteen whole masks. Cancellation
  // must release the admitted trace and preserve the canonical generation.
  const auto activeJobs=core.cpuPendingFacts().jobs;
  for(unsigned round=0;round<18;++round){editor.serviceCpuPendingErases();require(core.takeCpuPendingRequest().empty(),"active input eagerly prepared full union");for(unsigned i=0;i<8;++i)pointer(input::PointerPhase::kMove,420+float(i),331+float(i));}
  pointer(input::PointerPhase::kCancel,0,0);editor.serviceCpuPendingErases();
  require(core.cpuPendingFacts().reserved==0&&core.cpuPendingFacts().jobs==activeJobs,"paced cancellation retained trace or eager job");return 0;
 }
 if(mode=="activecancel"){
  editor.serviceCpuPendingErases();require(core.takeCpuPendingRequest().empty(),"active eager full job");pointer(input::PointerPhase::kCancel,0,0);editor.serviceCpuPendingErases();require(core.cpuPendingFacts().reserved==0&&editor.semanticGeneration().value()==0,"cancel slot retained or canonical changed");require(editor.releaseCpuWorkerGrant(1,workerFloor)&&editor.cpuWorkerGrantBytes()==0,"cancel lease leaked");require(core.close(),"cancel close");return 0;
 }
 if(mode=="activeprefix"){
  for(unsigned n=0;n<32;++n){editor.serviceCpuPendingErases();auto request=core.takeCpuPendingRequest();if(!request.empty()){std::cout<<"actual_active_request_bytes="<<request.size()<<std::endl;return 0;}}
  require(false,"active confirmed Move never produced a Worker request before Up");
 }
 const auto before=ink_playground::InkPlaygroundHistoryTestAccess::work(editor);const auto generation=editor.semanticGeneration().value();const auto start=std::chrono::steady_clock::now();pointer(input::PointerPhase::kUp,999,999);const auto upMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 require(editor.semanticGeneration().value()==generation&&core.cpuPendingFacts().pending==1&&!core.platformKey(71,1),"Up pending/contact independently retired");
 require(ink_playground::InkPlaygroundHistoryTestAccess::work(editor).maskHistory.queryUnionOps==before.maskHistory.queryUnionOps,"new query union inUp");
 require(!core.close()&&!editor.canUndo(),"pending close/Undo must Busy");
 if(mode.starts_with("progress")){
  using P=interaction::CpuPendingEraseState::Phase;auto head=[&]{return ink_playground::InkPlaygroundHistoryTestAccess::head(editor);};
  unsigned stages=0,requests=0;bool reached=false;
  for(;stages<4096;++stages){
   auto* h=head();if(h&&h->phase==P::CommitPreparing){reached=true;break;}
   ink_playground::InkPlaygroundHistoryTestAccess::step(editor);
   auto request=core.takeCpuPendingRequest();if(request.empty())continue;
   if(mode=="progresswait"){
    const auto before=core.cpuPendingFacts();for(unsigned i=0;i<4;++i){const auto old=head()->preparationProgress();const auto count=core.cpuPendingFacts().opportunities;editor.serviceCpuPendingErases();require(core.cpuPendingFacts().opportunities==count+1&&head()->preparationProgress()==old&&head()->phase==P::Waiting&&!head()->completion,"Worker wait busyloop or fake progress");}
    require(core.cpuPendingFacts().jobs==before.jobs&&editor.semanticGeneration().value()==generation,"Worker wait changed jobs/canonical");std::ofstream(argv[2])<<json{{"mode",mode},{"actual_request",request.size()},{"turns",4},{"steps_per_turn",1},{"canonical_unchanged",true}}.dump(2)<<'\n';return 0;
   }
   if(mode=="progressunavailable"){
    require(core.loseSurface(),"progress surface loss");const auto old=head()->preparationProgress();const auto count=core.cpuPendingFacts().opportunities;editor.serviceCpuPendingErases();require(head()->preparationProgress()==old&&core.cpuPendingFacts().opportunities==count&&editor.semanticGeneration().value()==generation,"unavailable owner spinned/published");std::ofstream(argv[2])<<json{{"mode",mode},{"no_preparation_steps",true},{"canonical_unchanged",true}}.dump(2)<<'\n';return 0;
   }
   ++requests;const auto decoded=render::internal::cpuDecodeRequest(request);require(bool(decoded),"progress actualrequest");render::internal::CpuExactPathJob kernel(decoded->geometry,decoded->prefixCount,decoded->prefixPath);while(!kernel.ready())require(kernel.advance(8,2000),"progress actualkernel");const auto size=kernel.path().writeToMemory(nullptr);std::vector<std::uint8_t> result(128+size);std::copy_n(request.begin(),128,result.begin());const auto word=[&](unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)result[p+i]=v>>(i*8);};word(0,0x31525043);word(4,1);word(96,size);word(100,decoded->piece?3:2);if(!decoded->piece)for(unsigned k=104;k<128;++k)result[k]=0;kernel.path().writeToMemory(result.data()+128);require(core.queueCpuPendingCompletion(decoded->identity.words,result,true),"progress completion");
  }
  std::cerr<<"progressreached="<<reached<<" requests="<<requests<<" proofs="<<(head()?head()->proofs.size():0)<<" phase="<<(head()?int(head()->phase):-1)<<" stages="<<stages<<" error="<<core.cpuPendingFacts().error<<std::endl;
  require(reached&&requests>0&&head()->proofs.size()==3,"progress actual3targetcommitprecondition");
  if(mode=="progressstamp"||mode=="progresshistory"||mode=="progresspublication")while(!head()->preparedErase->ready())ink_playground::InkPlaygroundHistoryTestAccess::step(editor);
  if(mode=="progressstamp"){
   // A real partially consumed stamp cursor must continue without a new frame.
   while(head()->sceneStamp.target==0)ink_playground::InkPlaygroundHistoryTestAccess::step(editor);
   require(head()->sceneStamp.target<3,"stamp precondition");
  }
  if(mode=="progresshistory"||mode=="progresspublication")while(head()->sceneStamp.target<3)ink_playground::InkPlaygroundHistoryTestAccess::step(editor);
  if(mode=="progresspublication")ink_playground::InkPlaygroundHistoryTestAccess::step(editor);
  const auto beforeTargets=head()->sceneStamp.target;const auto beforeReady=head()->preparedErase->ready();const auto committed=core.cpuPendingFacts().committed;
  if(mode=="progressbudget"){
   const auto prior=head()->preparationProgress();ink_playground::InkPlaygroundHistoryTestAccess::step(editor,std::chrono::steady_clock::now()-std::chrono::microseconds(8000));require(head()->preparationProgress()==prior&&!head()->preparedErase->ready()&&editor.semanticGeneration().value()==generation,"expired turn published");
  }else{
   const auto at=std::chrono::steady_clock::now();editor.serviceCpuPendingErases();const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-at).count();
   auto* after=head();const bool complete=core.cpuPendingFacts().committed==committed+1;
   const bool useful=complete||(after&&(after->preparedErase->ready()!=beforeReady||after->sceneStamp.target>beforeTargets+1));
   json r{{"mode",mode},{"actual_requests",requests},{"prior_stages",stages},{"owner_ns",ns},{"ready_before",beforeReady},{"stamp_before",beforeTargets},{"complete",complete},{"useful_multiple_internal_stages",useful},{"pending",core.cpuPendingFacts().pending}};std::ofstream(argv[2])<<r.dump(2)<<'\n';std::cout<<r.dump()<<std::endl;
   require(useful,"INTERNAL_PREPARATION_PROGRESS_FORCED_TO_NEXT_OWNER_TURN");
  }
  return 0;
 }
 if(mode.starts_with("camera")){
  // Hold the actual encoded kernel job, never synthesize a pending flag. The
  // existing Composition must keep rendering transient feedback while waiting.
  std::vector<std::uint8_t> held;
  for(unsigned i=0;i<512;++i){frame();if(held.empty())held=core.takeCpuPendingRequest();if(!held.empty()&&!editor.transientOverlayDirty())break;require(i<511,"camera waitingclean");}
  require(!held.empty()&&core.cpuPendingFacts().state=="WaitingWorker"&&core.cpuPendingFacts().pending==1,"camera actual Worker wait");
  const auto read=[&]{std::vector<std::uint8_t> result(1431U*1010U*4U);require(editor.activeSurfaceProvider()->readbackRgba(result).code==render::BackendSubmissionCode::kAccepted,"camera readback");return result;};
  const auto old=read();const auto oldCamera=editor.viewportGesture();const auto oldCameraGeneration=editor.cameraGeneration();
  const auto previewRevision=editor.previewDisplayRevision();
  require(editor.setViewportZoomAt(oldCamera.scale,400,300)&&editor.cameraGeneration()==oldCameraGeneration&&!editor.transientOverlayDirty()&&editor.previewDisplayRevision()==previewRevision,"camera no-op invalidateddisplay");
  require(!editor.setViewportZoomAt(NAN,400,300)&&!editor.fitViewportToContent(foundation::WorldRect{0,0,NAN,5})&&editor.cameraGeneration()==oldCameraGeneration&&!editor.transientOverlayDirty(),"invalid camera damaged state");
  if(mode=="camerapantool"){
   require(editor.setPanTool(true),"camera pantool select");pointer(input::PointerPhase::kDown,700,500);const auto rev=editor.previewDisplayRevision();pointer(input::PointerPhase::kMove,770,530);require(editor.previewDisplayRevision()==rev+1,"one paninput invalidated samecamera twice");pointer(input::PointerPhase::kUp,770,530);
  }else if(mode=="camerafit")require(editor.fitViewportToContent(foundation::WorldRect{0,0,2800,1600}),"camera fit");
  else if(mode=="camerapan")require(editor.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,70,-30}),"camera pan");
  else if(mode=="cameratouch"||mode=="cameraselectiontouch"){
   if(mode=="cameraselectiontouch")require(editor.setSelectionMode(true),"camera selectwhilepending");
   const auto touch=[&](unsigned id,input::PointerPhase phase,float x,float y){input::PlatformPointerSample p{};p.source=71;p.pointer=id;p.sequence=++sequence;p.timestampNs=90000000000+sequence*8000000;p.x=x;p.y=y;p.pressure=.5;p.phase=phase;p.capabilities.tool=input::PointerTool::kFinger;require(core.submit({{p}}),"camera touchinput");};
   touch(2,input::PointerPhase::kDown,500,400);touch(3,input::PointerPhase::kDown,700,400);
   for(unsigned i=0;i<64&&editor.transientOverlayDirty();++i)frame();
   touch(3,input::PointerPhase::kMove,850,450);
  }else require(editor.setViewportZoomAt(mode=="camerazoomin"?1.5F:.7F,700,500),"camera zoom");
  require(editor.cameraGeneration()!=oldCameraGeneration,"camera unchanged fixture");
  const bool dirtyAtChange=editor.transientOverlayDirty();
  for(unsigned i=0;i<256;++i){frame();if(!editor.transientOverlayDirty())break;require(i<255,"camera transientconverge");}
  const auto actual=read();
  // Existing pan admission increments its generation even for zero delta.
  // This is an independent redraw control at the exact same transform.
  require(editor.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,0,0}),"camera panoracle");
  for(unsigned i=0;i<256;++i){frame();if(!editor.transientOverlayDirty())break;require(i<255,"camera oracleconverge");}
  const auto expected=read();std::size_t changed=0,mismatch=0;for(std::size_t i=0;i<actual.size();++i){changed+=expected[i]!=old[i];mismatch+=actual[i]!=expected[i];}
  json result{{"mode",mode},{"pending",core.cpuPendingFacts().pending},{"state",core.cpuPendingFacts().state},{"dirty_at_camera",dirtyAtChange},{"camera",editor.cameraGeneration()},{"canonical_generation",editor.semanticGeneration().value()},{"changed_channels",changed},{"mismatched_channels",mismatch},{"held_actual_request_bytes",held.size()}};std::ofstream(argv[2])<<result.dump(2)<<'\n';std::cout<<result.dump()<<std::endl;
  require(editor.semanticGeneration().value()==generation&&core.cpuPendingFacts().pending==1,"camera prematurelycommitted");require(changed>0,"camera oracle unchanged");require(actual==expected,"PENDING_CAMERA_FRAME_STALE_UNTIL_ZERO_PAN");require(dirtyAtChange,"camera forgot transientinvalidation");return 0;
 }
 const auto unchanged=editor.semanticGeneration().value();
 semantic::Operation forbidden{semantic::OperationId{foundation::ObjectId::fromUint64(880001)},s.document_id,1,1,semantic::SetDocumentBackgroundOp{foundation::DocumentBackground{}}};
 require(editor.applyExternalOperation(forbidden,semantic::ApplySource::kRemoteSync).disposition!=semantic::ApplyDisposition::kApplied&&editor.semanticGeneration().value()==unchanged,"remote overtookpending");

 foundation::ObjectId mixedId{};
 if(mode=="mixedred"||mode=="mixedcommit"||mode=="mixedframe"||mode=="mixedsticky"||mode=="mixedtable"||mode=="historybarrier"||mode=="failedcreate"||mode=="dependencychain"||mode=="failuremixed"||mode=="activebasis"){
  require(editor.selectCreationPreset(mode=="mixedsticky"?interaction::CreationPreset::kSticky:mode=="mixedtable"?interaction::CreationPreset::kTable:interaction::CreationPreset::kRectangle),"select creation tool");
  pointer(input::PointerPhase::kDown,800,500);pointer(input::PointerPhase::kUp,800,500);
  require(editor.semanticGeneration().value()==generation,"create overtook pending erase");
  require(editor.selectionSummary().count==0,"pending create falsely canonical selected");
  const auto pending=editor.pendingLocalValue();std::cerr<<"intents="<<pending.intents.size()<<" selected="<<pending.selected.size()<<" mode="<<editor.selectionMode()<<std::endl;require(pending.intents.size()==2&&pending.selected.size()==1,"pending HUD identity missing");const auto id=pending.selected.front();
  pointer(input::PointerPhase::kDown,1300,900);pointer(input::PointerPhase::kUp,1300,900);require(editor.selectedObjectCount()==0,"empty click retained pending selection");
  pointer(input::PointerPhase::kDown,850,540);pointer(input::PointerPhase::kUp,850,540);require(editor.pendingLocalValue().selected.size()==1&&editor.pendingLocalValue().selected.front()==id,"pending created object cannot reselect");
  pointer(input::PointerPhase::kDown,850,540);pointer(input::PointerPhase::kMove,920,580);
  const auto moving=editor.pendingLocalValue();require(moving.bounds&&(*moving.bounds)[0]>850&&moving.intents.back().token[2]!=0,"active pending bounds/token stale");
  if(mode!="activebasis")pointer(input::PointerPhase::kUp,920,580);
  require(editor.semanticGeneration().value()==generation&&editor.pendingLocalValue().intents.size()==3,"pending created drag unavailable");
  std::cout<<"pending creation and drag accepted without canonical apply"<<std::endl;if(mode=="mixedred")return 0;mixedId=id;
 }
 if(mode=="rejectcleanup"){
  const auto beforeSlots=core.cpuPendingFacts().reserved;semantic::ObjectRecord invalid;invalid.id=foundation::ObjectId::fromUint64(999001);invalid.kind=semantic::ObjectKind::kShape;invalid.kind_version=1;invalid.content=semantic::ShapeContent{1,NAN,80};
  require(!editor.enqueuePendingCreate({invalid})&&core.cpuPendingFacts().reserved==beforeSlots,"rejected creation leaked admission slot");return 0;
 }
 if(mode=="quota8"){
  for(unsigned i=0;i<7;++i){semantic::ObjectRecord shape;shape.id=foundation::ObjectId::fromUint64(910000+i);shape.kind=semantic::ObjectKind::kShape;shape.kind_version=1;shape.content=semantic::ShapeContent{1,120,80};shape.placement.order_key=semantic::OrderKey{{128}};require(editor.enqueuePendingCreate({shape}),"within8slots rejected");}
  const auto selected=editor.selectedPrimaryObject();semantic::ObjectRecord shape;shape.id=foundation::ObjectId::fromUint64(920000);shape.kind=semantic::ObjectKind::kShape;shape.kind_version=1;shape.content=semantic::ShapeContent{1,120,80};shape.placement.order_key=semantic::OrderKey{{128}};require(!editor.enqueuePendingCreate({shape})&&editor.selectedPrimaryObject()==selected&&core.cpuPendingFacts().reserved==8,"overflow lost accepted/selection");return 0;
 }
 if(mode=="timeline"){
  const auto stroke=std::find_if(s.objects.begin(),s.objects.end(),[](const auto& object){return std::holds_alternative<semantic::CompactStrokeContent>(object.content);});require(stroke!=s.objects.end(),"stroke");mixedId=stroke->id;
  require(editor.setSelectionMode(true),"timeline select tool");const auto prior=editor.pendingSceneRecord(mixedId);require(bool(prior),"timeline initial pose");auto next=prior->transform;next.tx+=60;next.ty+=30;require(editor.enqueuePendingTransform({{{mixedId,next}}}),"timeline B");
  require(editor.setSelectionMode(false)&&editor.selectTool(interaction::CanvasEditingRuntime::ToolMode::kPartialEraser),"timeline eraser");pointer(input::PointerPhase::kDown,480,210);pointer(input::PointerPhase::kMove,480,260);pointer(input::PointerPhase::kUp,999,999);
  render::ReferenceDrawList list;render::ReferenceTraversalEntry entry;entry.record=*prior;entry.command=render::CompactStrokeReferenceCommand{scene::ImmutableValue<semantic::CompactStrokeContent>::alias(entry.record.content)};list.entries.push_back(entry);require(ink_playground::InkPlaygroundHistoryTestAccess::feedback(editor,list),"timeline actual feedback");const auto& shown=list.entries.front();std::cerr<<"timeline masks="<<shown.presentationEraseMasks.size()<<" pose="<<shown.record.transform.tx<<","<<shown.record.transform.ty<<" expected="<<next.tx<<","<<next.ty<<" pending="<<core.cpuPendingFacts().pending<<" count="<<list.entries.size()<<std::endl;require(shown.record.transform==next&&shown.presentationEraseMasks.size()==2,"timeline pose/mask count");std::vector<foundation::WorldPoint> trace{{420,160}};for(unsigned i=1;i<=length;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/length)});const auto a=interaction::partialEraseGeometry(trace,prior->transform,16);require(a&&shown.presentationEraseMasks[0].geometry==*a,"A mask became pinned world instead of local");return 0;
 }
 if(mode=="mixedimage"){
  const auto file=std::string(AXIOM_SOURCE_ROOT)+"/.aegis/results/G5-object-interaction-r2-image-move-diagnostic/image-2_3mb.png";std::ifstream imageFile(file,std::ios::binary);runtime::ImageImportRequest image;image.bytes={std::istreambuf_iterator<char>(imageFile),{}};image.mimeType="image/png";image.width=120;image.height=80;image.transform.tx=800;image.transform.ty=500;
  const auto receipt=f.importImage(image);require(receipt.state==runtime::ImageCommandState::kPending,"image queue failed");mixedId=receipt.objectId;require(editor.semanticGeneration().value()==generation&&!f.readSelectedImage().available,"image falsely canonical");
  frame();std::vector<std::uint8_t> beforeImage(1431U*1010U*4U);require(editor.activeSurfaceProvider()->readbackRgba(beforeImage).code==render::BackendSubmissionCode::kAccepted,"pending image readback");
  bool visibleImage=false;for(unsigned yy=510;yy<570;++yy)for(unsigned xx=810;xx<910;++xx){const auto at=(yy*1431+xx)*4;if(beforeImage[at]<240||beforeImage[at+1]<240||beforeImage[at+2]<240)visibleImage=true;}require(visibleImage,"pending imported image did not produce pixels");
  pointer(input::PointerPhase::kDown,850,540);pointer(input::PointerPhase::kMove,920,580);pointer(input::PointerPhase::kUp,920,580);require(editor.pendingLocalValue().intents.size()==3,"pendingimage drag");
 }
 if(mode=="failedcreate"||mode=="dependencychain"){
  auto invalid=*editor.pendingObject(mixedId);invalid.id=foundation::ObjectId::fromUint64(900001);std::get<semantic::ShapeContent>(invalid.content).width=-1;
  require(editor.enqueuePendingCreate({invalid}),"failedcreate admission");require(editor.enqueuePendingTransform({{{invalid.id,{1,0,0,1,70,70}}}}),"dependent transform admission");
  if(mode=="dependencychain"){const auto dependent=editor.lastPendingLocalSerial();auto child=invalid;child.id=foundation::ObjectId::fromUint64(900003);std::get<semantic::ShapeContent>(child.content).width=120;require(editor.enqueuePendingCreate({child}),"second level dependency admission");ink_playground::InkPlaygroundHistoryTestAccess::dependency(editor,editor.lastPendingLocalSerial(),dependent);}
  auto unrelated=invalid;unrelated.id=foundation::ObjectId::fromUint64(900002);std::get<semantic::ShapeContent>(unrelated.content).width=120;require(editor.enqueuePendingCreate({unrelated}),"independent suffix admission");
 }
 if(mode=="mixedbrush"){
  c={};c.kind=runtime::CanvasControlPayloadKind::kSelectTool;c.tool=runtime::CanvasToolKind::kInk;control(c);pointer(input::PointerPhase::kDown,800,500);pointer(input::PointerPhase::kMove,900,550);pointer(input::PointerPhase::kUp,950,580);require(core.cpuPendingFacts().pending==2&&editor.semanticGeneration().value()==generation,"Brush overtook A");
 }
 if(mode=="historybarrier"){
  c={};c.kind=runtime::CanvasControlPayloadKind::kUndo;control(c);require(editor.pendingHistoryBarrier(),"History barrier missing");
  c={};c.kind=runtime::CanvasControlPayloadKind::kPanBy;c.x=2;c.y=3;control(c);
 }
 if(mode=="radius")require(editor.setEraserOptions(2,64),"change toolafterfrozenA");
 if(mode=="guards"){
  runtime::TableRequest t{};t.rows=1;t.columns=1;t.generation=generation;t.x=10;t.y=10;
  require(!ink_playground::InkPlaygroundHistoryTestAccess::table(editor,t)&&editor.semanticGeneration().value()==generation,"pendingtableovertake");
  require(!editor.setDocumentBackground(foundation::DocumentBackground{})&&editor.semanticGeneration().value()==generation,"pendingbackgroundovertake");
 }
 if(mode!="mixedcommit"&&mode!="mixedframe"&&mode!="mixedsticky"&&mode!="mixedtable"&&mode!="mixedbrush"&&mode!="historybarrier"&&mode!="mixedimage"&&mode!="failedcreate"&&mode!="dependencychain"&&mode!="failuremixed"&&mode!="activebasis"){pointer(input::PointerPhase::kDown,mode=="noop"?430:350,mode=="noop"?240:210);pointer(input::PointerPhase::kMove,mode=="noop"?435:360,mode=="noop"?250:230);pointer(input::PointerPhase::kUp,900,900);
 require(core.cpuPendingFacts().pending==2&&editor.semanticGeneration().value()==generation,"B cannot overtakeA");}
 if(mode=="sourcecancel"){
  pointer(input::PointerPhase::kDown,10,10);pointer(input::PointerPhase::kMove,11,11);
  ++sequence;require(core.unregisterSource(71,sequence,90000000000+sequence*8000000),"sourcecancel");
  require(core.cpuPendingFacts().pending==2,"sourcelostdiscardedsealedUp");
 }
 std::vector<std::uint64_t> pressure;
 if(mode=="metadatapressure"){const auto desired=511U*1024U*1024U;while(editor.cpuWorkerGrantBytes()<desired){const auto bytes=std::min<std::size_t>(80U*1024U*1024U,desired-editor.cpuWorkerGrantBytes());const auto grant=editor.reserveCpuWorkerGrant(9,bytes);require(grant,"native deterministic quota pressure");pressure.push_back(grant);}}
 json jobs=json::array();unsigned opportunities=0;bool postFault=false;bool faulted=false,contextTested=false,rebound=false;
 while(core.cpuPendingFacts().pending&&opportunities++<1024){
  if(mode=="postpublishfault")postFault=faultSink.armed;
  if(mode=="frame"||mode=="edgeframe"||mode=="mixedframe"||mode=="mixedimage")frame();else editor.serviceCpuPendingErases();
  if(mode=="fatal"&&faulted&&!rebound&&core.cpuPendingFacts().state!="WaitingWorker"){
   editor.suspendCpuPendingExecutor();const auto issued=core.cpuPendingFacts().jobs;for(unsigned i=0;i<4;++i)editor.serviceCpuPendingErases();require(core.cpuPendingFacts().jobs==issued,"suspendedissuedoldexecutorjob");
   require(core.bindCpuPendingExecutor(2),"rebindnextreadyexecutor");rebound=true;
  }
  if(mode=="metadatapressure"&&opportunities==1){require(core.cpuPendingFacts().failed==0,"accepted short erase lost to worst-case4096 metadata reservation");for(auto grant:pressure)require(editor.releaseCpuWorkerGrant(9,grant),"release native pressure");pressure.clear();}
  auto request=core.takeCpuPendingRequest();if(request.empty())continue;
  auto value=render::internal::cpuDecodeRequest(request);require(bool(value),"ownerencoded exactrequest");
  if(mode=="radius"&&!core.cpuPendingFacts().historyJob){const auto& sweep=std::get<semantic::SweptCircleMask>(value->geometry);require(sweep.segments.front().p0.radius==(value->identity.words[7]==1?16:32),"capturedradiuschanged");}
  if((mode=="failure"||mode=="fatal"||mode=="failuremixed")&&!faulted){faulted=true;auto wrong=value->identity.words;++wrong[4];require(!core.queueCpuPendingCompletion(wrong,{},false),"foreignfailedbasis accepted");require(core.queueCpuPendingCompletion(value->identity.words,{},false),"failedterminal");jobs.push_back(value->identity.words);continue;}
render::internal::CpuExactPathJob job(value->geometry,value->prefixCount,value->prefixPath);while(!job.ready())require(job.advance(8,2000),"kernel bounded progress");
  const auto n=job.path().writeToMemory(nullptr);std::vector<std::uint8_t> result(128+n);std::copy_n(request.begin(),128,result.begin());
  const auto word=[&](unsigned p,std::uint32_t v){for(unsigned i=0;i<4;++i)result[p+i]=v>>(i*8);};word(0,0x31525043);word(4,1);word(96,n);word(100,value->piece?3:2);if(!value->piece)for(unsigned k=104;k<128;++k)result[k]=0;job.path().writeToMemory(result.data()+128);
  auto wrong=value->identity.words;++wrong[1];require(!core.queueCpuPendingCompletion(wrong,result,true),"foreignbasis accepted");
  if(mode=="context"&&!contextTested){contextTested=true;const auto gen=editor.semanticGeneration();require(core.loseSurface(),"contextlost");require(core.queueCpuPendingCompletion(value->identity.words,result,true),"holdresultwhilelost");editor.serviceCpuPendingErases();require(editor.semanticGeneration()==gen&&core.cpuPendingFacts().pending==2,"lost intent mutatedorvanished");require(core.rebindSurface({1431,1010,1431,1010,1,1}),"rebind");}
  else require(core.queueCpuPendingCompletion(value->identity.words,result,true),"matchingcompletion");require(!core.queueCpuPendingCompletion(value->identity.words,result,true),"duplicatecompletion");jobs.push_back(value->identity.words);
 }
 if(mode=="frame"||mode=="edgeframe"||mode=="mixedframe"||mode=="mixedimage")for(unsigned n=0;n<4096&&core.canonicalPresentationDirty();++n)frame();
 if(mode=="activebasis"){
  require(editor.selectionTransformActive(),"A mask commit cancelled active transform");pointer(input::PointerPhase::kMove,930,590);pointer(input::PointerPhase::kUp,930,590);editor.serviceCpuPendingErases();require(core.cpuPendingFacts().pending==0&&editor.pendingObject(mixedId)->transform.tx==880,"active transform did not rebase after generation");return 0;
 }
 if(mode=="postpublishfault"){require(faultSink.armed&&cpuFaultGeneration>generation&&cpuFaultGeneration!=UINT64_MAX,"postpublish fault not exercised after atomic mutation");require(ink_playground::InkPlaygroundHistoryTestAccess::full(editor).hasValue(),"postpublish recovery failed");cpuFaultRuntime=nullptr;editor.setLocalOperationSink(faultSink.inner);}
 const auto facts=core.cpuPendingFacts();json result{{"Up_ms",upMs},{"pending",facts.pending},{"committed",facts.committed},{"failed",facts.failed},{"noops",facts.noops},{"error",facts.error},{"jobs",jobs},{"opportunities",opportunities},{"generation",editor.semanticGeneration().value()},{"geometry_reuses",facts.geometryReuse},{"max_owner_ns",facts.maxOwnerNs},{"max_exact_ns",facts.maxExactNs},{"newquery_unions_main",ink_playground::InkPlaygroundHistoryTestAccess::work(editor).maskHistory.queryUnionOps-before.maskHistory.queryUnionOps}};
 std::ofstream(argv[2])<<result.dump(2)<<'\n';std::cout<<result.dump()<<std::endl;
 if(mode=="failuremixed"){require(!facts.pending&&facts.failed==1&&facts.committed==2&&editor.pendingObject(mixedId)&&editor.pendingObject(mixedId)->transform.tx==870,"failed A lost unrelated create/drag");return 0;}
 if(mode=="dependencychain"){require(!facts.pending&&facts.failed==3&&!editor.pendingObject(foundation::ObjectId::fromUint64(900003)),"transitive dependency survived failed predecessor");return 0;}
 if(mode=="failedcreate"){require(!facts.pending&&facts.failed==2&&facts.committed==4&&editor.pendingObject(foundation::ObjectId::fromUint64(900002)),"failed create lost unrelated suffix or dependent remained");return 0;}
 if(mode=="mixedimage"){require(!facts.pending&&!facts.failed&&facts.committed==3&&editor.pendingObject(mixedId)&&editor.pendingObject(mixedId)->transform.tx==870,"image final ordered transform");require(editor.undo()&&editor.undo(),"image undo");require(!editor.pendingObject(mixedId),"image deleted by Undo");return 0;}
 if(mode=="mixedbrush"){require(editor.pendingCanonicalHandoffCount()==1,"queued Brush skipped canonical handoff identity");require(facts.pending==0&&facts.failed==0&&facts.committed==2&&editor.semanticObjectCount()==41,"queued brush commit failed");return 0;}
 if(mode=="historybarrier"){require(!facts.pending&&!facts.failed&&facts.committed==4&&editor.pendingObject(mixedId)->transform.tx==800,"ordered undo target");require(!editor.pendingHistoryBarrier(),"history barrier never retired");return 0;}
 if(mode=="mixedcommit"||mode=="mixedframe"||mode=="mixedsticky"||mode=="mixedtable"){
  require(!facts.pending&&facts.failed==0&&facts.committed==3,"actual mixed head commits");const auto* actual=editor.pendingObject(mixedId);require(actual&&actual->transform.tx==870&&actual->transform.ty==540,"final transform from latest preimage");require(editor.undo(),"undo transform");require(editor.pendingObject(mixedId)->transform.tx==800,"undo preimage");require(editor.undo()&&!editor.pendingObject(mixedId),"undo create");require(editor.redo()&&editor.redo(),"redo create transform");require(editor.pendingObject(mixedId)->transform.tx==870,"redo final");return 0;
 }
 require(!facts.pending&&(((mode=="failure"||mode=="fatal")&&facts.failed==1&&facts.committed==1)||(mode!="failure"&&mode!="fatal"&&facts.failed==0&&((mode=="noop"&&facts.committed==1&&facts.noops==1)||(mode!="noop"&&facts.committed==2&&facts.noops==0)))),"ordered pending A/B didnotcommit");require(result["newquery_unions_main"]==0,"import consumer rebuiltnewquery");require(editor.releaseCpuWorkerGrant(1,workerFloor)&&editor.cpuWorkerGrantBytes()==0,"leaseclosed");require(core.close(),"finalclose");return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

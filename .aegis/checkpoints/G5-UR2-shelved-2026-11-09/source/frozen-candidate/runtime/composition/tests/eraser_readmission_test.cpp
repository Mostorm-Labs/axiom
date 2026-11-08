#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include <cstdio>
#include <stdexcept>
#include <string_view>
#include <cstdlib>
#include <new>

namespace { thread_local bool failNextTestAllocation=false; }
void* operator new(std::size_t n){if(failNextTestAllocation){failNextTestAllocation=false;throw std::bad_alloc();}if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}
using namespace canvas;
namespace {
void require(bool ok,const char* label){if(!ok)throw std::runtime_error(label);}
class SpyProvider final:public render::SkiaSurfaceProvider {
 public:
  render::RasterSkiaSurfaceProvider actual;
  bool busyAcquire=false,busyPresent=false;
  std::uint64_t acquires=0,presents=0;
  render::RenderTargetInfo describe()const noexcept override{return actual.describe();}
  render::SkiaSurfaceAcquireResult acquire()noexcept override{++acquires;return busyAcquire?render::SkiaSurfaceAcquireResult::rejected(render::SkiaSurfaceAcquireCode::kUnavailable,"busy fixture"):actual.acquire();}
  void release()noexcept override{actual.release();}
  render::BackendSubmissionResult present()noexcept override{++presents;return busyPresent?render::BackendSubmissionResult::retry("busy fixture"):actual.present();}
  render::BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return actual.resize(w,h);}
  render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{return actual.readbackRgba(out);}
  std::uint64_t generation()const noexcept override{return actual.generation();}
  render::BackendSubmissionResult advanceGeneration()noexcept override{return actual.advanceGeneration();}
  std::uint64_t readbackCount()const noexcept override{return actual.readbackCount();}
  std::uint64_t cpuCopyCount()const noexcept override{return actual.cpuCopyCount();}
  std::uint64_t presentCount()const noexcept override{return actual.presentCount();}
};
void admission(bool acquire,bool navigation=false){
  runtime::RuntimeCompositionCore c;require(c.open(),"open");
  auto canonical=std::make_unique<SpyProvider>(),preview=std::make_unique<SpyProvider>();
  auto* target=canonical.get();auto* overlay=preview.get();
  require(canonical->resize(256,256).code==render::BackendSubmissionCode::kAccepted,"resize canonical");
  require(preview->resize(256,256).code==render::BackendSubmissionCode::kAccepted,"resize preview");
  require(c.registerSurfaceProvider("canonical",std::move(canonical)),"register canonical");
  require(c.registerPreviewSurfaceProvider("preview",std::move(preview)),"register preview");
  require(c.bindSurface(256,256)&&c.registerSource(71),"bind/source");
  auto& q=c.qualificationRuntime();require(q.selectBrushProfile("round-handdrawn",4),"v4 profile");
  input::PlatformPointerBatch seed;
  for(unsigned i=0;i<=25;++i){input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=i+1;p.timestampNs=(i+1)*1000000;p.x=20+i*8;p.y=128;p.pressure=.5F;p.phase=i==0?input::PointerPhase::kDown:i==25?input::PointerPhase::kUp:input::PointerPhase::kMove;seed.samples.push_back(p);}
  require(c.submit(seed),"real v4 seed input");
  require(std::holds_alternative<semantic::CompactStrokeContent>(q.snapshotObjects().front().content),"real v4 content");
  for(unsigned i=0;i<3 && (q.pendingCanonicalHandoffCount()!=0 || q.previewActive());++i){
    runtime::RuntimeFrameOpportunity ready;require(c.serviceFrameOpportunity(true,ready),"seed frame");
    if(ready.previewWork)require(c.finishFrameOpportunity({ready.previewWork,true}),"seed preview complete");
    if(ready.canonicalWork)require(c.receivePlatformPresented({*ready.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"seed canonical complete");
  }
  require(q.pendingCanonicalHandoffCount()==0&&!q.previewActive(),"seed brush handoff complete before erase test");
  require(c.setPlatformPresentationDeferred(true)&&q.setEraserOptions(2,20),"deferred erase");
  std::uint64_t seq=100;
  const auto send=[&](input::PointerPhase phase,float x,float y){
    input::PlatformPointerSample p{};p.source=71;p.pointer=1;p.sequence=++seq;p.timestampNs=seq*1000000;p.x=x;p.y=y;p.phase=phase;p.pressure=.5F;
    return c.submit({{p}});
  };
  require(send(input::PointerPhase::kDown,80,100)&&send(input::PointerPhase::kMove,80,155)&&send(input::PointerPhase::kUp,80,155),"first erase real input");
  require(q.snapshotObjects().front().erase_masks.size()==1,"first erase canonical mask");
  if(navigation){
    const auto before=q.semanticGeneration();
    require(q.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,16,0,128,128,1}),"navigation admission");
    require(q.semanticGeneration()==before,"camera changed document");
    runtime::RuntimeFrameOpportunity frame;require(c.serviceFrameOpportunity(false,frame),"navigation frame");
    std::printf("{\"navigation_pending\":%s,\"navigation_presented\":%s,\"exact_requested\":%s}\n",frame.navigationPending?"true":"false",frame.navigationPresented?"true":"false",frame.canonicalRequested?"true":"false");std::fflush(stdout);
    require(frame.navigationPending||frame.navigationPresented,"post-Up current-Scene navigation must progress before exact canonical");
    require(c.close(),"close");return;
  }
  const auto generation=q.semanticGeneration();const auto ac=target->acquires,pc=target->presents,oa=overlay->acquires,op=overlay->presents;
  target->busyAcquire=acquire;target->busyPresent=!acquire;
  const bool accepted=send(input::PointerPhase::kDown,150,100);
  std::printf("{\"boundary\":\"%s\",\"accepted\":%s,\"active\":%s,\"canonical_acquires\":%llu,\"canonical_presents\":%llu,\"overlay_acquires\":%llu,\"overlay_presents\":%llu}\n",acquire?"acquire":"present",accepted?"true":"false",q.partialEraseFeedbackActive()?"true":"false",(unsigned long long)(target->acquires-ac),(unsigned long long)(target->presents-pc),(unsigned long long)(overlay->acquires-oa),(unsigned long long)(overlay->presents-op));std::fflush(stdout);
  require(accepted&&q.partialEraseFeedbackActive(),"next deferred Down must be admitted independently of canonical busy");
  require(target->acquires==ac&&target->presents==pc&&overlay->acquires==oa&&overlay->presents==op,"Down must not acquire/present/clear either surface");
  require(q.semanticGeneration()==generation,"Down changed semantic state");
  require(send(input::PointerPhase::kMove,150,155),"Move while busy");require(q.semanticGeneration()==generation,"Move changed semantics");
  require(send(input::PointerPhase::kCancel,150,155),"Cancel while busy");
  target->busyAcquire=target->busyPresent=false;require(c.close(),"close");
}
}

namespace canvas::interaction {
class EraseReadmissionTestAccess {
 public:
  static std::size_t sealed(const CanvasEditingRuntime& q){return q.sealedEraseFeedback_.size();}
  static std::size_t active(const CanvasEditingRuntime& q){return q.activeEraseFeedback_.size();}
  static std::size_t history(const CanvasEditingRuntime& q){return q.history_.size();}
  static bool sceneReady(const CanvasEditingRuntime& q){return q.historySceneReady();}
  static std::uint64_t retired(const CanvasEditingRuntime& q){return q.replacedEraseFeedback_;}
  static std::uint64_t superseded(const CanvasEditingRuntime& q){return q.supersededEraseFeedback_;}
  static std::unique_ptr<ISemanticSceneCompiler>& compiler(CanvasEditingRuntime& q){return q.sceneCompiler_;}
};
}
namespace {
using Access=interaction::EraseReadmissionTestAccess;
class CompilerFault final:public ISemanticSceneCompiler {
 public:
  std::unique_ptr<ISemanticSceneCompiler> actual;
  bool deltaFails=false,fullFails=false,fullThrows=false;
  mutable unsigned fullCalls=0,deltaCalls=0;
  explicit CompilerFault(std::unique_ptr<ISemanticSceneCompiler> value):actual(std::move(value)){}
  foundation::Result<CompiledSceneSnapshot> compileFull(const semantic::SemanticReadView& v)const override{
    ++fullCalls;if(fullThrows)throw std::bad_alloc{};if(fullFails)return foundation::Result<CompiledSceneSnapshot>::failure({foundation::ErrorCode::kRequiresFullRebuild,"fixture Scene unavailable"});return actual->compileFull(v);
  }
  foundation::Result<CompiledSceneDelta> compileDelta(const semantic::SemanticReadView& v,const semantic::ChangeSet& s)const override{
    ++deltaCalls;if(deltaFails)return foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kRequiresFullRebuild,"fixture Scene unavailable"});return actual->compileDelta(v,s);
  }
  foundation::Result<CompiledSceneDelta> compilePreparedDelta(const semantic::SemanticReadView& v,const semantic::ChangeSet& s,std::span<const RuntimeSceneRecord> records,const SceneReadView& view)const override{
    ++deltaCalls;if(deltaFails)return foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kRequiresFullRebuild,"fixture Scene unavailable"});return actual->compilePreparedDelta(v,s,records,view);
  }
};
struct Fixture {
  runtime::RuntimeCompositionCore c;
  SpyProvider* target=nullptr;SpyProvider* overlay=nullptr;
  std::uint64_t sequence=0;unsigned dpr=1;
  interaction::CanvasEditingRuntime& q(){return c.qualificationRuntime();}
  explicit Fixture(unsigned density=1):dpr(density){
    require(c.open(),"fixture open");auto a=std::make_unique<SpyProvider>(),b=std::make_unique<SpyProvider>();target=a.get();overlay=b.get();
    require(a->resize(256,256).code==render::BackendSubmissionCode::kAccepted&&b->resize(256,256).code==render::BackendSubmissionCode::kAccepted,"fixture provider resize");
    require(c.registerSurfaceProvider("canonical",std::move(a))&&c.registerPreviewSurfaceProvider("preview",std::move(b)),"fixture register");
    require(c.bindSurface(256,256)&&c.registerSource(71),"fixture bind/source");
    if(dpr!=1)require(c.resizeSurface(render::SurfaceMetrics{256,256,256*dpr,256*dpr,float(dpr),1}),"fixture DPR");
    semantic::ObjectRecord backdrop;backdrop.id=foundation::ObjectId::fromUint64(50001);backdrop.kind=semantic::ObjectKind::kShape;backdrop.kind_version=1;
    backdrop.content=semantic::ShapeContent{1,256,256};backdrop.placement.order_key=semantic::OrderKey({1});
    backdrop.properties.entries.push_back({0x100U,semantic::FillStyleValue{semantic::SolidFill{{0,0,1,1}}}});
    semantic::Operation insert{semantic::OperationId{foundation::ObjectId::fromUint64(51001)},q().documentId(),1,1,semantic::InsertObjectsOp{{backdrop}}};
    require(c.applyExternalOperation(insert,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"fixture backdrop");
    require(q().selectBrushProfile("round-handdrawn",4)&&q().setInkOptions(16,0,0,0,1,.65F),"fixture v4 paint");
    require(send(input::PointerPhase::kDown,20,128),"fixture stroke Down");
    for(unsigned i=1;i<25;++i)require(send(input::PointerPhase::kMove,20+i*8,128),"fixture stroke Move");
    require(send(input::PointerPhase::kUp,220,128),"fixture stroke Up");
    for(unsigned i=0;i<3 && (q().previewActive()||q().pendingCanonicalHandoffCount());++i){
      runtime::RuntimeFrameOpportunity f;require(c.serviceFrameOpportunity(true,f),"fixture seed frame");
      if(f.previewWork)require(c.finishFrameOpportunity({f.previewWork,true}),"fixture preview completion");
      if(f.canonicalWork)require(complete(*f.canonicalWork),"fixture canonical completion");
    }
    require(!q().previewActive()&&!q().pendingCanonicalHandoffCount(),"fixture no ARC backlog");
    require(c.setPlatformPresentationDeferred(true)&&q().setEraserOptions(2,20),"fixture erase mode");
  }
  ~Fixture(){(void)c.close();}
  bool send(input::PointerPhase phase,float x,float y,std::uint64_t pointer=1,input::PointerTool tool=input::PointerTool::kUnknown){
    input::PlatformPointerSample p{};p.source=71;p.pointer=pointer;p.sequence=++sequence;p.timestampNs=sequence*1000000;p.x=x;p.y=y;p.phase=phase;p.pressure=.5F;p.capabilities.tool=tool;return c.submit({{p}});
  }
  void erase(float x){require(send(input::PointerPhase::kDown,x,100)&&send(input::PointerPhase::kMove,x,155)&&send(input::PointerPhase::kUp,x,155),"fixture erase");}
  std::size_t masks(){for(const auto& r:q().snapshotObjects())if(std::holds_alternative<semantic::CompactStrokeContent>(r.content))return r.erase_masks.size();throw std::runtime_error("missing v4");}
  runtime::CanonicalWork exact(){for(unsigned i=0;i<256;++i){runtime::RuntimeFrameOpportunity f;require(c.serviceFrameOpportunity(true,f),"exact frame opportunity");if(f.canonicalWork&&f.canonicalWork->frame.sceneGeneration==q().semanticGeneration().value())return *f.canonicalWork;}throw std::runtime_error("exact work incomplete");}
  bool complete(const runtime::CanonicalWork& work){return c.receivePlatformPresented({work,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}});}
  std::vector<std::uint8_t> pixels(){std::vector<std::uint8_t> b(256*256*dpr*dpr*4);require(target->readbackRgba(b).code==render::BackendSubmissionCode::kAccepted,"pixels");return b;}
  unsigned blue(const std::vector<std::uint8_t>& p,unsigned x){return p[(128*dpr*256*dpr+x*dpr)*4+2];}
};
void handoff(){
  for(const unsigned dpr:{1U,2U,4U}){
    Fixture f(dpr);f.erase(80);require(Access::sealed(f.q())==1,"seal A");const auto a=f.exact();
    require(f.send(input::PointerPhase::kDown,150,100)&&f.send(input::PointerPhase::kMove,150,155),"B admitted while A in flight");
    runtime::RuntimeFrameOpportunity preview;require(f.c.serviceFrameOpportunity(false,preview),"B feedback");
    const auto before=f.pixels();require(f.blue(before,80)>245&&f.blue(before,150)>245&&f.blue(before,195)<200,"current Scene plus active mask/backdrop/opacity");
    require(f.complete(a),"A qualified completion");require(Access::sealed(f.q())==0&&Access::active(f.q())==1,"A completion preserved active B");
    require(before==f.pixels(),"A callback changed B pixels");const auto generation=f.q().semanticGeneration();
    require(f.send(input::PointerPhase::kCancel,150,155),"B cancel");require(f.q().semanticGeneration()==generation&&f.masks()==1,"cancel changed commit");
    const auto clean=f.exact();require(f.complete(clean),"cancel exact completion");require(f.blue(f.pixels(),150)<200,"cancel did not restore canonical ink");
  }
  {
    Fixture f;f.erase(80);const auto a=f.exact();f.erase(150);const auto b=f.exact();
    require(!f.complete(a)&&Access::sealed(f.q())==2,"superseded old completion retired feedback");
    require(f.send(input::PointerPhase::kDown,200,100),"active C");require(f.complete(b),"B exact completes A/B");
    require(Access::sealed(f.q())==0&&Access::active(f.q())==1&&Access::retired(f.q())==2,"B frozen roster cleared C or lost A");
    require(f.send(input::PointerPhase::kCancel,200,100),"C cancel");
  }
  {
    Fixture f;f.erase(80);
    require(f.q().presentCanonicalFrame(f.q().nextCanonicalFrameId(),0,false),"prepare exact A without Core capture");
    f.erase(150);require(f.send(input::PointerPhase::kDown,200,100),"C before capture");
    runtime::RuntimeFrameOpportunity opportunity;require(f.c.serviceFrameOpportunity(false,opportunity)&&opportunity.canonicalWork.has_value(),"capture exact A");
    require(f.complete(*opportunity.canonicalWork),"A completion after capture");
    require(Access::sealed(f.q())==1&&Access::active(f.q())==1,"capture expanded frozen A roster to B/C");
    require(f.send(input::PointerPhase::kCancel,200,100),"cancel C");const auto b=f.exact();require(f.complete(b)&&Access::sealed(f.q())==0,"B exact retire remaining seal");
  }
  {
    Fixture f;f.erase(80);require(f.q().undo(),"Undo before replacement");const auto current=f.exact();require(f.complete(current),"current undone state replacement");
    require(Access::superseded(f.q())==1&&Access::sealed(f.q())==0&&f.masks()==0,"Undo effect not superseded correctly");
    require(f.q().redo(),"Redo");const auto redo=f.exact();require(f.complete(redo)&&f.masks()==1,"Redo exactly once");
  }
  std::puts("handoff pixels DPR1/2/4; frozen successful work; supersession; Undo/Redo PASS");
}
void sceneRecovery(){
  Fixture f;auto wrapped=std::make_unique<CompilerFault>(std::move(Access::compiler(f.q())));auto* fault=wrapped.get();Access::compiler(f.q())=std::move(wrapped);
  fault->deltaFails=fault->fullFails=true;const auto history=Access::history(f.q());f.erase(80);
  require(!Access::sceneReady(f.q())&&f.masks()==1&&Access::history(f.q())==history+1,"derived failure lost accepted commit/History");
  const auto generation=f.q().semanticGeneration();const auto acquires=f.target->acquires;
  fault->fullThrows=true;runtime::RuntimeFrameOpportunity unavailable;
  require(!f.c.serviceFrameOpportunity(false,unavailable)&&f.q().semanticGeneration()==generation&&f.target->acquires==acquires,"throwing derived recovery escaped or presented");
  fault->fullThrows=false;
  require(f.send(input::PointerPhase::kDown,150,100)&&f.send(input::PointerPhase::kMove,150,155),"B admission with lagging Scene");
  require(!f.send(input::PointerPhase::kUp,150,155),"stale Scene query accepted");
  require(f.q().semanticGeneration()==generation&&f.masks()==1&&!f.q().partialEraseFeedbackActive(),"failed recovery mutated or leaked session");
  fault->deltaFails=fault->fullFails=false;f.erase(150);
  require(Access::sceneReady(f.q())&&fault->fullCalls>0&&f.masks()==2&&Access::history(f.q())==history+2,"independent Scene recovery before next exact hit");
  require(f.target->acquires==acquires,"Scene preparation required pixel present");
  std::puts("Scene failure/recovery before exact query; no present dependency PASS");
}
void lifecycle(){
  {
    Fixture f;f.erase(80);const auto a=f.exact();
    for(unsigned kind=0;kind<10;++kind){auto wrong=a;if(kind==0)++wrong.runtimeInstanceId;if(kind==1)++wrong.lifetimeGeneration;if(kind==2)++wrong.attachmentEpoch;if(kind==3)++wrong.workId;if(kind==4)wrong.frame.surfaceGeneration=render::SurfaceGeneration{999};if(kind==5)wrong.frame.metricsGeneration=render::MetricsGeneration{999};if(kind==6)++wrong.frame.documentRevision;if(kind==7)++wrong.frame.sceneGeneration;if(kind==8)wrong.frame.viewId=render::ViewId{999};if(kind==9)wrong.frame.frameId=render::FrameId{999};require(!f.complete(wrong)&&Access::sealed(f.q())==1,"foreign completion retired sealed feedback");}
    require(!f.c.receivePlatformPresented({a,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kApproximate,{}}),"unqualified completion accepted");
    require(f.c.resizeSurface(render::SurfaceMetrics{256,256,512,512,2,1}),"resize");require(Access::sealed(f.q())==0&&!f.complete(a),"resize retained/qualified old display demand");
  }
  {
    Fixture f;f.erase(80);const auto a=f.exact();require(f.c.loseSurface(),"lost");require(Access::sealed(f.q())==0&&!f.complete(a),"lost qualified old work");
    require(!f.send(input::PointerPhase::kDown,150,100),"lost surface admitted input");require(f.c.rebindSurface(),"rebind");
  }
  {
    Fixture f;const auto g=f.q().semanticGeneration();const auto h=Access::history(f.q());f.erase(245);
    require(f.q().semanticGeneration()==g&&Access::history(f.q())==h,"no-op mutated History");const auto noOp=f.exact();require(f.complete(noOp)&&Access::sealed(f.q())==0,"no-op handoff leaked");
    f.erase(80);require(f.send(input::PointerPhase::kDown,150,100),"source lost session");++f.sequence;require(f.c.unregisterSource(71,f.sequence,f.sequence*1000000),"source lost");
    require(!f.q().partialEraseFeedbackActive()&&Access::active(f.q())==0&&Access::sealed(f.q())==1,"source loss cleared committed seal or leaked active");
    require(f.c.registerSource(71),"source reuse");require(f.send(input::PointerPhase::kDown,150,100),"pointer reuse");require(!f.send(input::PointerPhase::kDown,150,100),"duplicate contact accepted");(void)f.c.cancelAllPointers();
  }
  std::puts("identity, approximate/foreign completion, no-op, cancel, source/lost/resize/rebind PASS");
}
class PublicationFault final:public interaction::LocalOperationSink {
 public:
  enum class Mode{RejectPublication,FailHistory,RejectApply,FailDerived};
  Mode mode;interaction::CanvasEditingRuntime& runtime;std::size_t expectedHistory;
  unsigned prepares=0,aborts=0,publications=0;bool historyFinalized=false;
  PublicationFault(Mode m,interaction::CanvasEditingRuntime& q,std::size_t h):mode(m),runtime(q),expectedHistory(h){}
  bool preparePublication(std::span<const semantic::Operation> values)noexcept override{
    ++prepares;if(mode==Mode::RejectPublication)return false;
    if(mode==Mode::FailHistory)failNextTestAllocation=true;
    if(mode==Mode::RejectApply)const_cast<semantic::Operation&>(values.front()).schema_version=999;
    return true;
  }
  void abortPreparedPublication()noexcept override{++aborts;}
  void applied(const semantic::Operation&,const semantic::CanonicalCommitRecord&)noexcept override{
    ++publications;historyFinalized=Access::history(runtime)==expectedHistory;
    if(mode==Mode::FailDerived)failNextTestAllocation=true;
  }
  interaction::AxisOperationStatus status(const semantic::OperationId&,const semantic::CanonicalCommitStamp&)const noexcept override{return {};}
};
void historySafety(){
  for(auto mode:{PublicationFault::Mode::RejectPublication,PublicationFault::Mode::FailHistory,PublicationFault::Mode::RejectApply,PublicationFault::Mode::FailDerived}){
    Fixture f;f.erase(80);require(f.q().undo()&&f.q().canRedo(),"History failure precondition");
    const auto original=f.q().snapshotObjects();const auto generation=f.q().semanticGeneration();const auto h=Access::history(f.q());const auto count=f.q().submittedOperationCount();
    PublicationFault sink(mode,f.q(),h); // reserve-before-apply must keep the redo tail until commit
    if(mode==PublicationFault::Mode::FailDerived)sink.expectedHistory=h;
    f.q().setLocalOperationSink(&sink);
    require(f.send(input::PointerPhase::kDown,150,100)&&f.send(input::PointerPhase::kMove,150,155),"History test input");
    const bool accepted=f.send(input::PointerPhase::kUp,150,155);const bool allocationConsumed=!failNextTestAllocation;failNextTestAllocation=false;
    f.q().setLocalOperationSink(nullptr);
    const bool applied=mode==PublicationFault::Mode::FailDerived;
    require(accepted==applied,"wrong erase transaction acceptance");
    if(!applied){require(f.q().semanticGeneration()==generation&&f.q().snapshotObjects()==original&&Access::history(f.q())==h&&f.q().canRedo(),"prepare/apply failure changed document/History");require(sink.publications==0,"publication without canonical commit");}
    else{require(f.q().semanticGeneration().value()==generation.value()+1&&f.masks()==1&&sink.publications==1&&sink.historyFinalized,"accepted erase lost History/publication");require(f.q().submittedOperationCount()==count+1,"erase duplicated commit");require(f.q().undo()&&f.masks()==0,"derived failure made erase not undoable");}
    if(mode==PublicationFault::Mode::FailHistory||mode==PublicationFault::Mode::FailDerived)require(allocationConsumed,"fault not reached");
    require(sink.prepares==1&&sink.aborts==1&&!f.q().partialEraseFeedbackActive(),"reservation/session cleanup");
  }
  std::puts("History/publication preflight failure, apply rejection, post-commit derived failure, exactly-once Undo PASS");
}
void navigationSafety(){
  for(unsigned dpr:{1U,2U,4U}){
    Fixture f(dpr);f.erase(80);const auto g=f.q().semanticGeneration();const auto h=Access::history(f.q());
    require(f.q().applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,8,0,128,128,1}),"pan admission");
    bool feedback=false;
    for(unsigned i=0;i<128&&!feedback;++i){runtime::RuntimeFrameOpportunity op;require(f.c.serviceFrameOpportunity(false,op),"post-Up navigation opportunity");feedback=op.navigationPresented;require(!op.canonicalRequested,"navigation forced full exact canonical");}
    require(feedback&&Access::sealed(f.q())==1,"navigation failed to present or retired seal");
    const auto feedbackPixels=f.pixels();require(f.blue(feedbackPixels,72)>245&&f.blue(feedbackPixels,187)<200,"post-Up navigation resurrected masked ink or lost backdrop");
    require(f.q().semanticGeneration()==g&&Access::history(f.q())==h,"camera mutated canonical state");
    const auto frame=f.exact();require(f.complete(frame)&&Access::sealed(f.q())==0,"eventual exact fairness");
  }
  {
    Fixture f;f.erase(80);f.target->busyAcquire=true;const auto g=f.q().semanticGeneration();
    require(f.q().setViewportZoomAt(1.1F,128,128),"busy zoom accepted camera");runtime::RuntimeFrameOpportunity busy;require(f.c.serviceFrameOpportunity(false,busy),"busy navigation must retain demand");
    require(!busy.navigationPresented&&Access::sealed(f.q())==1,"busy target invented visible replacement");
    f.target->busyAcquire=false;runtime::RuntimeFrameOpportunity retry;require(f.c.serviceFrameOpportunity(false,retry),"navigation retry");
    require(retry.navigationPending||retry.navigationPresented,"camera demand dropped after busy");require(f.q().semanticGeneration()==g,"zoom changed document");
  }
  std::puts("post-Up pan/zoom admission, current-Scene feedback before exact, busy retry, fairness PASS");
}
}

int main(int argc,char** argv){try{const std::string_view mode=argc>1?argv[1]:"acquire";if(mode=="handoff")handoff();else if(mode=="scene")sceneRecovery();else if(mode=="history")historySafety();else if(mode=="lifecycle")lifecycle();else if(mode=="navigation-safety")navigationSafety();else admission(mode!="present",mode=="navigation");return 0;}catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 1;}}

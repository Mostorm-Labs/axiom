#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/semantic/compact_stroke.hpp"
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <thread>
#include <chrono>
#include <atomic>
#include <new>
#include "canvas/scene/scene_compiler.hpp"
#include <stdexcept>
#include <string_view>

namespace { std::atomic<bool> failNextAllocation{false};std::atomic<unsigned> allocationFaults{0}; }
void* operator new(std::size_t size){if(failNextAllocation.exchange(false)){++allocationFaults;throw std::bad_alloc();}if(auto* p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t size){return ::operator new(size);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
public:
 static std::size_t records(const interaction::CanvasEditingRuntime& r){return std::count_if(r.displayRecords_.begin(),r.displayRecords_.end(),[](const auto& x){return x.serial!=0;});}
 static std::size_t history(const interaction::CanvasEditingRuntime& r){return r.history_.size();}
 static std::size_t covered(const interaction::CanvasEditingRuntime& r){return std::count_if(r.displayRecords_.begin(),r.displayRecords_.end(),[](const auto& x){return x.serial&&x.covered;});}
 static std::uint64_t serial(const interaction::CanvasEditingRuntime& r){return r.nextDisplaySerial_-1;}
 static bool hasSerial(const interaction::CanvasEditingRuntime& r,std::uint64_t serial){return std::any_of(r.displayRecords_.begin(),r.displayRecords_.end(),[&](const auto& x){return x.serial==serial;});}
 static bool ready(const interaction::CanvasEditingRuntime& r){return r.historySceneReady();}
 static auto takeCompiler(interaction::CanvasEditingRuntime& r){return std::move(r.sceneCompiler_);}
 static void compiler(interaction::CanvasEditingRuntime& r,std::unique_ptr<ISemanticSceneCompiler> c){r.sceneCompiler_=std::move(c);}
 static void fill(interaction::CanvasEditingRuntime& r){for(auto& x:r.displayRecords_){x={};x.serial=r.nextDisplaySerial_++;x.session=x.serial+100000;}}
 static void drainFaultSlots(interaction::CanvasEditingRuntime& r){for(auto& x:r.displayRecords_)if(x.session>=100000)x={};}
};
}
using namespace canvas;
using Access=canvas::ink_playground::InkPlaygroundHistoryTestAccess;
namespace {
void require(bool ok,const char* message){if(!ok){std::fprintf(stderr,"UR2A_TEST_FAILURE %s\n",message);std::fflush(stdout);std::exit(std::string_view(message).starts_with("BEHAVIOR:")?1:2);}}
struct SpyProvider final : render::SkiaSurfaceProvider {
    std::unique_ptr<render::SkiaSurfaceProvider> actual;
    bool busyAcquire=false,busyPresent=false;
    unsigned acquisitions=0,releases=0,presentAttempts=0,visibilityCalls=0;
    explicit SpyProvider(bool overlay=false){
        if(overlay)actual=std::make_unique<render::TransparentOverlaySkiaSurfaceProvider>();
        else actual=std::make_unique<render::RasterSkiaSurfaceProvider>();
    }
    void resetCounts(){acquisitions=releases=presentAttempts=visibilityCalls=0;}
    render::RenderTargetInfo describe()const noexcept override{return actual->describe();}
    render::SkiaSurfaceAcquireResult acquire()noexcept override{
        ++acquisitions;return busyAcquire?render::SkiaSurfaceAcquireResult::rejected(render::SkiaSurfaceAcquireCode::kUnavailable,"labelled test backbuffer busy"):actual->acquire();
    }
    void release()noexcept override{++releases;actual->release();}
    render::BackendSubmissionResult present()noexcept override{++presentAttempts;return busyPresent?render::BackendSubmissionResult::retry("labelled test present busy"):actual->present();}
    render::BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return actual->resize(w,h);}
    render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> p)noexcept override{return actual->readbackRgba(p);}
    std::uint64_t generation()const noexcept override{return actual->generation();}
    render::BackendSubmissionResult advanceGeneration()noexcept override{return actual->advanceGeneration();}
    std::uint64_t readbackCount()const noexcept override{return actual->readbackCount();}
    std::uint64_t cpuCopyCount()const noexcept override{return actual->cpuCopyCount();}
    std::uint64_t presentCount()const noexcept override{return actual->presentCount();}
    void setOverlayVisible(bool v)noexcept override{++visibilityCalls;actual->setOverlayVisible(v);}
    bool overlayVisible()const noexcept override{return actual->overlayVisible();}
};
struct Host {
    runtime::RuntimeCompositionCore core;
    SpyProvider *canonical=nullptr,*overlay=nullptr;
    std::uint64_t sequence=0,request=0;
    unsigned seedQualified=0;
    explicit Host(bool qualifySeed=true){
        require(core.open(),"fixture open");
        auto c=std::make_unique<SpyProvider>(),p=std::make_unique<SpyProvider>(true);canonical=c.get();overlay=p.get();
        require(c->resize(256,128).code==render::BackendSubmissionCode::kAccepted&&p->resize(256,128).code==render::BackendSubmissionCode::kAccepted,"fixture targets");
        require(core.registerSurfaceProvider("ur2a-canonical",std::move(c))&&core.registerPreviewSurfaceProvider("ur2a-preview",std::move(p)),"fixture register");
        require(core.bindSurface(256,128)&&core.registerSource(71)&&core.setPlatformPresentationDeferred(true),"fixture bind/source/deferred");
        runtime::CanvasControlPayload preset{};preset.kind=runtime::CanvasControlPayloadKind::kSelectPreset;preset.preset={"round-handdrawn",4};control(preset);
        runtime::CanvasControlPayload ink{};ink.kind=runtime::CanvasControlPayloadKind::kInkOptions;ink.ink.size=runtime::OptionPatch<float>::set(24);control(ink);
        require(pointer(input::PointerPhase::kDown,20,64),"fixture real Brush down");
        for(unsigned i=1;i<=24;++i)require(pointer(input::PointerPhase::kMove,20+i*8,64),"fixture real Brush move");
        require(pointer(input::PointerPhase::kUp,220,64),"fixture real Brush Up");
        const auto objects=core.qualificationRuntime().snapshotObjects();require(objects.size()==1&&objects[0].kind_version==3,"fixture nonempty v4 object");
        const auto* content=std::get_if<semantic::CompactStrokeContent>(&objects[0].content);require(content&&content->stroke.config.algorithmVersion==4&&!content->stroke.knots.empty(),"fixture real v4 knots");
        if(qualifySeed)qualifyAll();if(qualifySeed)require(!core.previewActive()&&core.qualificationRuntime().pendingCanonicalHandoffCount()==0&&seedQualified>0,"fixture seed handoff genuinely qualified");
        runtime::CanvasControlPayload tool{};tool.kind=runtime::CanvasControlPayloadKind::kSelectTool;tool.tool=runtime::CanvasToolKind::kEraser;control(tool);
        runtime::CanvasControlPayload options{};options.kind=runtime::CanvasControlPayloadKind::kEraserOptions;options.eraser.mode=runtime::OptionPatch<std::uint32_t>::set(2);options.eraser.diameterLogicalPx=runtime::OptionPatch<float>::set(20);control(options);
        if(qualifySeed)qualifyAll();
        std::printf("{\"stage\":\"qualified-seed\",\"kind_version\":3,\"algorithm_version\":4,\"knots\":%zu,\"semantic\":%llu,\"qualified_receipts\":%u,\"pending_brush\":%zu,\"preview_active\":%s}\n",content->stroke.knots.size(),static_cast<unsigned long long>(core.readback().semanticGeneration),seedQualified,core.qualificationRuntime().pendingCanonicalHandoffCount(),core.previewActive()?"true":"false");
    }
    void control(runtime::CanvasControlPayload payload){
        auto& facade=core.controlFacade();runtime::CanvasControlRequest r{};r.clientId=711;r.requestId=++request;r.target=facade.mountedCanvasTargets().front();r.payload=std::move(payload);
        require(facade.submitCanvasControl(r).state==runtime::ProductControlState::kQueued&&core.ownerPump(),"fixture actual control admission/pump");
    }
    bool pointer(input::PointerPhase phase,float x,float y,std::uint64_t id=1){
        input::PlatformPointerSample s{};s.source=71;s.pointer=id;s.sequence=++sequence;s.timestampNs=sequence*8000000;s.x=x;s.y=y;s.pressure=.5F;s.phase=phase;
        return core.submit({{s}});
    }
    void qualifyAll(){
        for(unsigned i=0;i<8192;++i){
            runtime::RuntimeFrameOpportunity f;require(core.serviceFrameOpportunity(false,f),"fixture actual frame opportunity");
            if(f.previewWork)require(core.finishFrameOpportunity({f.previewWork,true}),"fixture actual preview acknowledgement");
            if(f.canonicalWork){require(core.receivePlatformPresented({*f.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"fixture strict actual CanonicalWork qualifier");++seedQualified;}
            if(!core.canonicalPresentationDirty()&&!core.lastSubmittedCanonicalWork()&&!core.previewActive()&&core.qualificationRuntime().pendingCanonicalHandoffCount()==0)return;
        }
        require(false,"fixture seed8192 opportunity guard");
    }
    runtime::CanonicalWork submitExact(){
        for(unsigned i=0;i<8192;++i){runtime::RuntimeFrameOpportunity f;require(core.serviceFrameOpportunity(true,f),"actual exact attempt");
            if(f.canonicalWork)return *f.canonicalWork;
        }
        require(false,"exact8192 guard");return {};
    }
    void complete(const runtime::CanonicalWork& w){require(core.receivePlatformPresented({w,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"actual qualified completion");}
    void shortErase(float x,std::uint64_t pointerId=1){
        require(pointer(input::PointerPhase::kDown,x,42,pointerId),"short Down");require(pointer(input::PointerPhase::kMove,x,85,pointerId),"short move");require(pointer(input::PointerPhase::kUp,x,85,pointerId),"short Up");
    }
    void eraseA(){
        const auto before=core.readback().semanticGeneration;
        require(pointer(input::PointerPhase::kDown,90,45),"fixture A eraseDown");
        require(pointer(input::PointerPhase::kMove,90,83),"fixture A eraseMove");
        require(pointer(input::PointerPhase::kUp,90,83),"fixture A eraseUp");
        const auto objects=core.qualificationRuntime().snapshotObjects();require(core.readback().semanticGeneration==before+1&&objects.size()==1&&!objects[0].erase_masks.empty(),"fixture A non-noop masks committed once");
        require(core.canonicalPresentationDirty(),"fixture A canonical pending");
        std::printf("{\"stage\":\"A-committed-pending\",\"semantic\":%llu,\"scene\":%llu,\"masks\":%zu}\n",static_cast<unsigned long long>(core.readback().semanticGeneration),static_cast<unsigned long long>(core.readback().sceneRevision),objects[0].erase_masks.size());
    }
};
void readmission(bool acquire){
    Host h;h.eraseA();h.canonical->resetCounts();h.overlay->resetCounts();h.canonical->busyAcquire=acquire;h.canonical->busyPresent=!acquire;
    // Prepare the existing actual job until the selected target denial is
    // reached, without accepting any exact frame. This avoids a cold compiler
    // yield masquerading as the provider-dependent admission regression.
    for(unsigned i=0;i<8192;++i){
        runtime::RuntimeFrameOpportunity f;require(h.core.serviceFrameOpportunity(true,f),"fixture busy preparation opportunity");
        require(!f.canonicalWork,"fixture busy target cannot qualify exact");
        if(acquire?h.canonical->acquisitions>0:h.canonical->presentAttempts>0)break;
        require(i+1<8192,"fixture actual busy target not reached");
    }
    h.canonical->resetCounts();h.overlay->resetCounts();const auto generation=h.core.readback().semanticGeneration;
    const bool accepted=h.pointer(input::PointerPhase::kDown,150,45);
    std::printf("{\"stage\":\"baseline-readmission\",\"denial\":\"%s\",\"accepted\":%s,\"canonical_acquire\":%u,\"canonical_present\":%u,\"preview_acquire\":%u,\"preview_present\":%u,\"preview_visibility_calls\":%u,\"semantic_unchanged\":%s}\n",acquire?"acquire":"present",accepted?"true":"false",h.canonical->acquisitions,h.canonical->presentAttempts,h.overlay->acquisitions,h.overlay->presentAttempts,h.overlay->visibilityCalls,h.core.readback().semanticGeneration==generation?"true":"false");
    require(accepted&&h.canonical->acquisitions==0&&h.canonical->presentAttempts==0&&h.overlay->acquisitions==0&&h.overlay->presentAttempts==0&&h.overlay->visibilityCalls==0,"BEHAVIOR: B Down must not depend on or touch either surface");
    require(h.core.readback().semanticGeneration==generation,"B Down semantic mutation");
}
void navigation(){
    Host h;h.eraseA();interaction::ViewportNavigationSample nav{};nav.deltaX=12;
    require(h.core.applyViewportNavigation(nav),"fixture actual viewport admission");
    runtime::RuntimeFrameOpportunity f;require(h.core.serviceFrameOpportunity(false,f),"fixture actual post-Up navigation opportunity");
    std::printf("{\"stage\":\"baseline-postUp-navigation\",\"navigation_pending\":%s,\"navigation_presented\":%s,\"canonical_requested\":%s,\"canonical_work\":%s,\"semantic\":%llu}\n",f.navigationPending?"true":"false",f.navigationPresented?"true":"false",f.canonicalRequested?"true":"false",f.canonicalWork?"true":"false",static_cast<unsigned long long>(h.core.readback().semanticGeneration));
    require(f.navigationPending||f.navigationPresented,"BEHAVIOR: current-Scene navigation cannot wait for a current exact plane");
}
void handoff(){
 Host h;auto& q=h.core.qualificationRuntime();h.eraseA();h.shortErase(150);
 require(h.core.readback().semanticGeneration==3&&Access::records(q)==2,"two Ups commit without opportunity");
 const auto b=h.submitExact();
 // A later C must not become eligible just because completion arrives later.
 require(h.pointer(input::PointerPhase::kDown,185,42),"C Down before B completion");
 require(h.pointer(input::PointerPhase::kMove,185,85),"C move");const auto c=Access::serial(q);
 auto wrong=b;++wrong.workId;require(!h.core.receivePlatformPresented({wrong,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"wrong work accepted");
 h.complete(b);require(q.transientOverlayDirty(),"BEHAVIOR: old B completion erased later C feedback demand");require(Access::records(q)==1&&Access::hasSerial(q,c)&&q.partialEraseFeedbackActive(),"B cleared later C");
 require(h.pointer(input::PointerPhase::kUp,185,85),"C Up");h.qualifyAll();require(Access::records(q)==0,"sealed C leaked");
 require(!h.core.receivePlatformPresented({b,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"old B replay accepted");
 Host brush(false);auto& r=brush.core.qualificationRuntime();interaction::PreviewPresentationCapture cap;std::uint64_t count=0;
 require(r.capturePreviewPresentation(cap)&&r.renderPreviewPresentation(cap,&count)&&r.acknowledgePreviewPresentation(cap,count),"B own Brush preview qualifier");
 const auto own=brush.submitExact();require(brush.pointer(input::PointerPhase::kDown,90,45)&&brush.pointer(input::PointerPhase::kMove,90,83),"new C during own Brush completion");
 brush.complete(own);require(r.transientOverlayDirty()&&r.partialEraseFeedbackActive(),"BEHAVIOR: own Brush B completion erased later C feedback demand");
 require(brush.pointer(input::PointerPhase::kUp,90,83),"later C commit");brush.qualifyAll();
}
struct FaultCompiler final:ISemanticSceneCompiler {
 std::unique_ptr<ISemanticSceneCompiler> delegate;bool fail=true;
 explicit FaultCompiler(std::unique_ptr<ISemanticSceneCompiler> d):delegate(std::move(d)){}
 auto compileFull(const semantic::SemanticReadView& v)const ->foundation::Result<CompiledSceneSnapshot> override{
   return fail?foundation::Result<CompiledSceneSnapshot>::failure({foundation::ErrorCode::kInvalidArgument,"labelled Scene projection fault"}):delegate->compileFull(v);}
 auto compileDelta(const semantic::SemanticReadView& v,const semantic::ChangeSet& c)const ->foundation::Result<CompiledSceneDelta> override{
   return fail?foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kInvalidArgument,"labelled Scene projection fault"}):delegate->compileDelta(v,c);}
 auto compilePreparedDelta(const semantic::SemanticReadView& v,const semantic::ChangeSet& c,std::span<const RuntimeSceneRecord> r,const SceneReadView& b)const ->foundation::Result<CompiledSceneDelta> override{
   return fail?foundation::Result<CompiledSceneDelta>::failure({foundation::ErrorCode::kInvalidArgument,"labelled Scene projection fault"}):delegate->compilePreparedDelta(v,c,r,b);}
};
void sceneRecovery(){
 Host h;auto& q=h.core.qualificationRuntime();auto f=std::make_unique<FaultCompiler>(Access::takeCompiler(q));auto* fault=f.get();Access::compiler(q,std::move(f));
 const auto history=Access::history(q);h.eraseA();require(!Access::ready(q)&&Access::history(q)==history+1,"Scene failure changed accepted history");
 h.canonical->resetCounts();h.overlay->resetCounts();require(!h.pointer(input::PointerPhase::kDown,150,42),"unrecoverable Scene accepted Down");
 require(h.canonical->acquisitions==0&&h.overlay->acquisitions==0,"Scene recovery touched pixels");
 fault->fail=false;require(h.pointer(input::PointerPhase::kDown,150,42),"recovered pointer reuse rejected");
 require(Access::ready(q),"Down used stale Scene");require(h.pointer(input::PointerPhase::kMove,150,85)&&h.pointer(input::PointerPhase::kUp,150,85),"recovered B terminal");
 require(h.core.readback().semanticGeneration==3&&Access::history(q)==history+2,"recovery reapplied A");
 h.qualifyAll();
}
struct Publication final:interaction::LocalOperationSink{
 runtime::RuntimeCompositionCore* core=nullptr;bool allow=false,allocationFault=false;unsigned prepared=0,published=0,aborted=0;bool reentryRejected=false;
 bool preparePublication(std::span<const semantic::Operation>)noexcept override{++prepared;return allow;}
 void abortPreparedPublication()noexcept override{++aborted;}
 void applied(const semantic::Operation&,const semantic::CanonicalCommitRecord& r)noexcept override{++published;require(!r.change_set.objects().empty(),"complete change_set lost");if(core)reentryRejected=!core->qualificationRuntime().eraserBegin(9001);if(allocationFault)failNextAllocation=true;}
 interaction::AxisOperationStatus status(const semantic::OperationId&,const semantic::CanonicalCommitStamp&)const noexcept override{return {};}
};
void history(){
 Host h;auto& q=h.core.qualificationRuntime();Publication sink;sink.core=&h.core;require(h.core.setLocalOperationSink(&sink),"sink register");
 const auto objects=q.snapshotObjects();const auto hist=Access::history(q);const auto gen=q.semanticGeneration();const auto fence=q.canonicalCommitFence();
 require(h.pointer(input::PointerPhase::kDown,90,45)&&h.pointer(input::PointerPhase::kMove,90,83),"publication denial gesture");require(!h.pointer(input::PointerPhase::kUp,90,83),"publication denial accepted");
 require(q.snapshotObjects()==objects&&q.semanticGeneration()==gen&&q.canonicalCommitFence()==fence&&Access::history(q)==hist&&sink.published==0,"publication rejected after mutation");
 sink.allow=true;h.shortErase(90);require(sink.published==1&&sink.reentryRejected&&Access::history(q)==hist+1,"publication/history exactly once");
 // Undo and Redo are actual root control commands before any exact frame.
 runtime::CanvasControlPayload c{};c.kind=runtime::CanvasControlPayloadKind::kUndo;h.control(c);require(q.snapshotObjects()==objects,"Undo before present failed");
 c.kind=runtime::CanvasControlPayloadKind::kRedo;h.control(c);require(q.snapshotObjects()!=objects&&q.semanticGeneration().value()==gen.value()+3,"Redo before present failed");
 h.qualifyAll();require(h.core.setLocalOperationSink(nullptr),"sink detach");
 Access::fill(q);const auto stable=q.snapshotObjects();require(!h.pointer(input::PointerPhase::kDown,200,40),"metadata capacity admission allowed");require(q.snapshotObjects()==stable,"capacity mutated document");Access::drainFaultSlots(q);
 require(h.pointer(input::PointerPhase::kDown,20,10)&&h.pointer(input::PointerPhase::kCancel,20,10),"capacity release pointer stranded");
 {
  Host faultHost;auto& runtime=faultHost.core.qualificationRuntime();Publication publication;publication.core=&faultHost.core;publication.allow=true;publication.allocationFault=true;
  require(faultHost.core.setLocalOperationSink(&publication),"allocation-fault sink");const auto beforeHistory=Access::history(runtime);const auto beforeFaults=allocationFaults.load();
  faultHost.shortErase(90);require(!failNextAllocation&&allocationFaults==beforeFaults+1,"labelled allocation fault not consumed");
  require(runtime.semanticGeneration().value()==2&&Access::history(runtime)==beforeHistory+1&&publication.published==1,"post-commit allocation lost acceptance/history");
  require(faultHost.core.setLocalOperationSink(nullptr),"allocation fault sink detach");faultHost.qualifyAll();
  require(runtime.semanticGeneration().value()==2,"derived recovery reapplied mutation");
 }
}
void lifecycle(){
 Host h;h.eraseA();const auto a=h.submitExact();require(h.core.loseSurface(),"surface loss");
 require(!h.core.receivePlatformPresented({a,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"lost surface qualified");
 require(h.core.rebindSurface(),"surface restore");h.qualifyAll();
 const auto before=h.core.readback().semanticGeneration;
 require(h.pointer(input::PointerPhase::kDown,40,10)&&h.pointer(input::PointerPhase::kCancel,40,10),"cancel lifecycle");
 require(h.core.readback().semanticGeneration==before,"cancel changed document");h.qualifyAll();
}
void navigationSafety(){
 Host h;h.eraseA();const auto a=h.submitExact();auto bad=a;++bad.attachmentEpoch;
 require(!h.core.receivePlatformPresented({bad,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"foreign attachment accepted");
 interaction::ViewportNavigationSample n{};n.deltaX=7;require(h.core.applyViewportNavigation(n),"camera input");
 runtime::RuntimeFrameOpportunity f;bool wrote=false;
 for(unsigned i=0;i<64;++i){require(h.core.serviceFrameOpportunity(false,f),"navigation opportunity");wrote|=f.navigationPresented;if(wrote)break;}
 require(wrote,"camera did not write current content");
 require(!f.canonicalWork,"approximate navigation issued canonical receipt");
 std::this_thread::sleep_for(std::chrono::milliseconds(105));h.qualifyAll();
}
void brushVisibility(){
 Host h(false);auto& q=h.core.qualificationRuntime();
 interaction::PreviewPresentationCapture capture;require(q.capturePreviewPresentation(capture),"real Brush capture");std::uint64_t presents=0;
 require(q.renderPreviewPresentation(capture,&presents)&&q.acknowledgePreviewPresentation(capture,presents),"real Brush display");
 require(h.overlay->overlayVisible()&&q.pendingCanonicalHandoffCount()==1,"retained Brush missing");
 h.canonical->resetCounts();h.overlay->resetCounts();require(h.pointer(input::PointerPhase::kDown,90,45),"Brush->erase Down");
 require(h.overlay->overlayVisible()&&!h.overlay->visibilityCalls&&!h.canonical->acquisitions&&!h.overlay->acquisitions,"Down hid or cleared Brush");
 require(h.pointer(input::PointerPhase::kMove,90,83),"Brush->erase move");
 runtime::RuntimeFrameOpportunity f;for(unsigned i=0;i<8192&&Access::covered(q)==0;++i)require(h.core.serviceFrameOpportunity(false,f),"Brush transfer opportunity");
 require(Access::covered(q)==1&&q.pendingCanonicalHandoffCount()==1,"full transient lost logical Brush identity");
 // A capture from before the accepted replacement cannot redraw the old ink.
 require(!q.renderPreviewPresentation(capture,&presents),"stale capture resurrected covered Brush");
 require(h.pointer(input::PointerPhase::kUp,90,83),"Brush->erase Up");h.qualifyAll();
 require(!q.previewActive()&&!h.overlay->overlayVisible()&&!q.pendingCanonicalHandoffCount()&&Access::records(q)==0,"replacement leaked overlay/handoff");
}

}
int main(int argc,char** argv){
    try{
        const std::string_view mode=argc>1?argv[1]:"";
        if(mode=="acquire")readmission(true);else if(mode=="present")readmission(false);else if(mode=="navigation")navigation();else if(mode=="handoff")handoff();else if(mode=="scene")sceneRecovery();else if(mode=="history")history();else if(mode=="lifecycle")lifecycle();else if(mode=="navigation-safety")navigationSafety();else if(mode=="brush")brushVisibility();else require(false,"unknown test mode");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"UR2A_TEST_FAILURE %s\n",e.what());return std::string_view(e.what()).starts_with("BEHAVIOR:")?1:2;}
}

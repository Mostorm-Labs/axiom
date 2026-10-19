#include "minimal_runtime_facade.hpp"
#include "web_surface_host.hpp"
#include "canvas/debug_ui/control_router.hpp"
#include "canvas/debug_ui/snapshot_assembler.hpp"
#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
#include <thread>
#include <functional>
#include <type_traits>
#include <string>
using namespace canvas;
template<class T> concept HasRuntime=requires(T& t){t.runtime();};
template<class T> concept HasProcess=requires(T& t){t.process(std::uint64_t{1});};
template<class T> concept HasCapture=requires(T& t,interaction::PreviewPresentationCapture& c){t.capturePreviewPresentation(c);};
template<class T> concept HasRender=requires(T& t,interaction::PreviewPresentationCapture& c,std::uint64_t* n){t.renderPreviewPresentation(c,n);};
template<class T> concept HasAck=requires(T& t,interaction::PreviewPresentationCapture& c){t.acknowledgePreviewPresentation(c,std::uint64_t{1});};
static_assert(!HasRuntime<runtime::RuntimeComposition>);
static_assert(!HasCapture<runtime::RuntimeComposition>&&!HasRender<runtime::RuntimeComposition>&&!HasAck<runtime::RuntimeComposition>);
static_assert(!HasProcess<runtime::RuntimeFacade>);
static_assert(std::is_same_v<decltype(std::declval<runtime::RuntimeComposition&>().controlFacade()),runtime::RuntimeFacade&>);
input::PlatformPointerBatch pointer(unsigned seq,input::PointerPhase phase,float x,float y){input::PlatformPointerBatch b;b.samples.push_back({1,1,seq,seq*1000000ULL,x,y,.5F,0,0,{},{},input::SampleProvenance::kConfirmedCurrent,phase});return b;}
void publicEditingAndFences() {
  minimal_runtime_host::MinimalRuntimeHost host;minimal_runtime_host::MinimalRuntimeFacade facade(host);auto& c=host.composition();
  assert(c.bindSurface(256,256)&&c.registerSource(1));for(unsigned i=0;i<300;++i)assert(c.ownerPump());assert(facade.process()==0);assert(facade.controlSequence()==301);
  debug_ui::DebugSnapshotAssembler assembler;debug_ui::DebugSnapshotSources src{};src.runtime=&facade;
  debug_ui::DebugControlRouter router(&facade,nullptr,nullptr,nullptr);router.beginFrame(assembler.capture(src));
  const auto r=router.setCreationPreset(runtime::CanvasCreationPreset::kRectangle);assert(r.control);assert(c.ownerPump());
  assert(facade.canvasControlReceipt(r.control->key)->state==runtime::CanvasControlReceiptState::kApplied);
  assert(c.submit(pointer(1,input::PointerPhase::kDown,20,20)));assert(c.submit(pointer(2,input::PointerPhase::kMove,120,100)));assert(c.submit(pointer(3,input::PointerPhase::kUp,120,100)));
  assert(facade.setSelectionMode(true,77,facade.readRuntimeState().identity.runtimeGeneration,0xB001).state==runtime::ProductControlState::kQueued);
  assert(c.submit(pointer(4,input::PointerPhase::kDown,70,60)));assert(c.submit(pointer(5,input::PointerPhase::kUp,70,60)));
  const auto state=facade.readRuntimeState();assert(state.selection.selectedObjectCount==1);assert(state.control.selectedKind==1);
  bool resized=true,registered=true,lost=true,closed=true;std::thread t([&]{resized=c.resizeSurface(300,200);registered=c.registerSource(77);lost=c.loseSurface();closed=c.close();});t.join();
  assert(!resized&&!registered&&!lost&&!closed&&c.state()==runtime::RuntimeCompositionState::kReady);
  std::puts("PASS public-only: 300 pumps -> real Rectangle create/select/readback; wrong-thread resize/register/lost/close rejected");
}
void diagnostics() {
  minimal_runtime_host::MinimalRuntimeHost host;minimal_runtime_host::MinimalRuntimeFacade facade(host);assert(host.composition().bindSurface(256,256));
  for(unsigned i=0;i<20;++i)assert(host.composition().ownerPump());assert(facade.readPlatformDiagnostics().presentCount==0);
  std::puts("PASS diagnostic: 20 owner pumps -> 0 provider presents");
}
semantic::ResourceManifest manifest() {
  semantic::ResourceManifest m;m.manifest_revision=1;semantic::ResourceManifestEntry e;e.resource_id=semantic::ResourceId{foundation::ObjectId::fromUint64(7)};e.resource_revision=1;
  e.content_hash="sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a";e.byte_length=1;e.decode_version=1;e.metadata={8,1,16,1,24,1,32,1};m.entries.push_back(e);return m;
}
void idleResource() {
  runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256)&&c.setPlatformPresentationDeferred(true)&&c.bindManifest(manifest(),1));runtime::RuntimeFrameOpportunity out;
  assert(c.serviceFrameOpportunity(false,out)&&out.canonicalFrame);
  assert(c.receivePlatformPresented({*out.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,std::nullopt}));
  assert(c.publishResource(foundation::ObjectId::fromUint64(7),{1}));assert(c.serviceFrameOpportunity(false,out));assert(out.canonicalRequested&&out.needsPaint&&c.readback().pendingNotifications==0);
  std::puts("PASS same-opportunity: new resource demand is rendered without another timer");
}
render::SurfaceSnapshot initial(){return {render::ViewId{301},render::SurfaceGeneration{1},render::MetricsGeneration{1},{256,256,256,256,1,1}};}
render::FrameState platformFrame(const render::SurfaceSnapshot& s,unsigned id) {
  return {.viewId=s.viewId,.camera={foundation::WorldPoint{0,0},1,0,render::CameraGeneration{1}},.worldViewport={-128,-128,128,128},.metrics=s.metrics,.sceneGeneration=semantic::SemanticGeneration{17},.sceneReadToken=SceneRevision{17},.surfaceGeneration=s.surfaceGeneration,.metricsGeneration=s.metricsGeneration,.frameId=render::FrameId{id}};
}
render::PresentedFeedback feedback(const render::FrameState& f){return {f.viewId,f.frameId,f.surfaceGeneration,f.metricsGeneration,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,std::nullopt};}
render::PresentFeedbackDisposition mappedFeedback(web_demo::WebSurfaceHost& host,
    const render::PresentedFeedback& value) {
  const auto token=host.submittedCanonicalWork(value.frameId);
  return host.feedback(value,token.value_or(runtime::CanonicalWork{}));
}
void feedbackMapping() {
  web_demo::WebSurfaceHost host(initial());const auto surface=host.lifecycle().current();const auto a=platformFrame(surface,101),b=platformFrame(surface,102);
  assert(host.submit(a)==render::PresentFeedbackDisposition::kSubmitted);const auto runtimeA=host.composition().lastSubmittedCanonicalFrame();assert(runtimeA);
  assert(host.submit(b)==render::PresentFeedbackDisposition::kSubmitted);const auto runtimeB=host.composition().lastSubmittedCanonicalFrame();assert(runtimeB&&runtimeA->frameId!=runtimeB->frameId);
  const auto result=mappedFeedback(host,feedback(a));const bool cleared=!host.composition().lastSubmittedCanonicalFrame();
  std::printf("PASS mapping: platformA=101/runtimeA=%llu platformB=102/runtimeB=%llu lateA_result=%u B_cleared=%d\n",runtimeA->frameId.value(),runtimeB->frameId.value(),unsigned(result),cleared);
  assert(result==render::PresentFeedbackDisposition::kStaleGeneration&&!cleared);
  assert(host.composition().lastSubmittedCanonicalFrame()==runtimeB);
  assert(mappedFeedback(host,feedback(b))==render::PresentFeedbackDisposition::kPresented);
  assert(!host.composition().lastSubmittedCanonicalFrame());
  assert(mappedFeedback(host,feedback(b))==render::PresentFeedbackDisposition::kUnknownFrame);
  web_demo::WebSurfaceHost reverse(initial());const auto rs=reverse.lifecycle().current();const auto ra=platformFrame(rs,201),rb=platformFrame(rs,202);
  assert(reverse.submit(ra)==render::PresentFeedbackDisposition::kSubmitted&&reverse.submit(rb)==render::PresentFeedbackDisposition::kSubmitted);
  assert(mappedFeedback(reverse,feedback(rb))==render::PresentFeedbackDisposition::kPresented);
  assert(mappedFeedback(reverse,feedback(ra))==render::PresentFeedbackDisposition::kStaleGeneration);
  assert(mappedFeedback(reverse,feedback(platformFrame(rs,999)))==render::PresentFeedbackDisposition::kUnknownFrame);
  std::puts("PASS mapping B-first, duplicate, unknown and late-A cannot acknowledge B");
}
void recoveryAndTarget() {
  web_demo::WebSurfaceHost host(initial());auto& c=host.composition();auto& facade=c.controlFacade();const auto oldTarget=facade.mountedCanvasTargets().front();
  runtime::CanvasControlRequest request{};request.clientId=0xD001;request.requestId=1;request.target=oldTarget;request.payload.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;request.payload.creationPreset=runtime::CanvasCreationPreset::kEllipse;
  const auto r=facade.submitCanvasControl(request);assert(r.control&&r.state==runtime::ProductControlState::kQueued);assert(host.markLost()==render::SurfaceLifecycleDisposition::kLost);
  auto p=std::make_unique<render::RasterSkiaSurfaceProvider>();assert(p->resize(256,256).code==render::BackendSubmissionCode::kAccepted);assert(c.registerSurfaceProvider("restored-canonical",std::move(p)));
  auto q=std::make_unique<render::RasterSkiaSurfaceProvider>();assert(q->resize(256,256).code==render::BackendSubmissionCode::kAccepted);assert(c.registerPreviewSurfaceProvider("restored-preview",std::move(q)));
  const auto prepared=c.surfaceSnapshot();assert(host.bind({prepared.viewId,render::SurfaceGeneration{prepared.surfaceGeneration},render::MetricsGeneration{prepared.metricsGeneration},{256,256,256,256,1,1}})==web_demo::WebSurfaceDisposition::kRebound);
  assert(c.state()==runtime::RuntimeCompositionState::kReady);assert(c.ownerPump());const auto now=facade.mountedCanvasTargets().front();const auto t=facade.canvasControlReceipt(r.control->key);assert(t);
  std::printf("PASS Lost replacement/restore target fencing: surface=%llu oldAttachment=%llu newAttachment=%llu oldRequestState=%u\n",c.surfaceSnapshot().surfaceGeneration,oldTarget.attachmentEpoch,now.attachmentEpoch,unsigned(t->state));
  assert(oldTarget!=now&&(t->state==runtime::CanvasControlReceiptState::kTargetDestroyed||t->state==runtime::CanvasControlReceiptState::kStaleTarget));
  request.target=now;request.requestId=2;const auto fresh=facade.submitCanvasControl(request);assert(fresh.control);assert(c.ownerPump());
  assert(facade.canvasControlReceipt(fresh.control->key)->state==runtime::CanvasControlReceiptState::kApplied);
}
void previewPending() {
  for(const bool resize:{false,true}) {
    runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256)&&c.registerSource(1)&&c.setPlatformPresentationDeferred(true));
    assert(c.submit(pointer(1,input::PointerPhase::kDown,20,20))&&c.submit(pointer(2,input::PointerPhase::kMove,80,80)));
    runtime::RuntimeFrameOpportunity a,b;assert(c.serviceFrameOpportunity(false,a)&&a.previewActive&&a.previewRendered);
    std::vector<std::uint8_t> before(256*256*4);assert(c.readPreviewRgba(before));
    if(resize)assert(c.resizeSurface(320,240));else assert(c.submit(pointer(3,input::PointerPhase::kMove,180,180)));
    assert(!c.finishFrameOpportunity({a.previewWork,true}));
    assert(c.serviceFrameOpportunity(false,b)&&b.previewRendered);
    assert(c.finishFrameOpportunity({b.previewWork,true})&&!c.previewRenderState().dirty);
    std::vector<std::uint8_t> after(resize?320*240*4:256*256*4);assert(c.readPreviewRgba(after));
    assert(before!=after);
    if(resize)assert(a.previewSurfaceGeneration!=b.previewSurfaceGeneration);else assert(b.previewRevision>a.previewRevision);
    std::printf("PASS obsolete preview recovery: resize=%d oldRevision=%llu newRevision=%llu oldGeneration=%llu newGeneration=%llu dirtyAfterAck=%d changedPixels=1\n",resize,a.previewRevision,b.previewRevision,a.previewSurfaceGeneration,b.previewSurfaceGeneration,c.previewRenderState().dirty);
  }
}
void previewRetryAndRestore() {
  runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256)&&c.registerSource(1)&&c.setPlatformPresentationDeferred(true));
  assert(c.submit(pointer(1,input::PointerPhase::kDown,20,20))&&c.submit(pointer(2,input::PointerPhase::kMove,80,80)));
  runtime::RuntimeFrameOpportunity a,b;assert(c.serviceFrameOpportunity(false,a)&&a.previewRendered);
  assert(!c.finishFrameOpportunity({a.previewWork,false}));assert(c.serviceFrameOpportunity(false,b)&&b.previewRevision==a.previewRevision);
  assert(c.finishFrameOpportunity({b.previewWork,true})&&!c.previewRenderState().dirty);
  const auto n=c.previewRenderState().submissionCount;assert(!c.finishFrameOpportunity({b.previewWork,true})&&c.previewRenderState().submissionCount==n);
  assert(c.submit(pointer(3,input::PointerPhase::kMove,110,110))&&c.serviceFrameOpportunity(false,a));
  assert(c.loseSurface()&&c.rebindSurface());assert(!c.finishFrameOpportunity({a.previewWork,true}));
  assert(c.serviceFrameOpportunity(false,b)&&b.previewRendered&&a.previewSurfaceGeneration!=b.previewSurfaceGeneration);
  assert(c.finishFrameOpportunity({b.previewWork,true})&&!c.previewRenderState().dirty);
  std::puts("PASS plain presentation failure retry, duplicate no-op, and restored-generation recovery");
}
void completionIdentity() {
  for(const bool resize:{false,true}) {
    runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256)&&c.registerSource(1)&&c.setPlatformPresentationDeferred(true));
    assert(c.submit(pointer(1,input::PointerPhase::kDown,20,20))&&c.submit(pointer(2,input::PointerPhase::kMove,80,80)));
    runtime::RuntimeFrameOpportunity a,b;assert(c.serviceFrameOpportunity(false,a)&&a.previewRendered);
    if(resize)assert(c.resizeSurface(320,240));else assert(c.submit(pointer(3,input::PointerPhase::kMove,180,180)));
    // B is rendered before delayed platform completion for A arrives.
    assert(c.serviceFrameOpportunity(false,b)&&b.previewRendered);
    assert(c.previewRenderState().dirty);
    const auto before=c.previewRenderState().platformQualifiedRevision;
    const bool acceptedOldA=c.finishFrameOpportunity({a.previewWork,true});
    const auto after=c.previewRenderState().platformQualifiedRevision;
    std::printf("PASS delayed A completion isolation: resize=%d A=(%llu,%llu) B=(%llu,%llu) qualifiedBefore=%llu acceptedA=%d qualifiedAfter=%llu dirtyB=%d\n",resize,a.previewRevision,a.previewSurfaceGeneration,b.previewRevision,b.previewSurfaceGeneration,before,acceptedOldA,after,c.previewRenderState().dirty);
    assert(!acceptedOldA&&after==before&&c.previewRenderState().dirty);
    assert(c.finishFrameOpportunity({b.previewWork,true})&&!c.previewRenderState().dirty);
  }
}
void crossInstanceCompletion() {
  runtime::RuntimeComposition a(render::ViewId{11}),b(render::ViewId{22});
  for(auto* c:{&a,&b}) {
    assert(c->open()&&c->bindSurface(256,256)&&c->registerSource(1)&&c->setPlatformPresentationDeferred(true));
    assert(c->submit(pointer(1,input::PointerPhase::kDown,20,20))&&c->submit(pointer(2,input::PointerPhase::kMove,80,80)));
  }
  runtime::RuntimeFrameOpportunity wa,wb;assert(a.serviceFrameOpportunity(false,wa)&&b.serviceFrameOpportunity(false,wb)&&wa.previewWork&&wb.previewWork);
  const auto before=b.previewRenderState().platformQualifiedRevision;
  const bool accepted=b.finishFrameOpportunity({wa.previewWork,true});
  const auto after=b.previewRenderState().platformQualifiedRevision;
  std::printf("OBSERVED cross-instance completion: viewA=%llu viewB=%llu workA=%llu workB=%llu equalToken=%d acceptedAinB=%d qualifiedBefore=%llu qualifiedAfter=%llu dirtyB=%d\n",a.surfaceSnapshot().viewId.value(),b.surfaceSnapshot().viewId.value(),wa.previewWork->workId,wb.previewWork->workId,wa.previewWork==wb.previewWork,accepted,before,after,b.previewRenderState().dirty);
  assert(wa.previewWork!=wb.previewWork&&!accepted&&after==before&&b.previewRenderState().dirty); assert(b.finishFrameOpportunity({wb.previewWork,true}));
}
void mappingCapacity() {
  web_demo::WebSurfaceHost host(initial());const auto s=host.lifecycle().current();
  for(unsigned i=1;i<=8;++i)assert(host.submit(platformFrame(s,i))==render::PresentFeedbackDisposition::kSubmitted);
  const auto before=host.composition().lastSubmittedCanonicalFrame();assert(before);
  assert(host.submit(platformFrame(s,9))==render::PresentFeedbackDisposition::kUnavailable);
  const auto after=host.composition().lastSubmittedCanonicalFrame();assert(before==after);
  assert(mappedFeedback(host,feedback(platformFrame(s,9)))==render::PresentFeedbackDisposition::kUnknownFrame);
  assert(host.composition().lastSubmittedCanonicalFrame()==before);
  assert(mappedFeedback(host,feedback(platformFrame(s,8)))==render::PresentFeedbackDisposition::kPresented);
  assert(!host.composition().lastSubmittedCanonicalFrame());
  assert(mappedFeedback(host,feedback(platformFrame(s,8)))==render::PresentFeedbackDisposition::kUnknownFrame);
  for(unsigned i=1;i<8;++i)assert(mappedFeedback(host,feedback(platformFrame(s,i)))==render::PresentFeedbackDisposition::kStaleGeneration);
  assert(host.submit(platformFrame(s,9))==render::PresentFeedbackDisposition::kSubmitted);
  assert(mappedFeedback(host,feedback(platformFrame(s,9)))==render::PresentFeedbackDisposition::kPresented);
  std::printf("PASS mapping full: rejected9 preserves runtime=%llu; accepted8 completes; rejected feedback isolated; submit9 works after cleanup\n",before->frameId.value());
}
void restoreVariants() {
  for(const bool metrics:{false,true}) {
    runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256));auto& f=c.controlFacade();const auto old=f.mountedCanvasTargets().front();
    runtime::CanvasControlRequest r{};r.clientId=0xD002;r.requestId=1;r.target=old;r.payload.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;r.payload.creationPreset=runtime::CanvasCreationPreset::kEllipse;
    const auto q=f.submitCanvasControl(r);assert(q.control&&c.loseSurface());
    assert(metrics?c.rebindSurface(render::SurfaceMetrics{256,256,256,256,1,1}):c.rebindSurface());
    const auto now=f.mountedCanvasTargets().front();assert(now!=old&&c.ownerPump());
    const auto receipt=f.canvasControlReceipt(q.control->key);assert(receipt&&(receipt->state==runtime::CanvasControlReceiptState::kTargetDestroyed||receipt->state==runtime::CanvasControlReceiptState::kStaleTarget));
    r.target=now;r.requestId=2;const auto fresh=f.submitCanvasControl(r);assert(fresh.control&&c.ownerPump()&&f.canvasControlReceipt(fresh.control->key)->state==runtime::CanvasControlReceiptState::kApplied);
    std::printf("PASS restore target: metrics=%d old=%llu new=%llu oldReceipt=%u freshApplied=1\n",metrics,old.attachmentEpoch,now.attachmentEpoch,unsigned(receipt->state));
  }
}
void autoFrames() {
  for(const bool deferred:{false,true}) {
    runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256)&&c.setPlatformPresentationDeferred(deferred));auto& facade=c.controlFacade();const auto g=facade.readRuntimeState().identity.runtimeGeneration;
    assert(facade.zoomAt(20,20,2,1,g).state==runtime::ProductControlState::kQueued);runtime::RuntimeFrameOpportunity out;assert(c.serviceFrameOpportunity(true,out));
    std::printf("frame opportunity: deferred=%d canonicalContentSerial=%llu returnedFrame=%llu\n",deferred,c.canonicalContentSerial(),out.canonicalFrame->frameId.value());
    assert(c.canonicalContentSerial()==(deferred?1:2));
  }
}
void uiMode() {
  ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={900,640};io.DeltaTime=1.f/60;
  unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256));debug_ui::DebugSnapshotAssembler assembler;debug_ui::DebugSnapshotSources src{};src.runtime=&c.controlFacade();
  debug_ui::DebugControlRouter router(&c.controlFacade(),nullptr,nullptr,nullptr);router.beginFrame(assembler.capture(src));
  assert(router.setCreationPreset(runtime::CanvasCreationPreset::kEllipse).state==runtime::ProductControlState::kQueued);assert(c.ownerPump());
  debug_ui::PanelRegistry registry;debug_ui::registerControlDebugPanels(registry);debug_ui::DebugUiSessionState ui;
  auto text=[&] {
    const auto snapshot=assembler.capture(src);debug_ui::DebugPanelContext ctx{snapshot,router,ui};ImGui::NewFrame();ImGui::SetNextWindowSize({900,640});ImGui::Begin("actual-tools");ImGui::LogToBuffer();
    for(const auto& p:registry.contributions(debug_ui::WorkspaceId::kControl))if(p.slot==debug_ui::PanelSlot::kTools)p.render(ctx);
    std::string s=ImGui::GetCurrentContext()->LogBuffer.c_str();ImGui::LogFinish();ImGui::End();ImGui::Render();return s;
  };
  assert(text().find("Creation mode: Ellipse")!=std::string::npos);router.beginFrame(assembler.capture(src));assert(router.setTool(runtime::CanvasToolKind::kInk).state==runtime::ProductControlState::kQueued);assert(c.ownerPump());
  assert(text().find("Creation mode: disabled (last preset: Ellipse)")!=std::string::npos);std::puts("PASS actual Tools panel text: active Ellipse -> disabled (last preset: Ellipse)");ImGui::DestroyContext();
}

class HookProvider final : public render::SkiaSurfaceProvider {
 public:
  render::RasterSkiaSurfaceProvider actual;
  std::function<void()> onResize;
  bool rejectActivation = false;
  render::RenderTargetInfo describe() const noexcept override { return actual.describe(); }
  render::SkiaSurfaceAcquireResult acquire() noexcept override { return actual.acquire(); }
  void release() noexcept override { actual.release(); }
  render::BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h) noexcept override {
    if(onResize) { auto f=std::move(onResize); f(); } return actual.resize(w,h);
  }
  render::BackendSubmissionResult lose() noexcept override { return actual.lose(); }
  render::BackendSubmissionResult present() noexcept override { return actual.present(); }
  render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> d) noexcept override { return actual.readbackRgba(d); }
  render::BackendSubmissionResult advanceGeneration() noexcept override { return actual.advanceGeneration(); }
  render::BackendSubmissionResult activateBinding() noexcept override {
    return rejectActivation ? render::BackendSubmissionResult::rejected("activation fault") : render::BackendSubmissionResult::accepted();
  }
  std::uint64_t generation() const noexcept override { return actual.generation(); }
  std::uint64_t readbackCount() const noexcept override { return actual.readbackCount(); }
  std::uint64_t cpuCopyCount() const noexcept override { return actual.cpuCopyCount(); }
  std::uint64_t presentCount() const noexcept override { return actual.presentCount(); }
};
void acceptedMetrics() {
  web_demo::WebSurfaceHost h({render::ViewId{301},render::SurfaceGeneration{1},render::MetricsGeneration{1},{100,80,200,160,2,1}});
  auto r=h.composition().surfaceSnapshot();auto p=h.lifecycle().current();
  assert(r.logicalWidth==p.metrics.logicalWidth&&r.devicePixelRatio==p.metrics.devicePixelRatio);
  assert(h.bindMetrics({100,80,200,160,2,1.25F})==web_demo::WebSurfaceDisposition::kRebound);
  const auto m=h.composition().surfaceSnapshot();
  assert(m.surfaceGeneration==r.surfaceGeneration&&m.metricsGeneration==r.metricsGeneration+1);
  assert(h.bindMetrics({100,80,200,160,2,1.25F})==web_demo::WebSurfaceDisposition::kRebound);
  assert(m.metricsGeneration==h.composition().surfaceSnapshot().metricsGeneration);
  std::puts("PASS accepted full metrics: logical100x80/physical200x160/DPR2; metrics-only and idempotence");
}
void activationFailure() {
  runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(128,128));
  auto p=std::make_unique<HookProvider>();assert(p->resize(128,128).code==render::BackendSubmissionCode::kAccepted);
  p->rejectActivation=true;
  assert(c.registerPreviewSurfaceProvider("activation-fault",std::move(p)));
  assert(!c.resizeSurface(128,128)&&c.state()==runtime::RuntimeCompositionState::kLost);
  assert(!c.surfaceSnapshot().available&&!c.canonicalProviderAvailable()&&!c.previewProviderAvailable());
  assert(c.rebindSurface()&&c.state()==runtime::RuntimeCompositionState::kReady);
  runtime::RuntimeFrameOpportunity out;assert(c.serviceFrameOpportunity(true,out)&&out.canonicalFrame);
  std::puts("PASS second-role activation failure -> uniform Lost -> successful recovery");
}
int remainingOracleFailures = 0;
void canonicalCrossInstanceObservation() {
  runtime::RuntimeComposition a(render::ViewId{1}),b(render::ViewId{1});
  for(auto* c:{&a,&b}) {
    assert(c->open()&&c->bindSurface(128,128)&&c->registerSource(1)&&c->setPlatformPresentationDeferred(true));
    assert(c->submit(pointer(1,input::PointerPhase::kDown,20,20)));
    assert(c->submit(pointer(2,input::PointerPhase::kMove,80,80)));
    assert(c->submit(pointer(3,input::PointerPhase::kUp,80,80)));
  }
  runtime::RuntimeFrameOpportunity fa,fb;assert(a.serviceFrameOpportunity(false,fa)&&b.serviceFrameOpportunity(false,fb));
  assert(fa.canonicalFrame&&fb.canonicalFrame&&fb.previewWork&&fb.pendingCanonicalHandoff==1);
  assert(b.finishFrameOpportunity({fb.previewWork,true}));
  const bool previewBefore=b.previewActive();
  const bool accepted=b.receivePlatformPresented({*fa.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,std::nullopt});
  std::printf("OBSERVED canonical cross-instance: equalIdentity=%d acceptedAinB=%d BsubmittedAfter=%d previewBefore=%d previewAfter=%d pendingBefore=%llu\n",fa.canonicalFrame==fb.canonicalFrame,accepted,b.lastSubmittedCanonicalFrame().has_value(),previewBefore,b.previewActive(),fb.pendingCanonicalHandoff);
  if(accepted || !b.previewActive()) ++remainingOracleFailures;
}
void reentrantCloseThenResizeObservation() {
  runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(128,128));
  auto p=std::make_unique<HookProvider>();auto* raw=p.get();
  assert(p->resize(128,128).code==render::BackendSubmissionCode::kAccepted);
  assert(c.registerSurfaceProvider("hook",std::move(p))&&c.resizeSurface(128,128));
  raw->onResize=[&]{
    assert(c.close());
    std::printf("OBSERVED reentrant after close state=%u\n",unsigned(c.state()));
    const bool resized=c.resizeSurface(500,500);
    std::printf("OBSERVED reentrant resize after close accepted=%d state=%u\n",resized,unsigned(c.state()));
  };
  const bool result=c.resizeSurface(256,256);
  std::printf("OBSERVED outer after close+resize: result=%d state=%u closedEnum=%u available=%d\n",result,unsigned(c.state()),unsigned(runtime::RuntimeCompositionState::kClosed),c.surfaceSnapshot().available);
  if(c.state()!=runtime::RuntimeCompositionState::kClosed) ++remainingOracleFailures;
}


int main(){std::setvbuf(stdout,nullptr,_IONBF,0);publicEditingAndFences();diagnostics();idleResource();feedbackMapping();recoveryAndTarget();autoFrames();uiMode();restoreVariants();previewPending();mappingCapacity();previewRetryAndRestore();completionIdentity();crossInstanceCompletion();acceptedMetrics();activationFailure();canonicalCrossInstanceObservation();reentrantCloseThenResizeObservation();std::printf("independent residual oracle failures=%d\n",remainingOracleFailures);return remainingOracleFailures;}

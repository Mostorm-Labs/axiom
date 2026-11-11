// Executes the actual Web adapter poll/prepare policy. Only WebGL context/fence
// calls are deterministic stubs; Composition, editing and Skia rendering are real.
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include "canvas/runtime/internal/runtime_platform_access.hpp"
#include "canvas/render/webgl_surface_backend.hpp"
#include <cassert>
#include <iostream>
#include <string_view>
#include <functional>
#include <array>
#include <thread>
unsigned ur3FenceResult=2;
bool ur3ContextLost=false;
namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {public:
 static void retainPreviewForFailure(interaction::CanvasEditingRuntime&r){r.submittedDisplayRoster_.bound=false;}
};
}
namespace canvas::render {
struct WebGlSurfaceProvider::Impl {RasterSkiaSurfaceProvider raster;WebGlSurfaceConfig config;bool visible=false;};
WebGlSurfaceProvider::WebGlSurfaceProvider(WebGlSurfaceConfig config):impl_(std::make_shared<Impl>()) {impl_->config=std::move(config);(void)impl_->raster.resize(256,256);}
WebGlSurfaceProvider::~WebGlSurfaceProvider()=default;
std::shared_ptr<GpuCompletionService> WebGlSurfaceProvider::gpuCompletionService()const noexcept{return {};}
bool WebGlSurfaceProvider::ready()const noexcept{return true;}
const std::string& WebGlSurfaceProvider::error()const noexcept{static const std::string empty;return empty;}
RenderTargetInfo WebGlSurfaceProvider::describe()const noexcept{return impl_->raster.describe();}
SkiaSurfaceAcquireResult WebGlSurfaceProvider::acquire()noexcept{return impl_->raster.acquire();}
void WebGlSurfaceProvider::release()noexcept{impl_->raster.release();}
BackendSubmissionResult WebGlSurfaceProvider::lose()noexcept{return impl_->raster.lose();}
BackendSubmissionResult WebGlSurfaceProvider::present()noexcept{return impl_->raster.present();}
BackendSubmissionResult WebGlSurfaceProvider::resize(std::uint32_t w,std::uint32_t h)noexcept{return impl_->raster.resize(w,h);}
BackendSubmissionResult WebGlSurfaceProvider::readbackRgba(std::span<std::uint8_t>o)noexcept{return impl_->raster.readbackRgba(o);}
std::uint64_t WebGlSurfaceProvider::presentCount()const noexcept{return impl_->raster.presentCount();}
std::uint64_t WebGlSurfaceProvider::generation()const noexcept{return impl_->raster.generation();}
BackendSubmissionResult WebGlSurfaceProvider::advanceGeneration()noexcept{return impl_->raster.advanceGeneration();}
void WebGlSurfaceProvider::setOverlayVisible(bool v)noexcept{impl_->visible=v;}
bool WebGlSurfaceProvider::overlayVisible()const noexcept{return impl_->visible;}
}
#include "../../../sdk/web/web_runtime_adapter.cpp"
using namespace canvas;
struct Fixture {
 runtime::RuntimeComposition owner;
 runtime::RuntimeCompositionCore& core=runtime::RuntimePlatformAccess::core(owner);
 platform::WebRuntimeAdapter adapter{owner};
 std::uint64_t seq=0,req=0;
 Fixture(){ur3FenceResult=GL_ALREADY_SIGNALED;ur3ContextLost=false;assert(owner.open());assert(adapter.bind(1,2,256,256,256,256,1));assert(core.registerSource(71));}
 void point(input::PointerPhase phase,float x=30,float y=40){input::PlatformPointerSample s{};s.source=71;s.pointer=1;s.sequence=++seq;s.timestampNs=seq*8000000;s.x=x;s.y=y;s.pressure=.5;s.phase=phase;assert(core.submit({{s}}));}
 void stroke(){point(input::PointerPhase::kDown);point(input::PointerPhase::kMove,80,40);}
 void control(runtime::CanvasControlPayload p){runtime::CanvasControlRequest q{};q.clientId=72;q.requestId=++req;q.target=core.controlFacade().mountedCanvasTargets().front();q.payload=std::move(p);assert(core.controlFacade().submitCanvasControl(q).state==runtime::ProductControlState::kQueued);assert(core.ownerPump());}
 void drain(){for(unsigned i=0;i<1000;++i){auto r=adapter.prepareFrame(false);assert(r!=platform::WebFrameResult::kRejected);if(r==platform::WebFrameResult::kPending)assert(adapter.pollFrame()==platform::WebFrameResult::kRetry);else if(r==platform::WebFrameResult::kIdle)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}std::cerr<<"drain dirty="<<core.canonicalPresentationDirty()<<" previewActive="<<core.previewActive()<<" pDirty="<<core.previewRenderState().dirty<<" displayDirty="<<core.qualificationRuntime().previewDisplayDirty()<<" handoffs="<<core.qualificationRuntime().pendingCanonicalHandoffCount()<<" pending="<<adapter.frameStatus().pending<<" submitted="<<bool(core.lastSubmittedCanonicalWork())<<"\n";assert(false&&"adapter did not progress");}
};
void displayOnlyDelayedCallback(){
 Fixture f;f.stroke();assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);const auto old=f.adapter.frameStatus();assert(old.previewWorkId&&old.canonicalWorkId);
 const auto revision=f.core.previewRenderState().contentRevision;
 const auto display=f.core.qualificationRuntime().previewDisplayRevision();
 assert(f.core.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,-8,4,0,0,1}));
 assert(f.core.previewRenderState().contentRevision==revision);assert(f.core.qualificationRuntime().previewDisplayRevision()!=display);
 const auto result=f.adapter.pollFrame();
 std::cout<<"{\"case\":\"display-only\",\"result\":"<<int(result)<<",\"pending\":"<<f.adapter.frameStatus().pending<<"}\n";
 assert(result==platform::WebFrameResult::kRetry&&"superseded preview must release adapter slot");assert(!f.adapter.frameStatus().pending);assert(f.adapter.presentedCount()==1);
 assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);assert(f.adapter.pollFrame()==platform::WebFrameResult::kRetry);assert(f.core.previewActive());assert(!f.core.previewRenderState().dirty);assert(!f.core.qualificationRuntime().previewDisplayDirty());
}

void contentAndNewGestureCallbacks(){
 for(bool newGesture:{false,true}){
  Fixture f;f.stroke();assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);const auto previous=f.core.previewRenderState().contentRevision;
  if(newGesture){f.point(input::PointerPhase::kCancel,80,40);f.stroke();}else f.point(input::PointerPhase::kMove,90,42);
  if(!newGesture)assert(f.core.previewRenderState().contentRevision!=previous);
  assert(f.adapter.pollFrame()==platform::WebFrameResult::kRetry);assert(!f.adapter.frameStatus().pending);
  auto prepared=f.adapter.prepareFrame(false);assert(prepared==platform::WebFrameResult::kPending);assert(f.adapter.pollFrame()==platform::WebFrameResult::kRetry);
  assert(f.core.previewActive());assert(!f.core.previewRenderState().dirty);
 }
}
void strictQualification(){
 Fixture f;f.stroke();runtime::RuntimeFrameOpportunity first;assert(f.core.serviceFrameOpportunity(false,first)&&first.previewWork);
 const auto old=*first.previewWork;using State=runtime::RuntimePreviewWorkState;
 assert(f.core.previewWorkState(old)==State::kCurrent);
 using Work=runtime::RuntimeFrameOpportunity::PreviewWork;
 const std::array<std::uint64_t Work::*,9> fields={&Work::runtimeInstanceId,&Work::viewId,&Work::lifetimeGeneration,&Work::workId,&Work::contentRevision,&Work::displayRevision,&Work::attachmentEpoch,&Work::surfaceGeneration,&Work::metricsGeneration};
 for(auto field:fields){auto forged=old;++(forged.*field);assert(f.core.previewWorkState(forged)==State::kInvalid);assert(!f.core.finishFrameOpportunity({forged,true}));assert(f.core.previewWorkState(old)==State::kCurrent);}
 State wrongThread=State::kCurrent;std::thread t([&]{wrongThread=f.core.previewWorkState(old);});t.join();assert(wrongThread==State::kInvalid);
 Fixture foreign;assert(foreign.core.previewWorkState(old)==State::kInvalid);
 assert(!f.core.finishFrameOpportunity({old,false}));assert(f.core.previewWorkState(old)==State::kCurrent);
 assert(f.core.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,-8,4,0,0,1}));
 assert(f.core.previewWorkState(old)==State::kSuperseded);assert(!f.core.finishFrameOpportunity({old,true}));
 runtime::RuntimeFrameOpportunity fresh;assert(f.core.serviceFrameOpportunity(false,fresh)&&fresh.previewWork);assert(fresh.previewWork!=first.previewWork);
 assert(fresh.previewWork->contentRevision==old.contentRevision&&fresh.previewWork->displayRevision!=old.displayRevision);
 assert(f.core.previewWorkState(old)==State::kSuperseded);assert(!f.core.finishFrameOpportunity({old,true}));assert(f.core.previewWorkState(*fresh.previewWork)==State::kCurrent);
 assert(f.core.finishFrameOpportunity({fresh.previewWork,true}));assert(f.core.previewWorkState(*fresh.previewWork)==State::kInvalid);assert(!f.core.finishFrameOpportunity({fresh.previewWork,true}));
}
void cancelMetricsLifetime(){
 using State=runtime::RuntimePreviewWorkState;
 for(unsigned mode=0;mode<4;++mode){
  Fixture f;f.stroke();runtime::RuntimeFrameOpportunity old;assert(f.core.serviceFrameOpportunity(false,old)&&old.previewWork);
  if(mode==0)f.point(input::PointerPhase::kCancel,80,40);
  if(mode==1)assert(f.core.resizeSurface({256,256,256,256,1,1.1F}));
  if(mode==2){assert(f.core.loseSurface());assert(f.core.rebindSurface({256,256,256,256,1,1}));}
  if(mode==3)assert(f.core.close());
  assert(f.core.previewWorkState(*old.previewWork)==State::kSuperseded);assert(!f.core.finishFrameOpportunity({old.previewWork,true}));
  if(mode==1){runtime::RuntimeFrameOpportunity fresh;assert(f.core.serviceFrameOpportunity(false,fresh)&&fresh.previewWork);assert(fresh.previewWork->metricsGeneration!=old.previewWork->metricsGeneration);assert(f.core.finishFrameOpportunity({fresh.previewWork,true}));}
 }
}
void creationCancelUndoRedo(){
 Fixture f;f.stroke();f.point(input::PointerPhase::kUp,80,40);assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);
 const auto revision=f.core.previewRenderState().contentRevision;
 runtime::CanvasControlPayload p{};p.kind=runtime::CanvasControlPayloadKind::kPanBy;p.x=8;p.y=4;f.control(p);
 p={};p.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;p.creationPreset=runtime::CanvasCreationPreset::kRectangle;f.control(p);
 f.point(input::PointerPhase::kDown,180,120);f.point(input::PointerPhase::kMove,210,150);f.point(input::PointerPhase::kCancel,210,150);
 p={};p.kind=runtime::CanvasControlPayloadKind::kUndo;f.control(p);p.kind=runtime::CanvasControlPayloadKind::kRedo;f.control(p);
 assert(f.core.previewRenderState().contentRevision==revision);assert(f.adapter.pollFrame()==platform::WebFrameResult::kRetry);f.drain();assert(f.core.readback().semanticGeneration==3);assert(!f.core.canonicalPresentationDirty());assert(!f.core.previewActive());
}
void currentRejectAndGpuFailure(){
 {Fixture f;f.stroke();assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);ur3FenceResult=GL_WAIT_FAILED;assert(f.adapter.pollFrame()==platform::WebFrameResult::kRejected);assert(f.adapter.frameStatus().pending);assert(f.adapter.presentedCount()==0);}
 struct RejectSink final:ink::CanonicalVisibilitySink {
  ink::HandoffResult canonicalCommitted(const ink::CanonicalHandoffIdentity&,const semantic::CanonicalCommitRecord&)noexcept override{return ink::HandoffResult::kAccepted;}
  ink::HandoffResult canonicalVisible(const ink::CanonicalHandoffIdentity&,const render::FrameState&)noexcept override{return ink::HandoffResult::kRejected;}
 } sink;
 {Fixture f;f.stroke();f.point(input::PointerPhase::kUp,80,40);assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);const auto canonical=f.core.lastSubmittedCanonicalWork();assert(canonical);ink_playground::InkPlaygroundHistoryTestAccess::retainPreviewForFailure(f.core.qualificationRuntime());assert(f.core.receivePlatformPresented({*canonical,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}));assert(f.core.previewActive());f.core.qualificationRuntime().setCanonicalVisibilitySink(&sink);assert(f.adapter.pollFrame()==platform::WebFrameResult::kRejected);assert(f.adapter.frameStatus().pending);assert(f.adapter.presentedCount()==0);f.core.qualificationRuntime().setCanonicalVisibilitySink(nullptr);}
 {Fixture f;f.stroke();assert(f.adapter.prepareFrame(false)==platform::WebFrameResult::kPending);ur3ContextLost=true;assert(f.adapter.pollFrame()==platform::WebFrameResult::kIdle);assert(!f.adapter.frameStatus().pending);assert(f.adapter.presentedCount()==0);ur3ContextLost=false;}
}
int main(){displayOnlyDelayedCallback();contentAndNewGestureCallbacks();strictQualification();cancelMetricsLifetime();creationCancelUndoRedo();currentRejectAndGpuFailure();std::cout<<"preview completion: all boundary cases passed\n";}


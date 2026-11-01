#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include <stdexcept>
#include <iostream>
using namespace canvas;
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
void deferredShapeDrag(){
 runtime::RuntimeCompositionCore c;require(c.open(),"shape open");
 auto canonical=std::make_unique<render::RasterSkiaSurfaceProvider>();auto* target=canonical.get();
 auto preview=std::make_unique<render::TransparentOverlaySkiaSurfaceProvider>();auto* overlay=preview.get();
 require(target->resize(256,128).code==render::BackendSubmissionCode::kAccepted&&
         overlay->resize(256,128).code==render::BackendSubmissionCode::kAccepted,"shape targets");
 require(c.registerSurfaceProvider("shape-target",std::move(canonical))&&
         c.registerPreviewSurfaceProvider("shape-overlay",std::move(preview))&&
         c.bindSurface(256,128)&&c.registerSource(7),"shape runtime");
 require(c.setPlatformPresentationDeferred(true),"shape deferred");
 auto& facade=c.controlFacade();std::uint64_t request=0,sequence=0;
 const auto control=[&](runtime::CanvasControlPayload payload){
  runtime::CanvasControlRequest command{};command.clientId=91;command.requestId=++request;
  command.target=facade.mountedCanvasTargets().front();command.payload=payload;
  require(facade.submitCanvasControl(command).state==runtime::ProductControlState::kQueued&&c.ownerPump(),"shape control");
 };
 runtime::CanvasControlPayload tool{};tool.kind=runtime::CanvasControlPayloadKind::kSelectCreationPreset;
 tool.creationPreset=runtime::CanvasCreationPreset::kRectangle;control(tool);
 const auto pointer=[&](input::PointerPhase phase,float x,float y){
  input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=++sequence;s.timestampNs=sequence*1000000;s.phase=phase;s.x=x;s.y=y;
  require(c.submit({{s}}),"shape pointer");
 };
 pointer(input::PointerPhase::kDown,40,40);pointer(input::PointerPhase::kUp,140,100);
 runtime::RuntimeFrameOpportunity frame;
 require(c.serviceFrameOpportunity(false,frame)&&frame.canonicalWork.has_value(),"shape canonical setup");
 require(c.receivePlatformPresented({*frame.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"shape setup feedback");
 tool={};tool.kind=runtime::CanvasControlPayloadKind::kSelectTool;tool.tool=runtime::CanvasToolKind::kSelect;control(tool);
 pointer(input::PointerPhase::kDown,80,70);pointer(input::PointerPhase::kUp,80,70);
 require(c.serviceFrameOpportunity(false,frame),"shape selection frame");
 if(frame.canonicalWork)require(c.receivePlatformPresented({*frame.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"shape selection feedback");
 const auto before=c.readback().semanticGeneration;
 const auto targetPresents=target->presentCount(),overlayPresents=overlay->presentCount();
 pointer(input::PointerPhase::kDown,80,70);pointer(input::PointerPhase::kMove,100,60);
 require(target->presentCount()==targetPresents&&overlay->presentCount()==overlayPresents,"pointer callback rasterizes transient scene");
 require(c.serviceFrameOpportunity(false,frame)&&!frame.canonicalRequested&&!frame.canonicalWork,
         "live transform redundantly submitted canonical frame");
 require(target->presentCount()==targetPresents+1&&overlay->presentCount()==overlayPresents&&!overlay->overlayVisible(),
         "transient frame rebuilt independent overlay instead of warm target");
 require(c.readback().semanticGeneration==before,"drag move mutated semantic document");
 pointer(input::PointerPhase::kUp,110,60);
 require(c.serviceFrameOpportunity(false,frame)&&frame.canonicalRequested&&frame.canonicalWork,"drag terminal missing canonical frame");
 require(c.readback().semanticGeneration==before+1,"drag terminal mutation count");
 require(c.receivePlatformPresented({*frame.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"drag terminal feedback");
 const auto committed=c.readback().semanticGeneration;
 pointer(input::PointerPhase::kDown,110,60);pointer(input::PointerPhase::kMove,125,55);
 require(c.serviceFrameOpportunity(false,frame)&&!frame.canonicalWork,"cancel setup transient");
 pointer(input::PointerPhase::kCancel,125,55);
 require(c.readback().semanticGeneration==committed,"cancelled transform mutated document");
 require(c.serviceFrameOpportunity(false,frame)&&frame.canonicalRequested&&frame.canonicalWork,
         "cancel exact restoration");
 require(c.receivePlatformPresented({*frame.canonicalWork,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),
         "cancel restoration feedback");
 require(c.loseSurface(),"transient target loss");
}
void drawAfterZoom(bool settling, bool cancel){
 interaction::CanvasEditingRuntime r;
 require(r.bindSurface(256,128)&&r.registerInputSource(7,"draw-after-zoom"),"draw setup");
 require(r.selectBrushProfile("round-handdrawn",3),"draw brush");
 r.setPlatformPresentationDeferred(true);
 input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=0;s.timestampNs=0;
 const auto pointer=[&](input::PointerPhase phase,float x,float y){
  ++s.sequence;++s.timestampNs;s.phase=phase;s.x=x;s.y=y;
  require(r.acceptPlatformBatch({{s}},s.sequence),"draw sample");
 };
 for(unsigned i=0;i<8;++i){pointer(input::PointerPhase::kDown,30,20+i*10);pointer(input::PointerPhase::kUp,180,20+i*10);}
 require(r.presentCanonicalFrame(r.nextCanonicalFrameId(),0),"draw initial exact");
 interaction::PreviewPresentationCapture capture;std::uint64_t count=0;
 require(r.capturePreviewPresentation(capture)&&r.renderPreviewPresentation(capture,&count)&&r.acknowledgePreviewPresentation(capture,count),"draw initial preview");
 const auto exact=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({exact.viewId,exact.frameId,exact.surfaceGeneration,exact.metricsGeneration,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"draw initial feedback");
 const auto generation=r.semanticGeneration();const auto scene=r.sceneRevision();
 require(r.setViewportZoomAt(2,128,64),"draw zoom");
 const auto start=std::chrono::steady_clock::time_point{}+std::chrono::seconds(1);
 bool presented=false;require(r.serviceNavigationPresentation(start,presented),"draw navigation");
 if(settling){
  unsigned tries=0;while(r.serviceNavigationPresentation(start+std::chrono::seconds(1),presented))require(++tries<128,"draw settle setup");
  require(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0)&&r.canonicalPresentationRetryable(),"draw settle did not yield");
 }
 pointer(input::PointerPhase::kDown,40,60);
 require(r.serviceNavigationPresentation(start+std::chrono::seconds(1),presented),"active brush fell through to synchronous viewport reconstruction");
 require(!r.lastSubmittedCanonicalFrame()&&r.semanticGeneration()==generation&&r.sceneRevision()==scene,"live brush changed canonical state");
 pointer(input::PointerPhase::kMove,80,65);
 require(r.capturePreviewPresentation(capture)&&r.renderPreviewPresentation(capture,&count),"live brush preview starved");
 pointer(cancel?input::PointerPhase::kCancel:input::PointerPhase::kUp,120,60);
 require(r.semanticGeneration().value()==generation.value()+(cancel?0:1),"draw terminal mutation mismatch");
 (void)r.serviceNavigationPresentation(start+std::chrono::seconds(2),presented);
 unsigned yields=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0)){
  require(r.canonicalPresentationRetryable()&&!r.lastSubmittedCanonicalFrame(),"draw partial exact acknowledged");
  require(++yields<32,"draw exact never converged");
 }
 require(yields>0,"draw terminal discarded cooperative exact policy");
 const auto replacement=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({replacement.viewId,replacement.frameId,replacement.surfaceGeneration,replacement.metricsGeneration,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"draw replacement feedback");
 pointer(input::PointerPhase::kDown,40,60);
 require(!r.serviceNavigationPresentation(start+std::chrono::seconds(3),presented),"settled camera unnecessarily holds canonical work");
 pointer(input::PointerPhase::kCancel,40,60);
 require(r.loseSurface(),"draw loss");
 require(!r.serviceNavigationPresentation(start+std::chrono::seconds(3),presented),"draw lost target reused");
}
int main(){
 deferredShapeDrag();
 drawAfterZoom(false,false);drawAfterZoom(true,false);drawAfterZoom(false,true);
 interaction::CanvasEditingRuntime r;
 require(r.bindSurface(256,128)&&r.registerInputSource(7,"navigation"),"prepare");
 require(r.selectBrushProfile("round-handdrawn",1),"compact brush");
 r.setPlatformPresentationDeferred(true);
 input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=1;s.timestampNs=1;s.x=30;s.y=50;s.phase=input::PointerPhase::kDown;
 require(r.acceptPlatformBatch({{s}},1),"down");s.sequence=2;s.timestampNs=2;s.x=120;s.phase=input::PointerPhase::kUp;
 require(r.acceptPlatformBatch({{s}},2),"up");
 for(unsigned i=1;i<8;++i){
  s.phase=input::PointerPhase::kDown;s.x=30;s.y=20+i*10;s.sequence=2*i+1;s.timestampNs=s.sequence;
  require(r.acceptPlatformBatch({{s}},s.sequence),"extra down");
  s.phase=input::PointerPhase::kUp;s.x=120;++s.sequence;s.timestampNs=s.sequence;
  require(r.acceptPlatformBatch({{s}},s.sequence),"extra up");
 }
 require(r.presentCanonicalFrame(r.nextCanonicalFrameId(),0),"exact");
 interaction::PreviewPresentationCapture preview;std::uint64_t previewCount=0;
 require(r.capturePreviewPresentation(preview)&&r.renderPreviewPresentation(preview,&previewCount)&&r.acknowledgePreviewPresentation(preview,previewCount),"preview feedback");
 const auto exact=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({exact.viewId,exact.frameId,exact.surfaceGeneration,exact.metricsGeneration,render::PresentOutcome::kPresented,render::PresentEvidenceKind::kPlatformQualified,{}}),"exact feedback");
 const auto generation=r.semanticGeneration();const auto scene=r.sceneRevision();
 require(r.setViewportZoomAt(1.1F,100,60),"zoom");
 const auto start=std::chrono::steady_clock::time_point{}+std::chrono::seconds(1);
 bool presented=false;
 require(r.serviceNavigationPresentation(start,presented),"navigation work missing");
 unsigned coldSteps=0;
 while(!presented){
  require(r.serviceNavigationPresentation(start,presented),"cold navigation abandoned");
  require(++coldSteps<128,"complete navigation coverage never presented");
 }
 require(!r.lastSubmittedCanonicalFrame()&&r.canonicalPresentationDirty(),"navigation created canonical coverage");
 unsigned refinementSteps=0;
 while(r.serviceNavigationPresentation(start+std::chrono::seconds(1),presented))
  require(++refinementSteps<128&&!r.lastSubmittedCanonicalFrame(),"navigation refinement published canonical work");
 unsigned navigationSteps=0;
 while(r.serviceNavigationPresentation(start+std::chrono::seconds(1),presented))
  require(++navigationSteps<128&&!r.lastSubmittedCanonicalFrame(),"navigation never settles or publishes canonical work");
 unsigned yields=0;
 while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0)){
  require(r.canonicalPresentationRetryable()&&r.canonicalPresentationDirty(),"preparation lost exact demand");
  require(!r.lastSubmittedCanonicalFrame(),"partial preparation created canonical receipt");
  require(++yields<16,"exact preparation never converged");
 }
 require(yields>0,"navigation settle was not cooperatively prepared");
 require(r.semanticGeneration()==generation&&r.sceneRevision()==scene,"camera mutated document");
 require(r.loseSurface(),"lose");
 require(!r.serviceNavigationPresentation(start+std::chrono::seconds(2),presented),"lost source reused");
 std::cout<<"navigation feedback has no canonical receipt; dirty gated and exact settle\n";
}

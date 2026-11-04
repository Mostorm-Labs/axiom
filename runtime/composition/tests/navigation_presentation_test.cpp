#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/runtime/internal/runtime_composition_core.hpp"
#include <stdexcept>
#include <iostream>
using namespace canvas;
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
class DemandRuntime final : public interaction::CanvasEditingRuntime {
public:
 void bounded(){navigationExactPreparation_=true;}
 void ordinary(){navigationExactPreparation_=false;}
 bool attempt(std::uint64_t id){return presentCanonicalFrame(id,0,false);}
};
void retainedDemand(){
 DemandRuntime r;require(r.bindSurface(256,128)&&r.registerInputSource(7,"exact-demand")&&r.selectBrushProfile("round-handdrawn",4),"demand setup");
 r.setPlatformPresentationDeferred(true);std::uint64_t sequence=0;
 for(unsigned n=0;n<80;++n){
  input::PlatformPointerBatch b;
  for(unsigned i=0;i<3;++i){input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=++sequence;s.timestampNs=sequence*8000000;
   s.x=20+i*80;s.y=20+(n%8)*10;s.phase=i==0?input::PointerPhase::kDown:i==2?input::PointerPhase::kUp:input::PointerPhase::kMove;b.samples.push_back(s);}
  require(r.acceptPlatformBatch(b,b.samples.back().timestampNs),"demand input");
 }
 require(r.setSelectionMode(true),"idle selection tool");
 r.ordinary();const auto first=r.nextCanonicalFrameId();
 require(!r.attempt(first)&&r.canonicalPresentationRetryable()&&!r.lastSubmittedCanonicalFrame(),"demand did not yield");
 unsigned retries=0;while(!r.attempt(r.nextCanonicalFrameId()))require(r.canonicalPresentationRetryable()&&++retries<32,"demand did not converge");
 require(r.lastSubmittedCanonicalFrame()->frameId.value()==first,
         "unchanged exact retry rebuilt a new frame identity instead of retaining demand");
 require(r.setViewportZoomAt(1.1F,128,64),"demand camera");r.bounded();
 const auto replacement=r.nextCanonicalFrameId();require(!r.attempt(replacement)&&r.canonicalPresentationRetryable(),"new camera reused old prepared pixels");
 require(r.setViewportZoomAt(1.2F,128,64),"superseding camera");r.bounded();
 const auto latest=r.nextCanonicalFrameId();retries=0;
 while(!r.attempt(r.nextCanonicalFrameId()))require(r.canonicalPresentationRetryable()&&++retries<32,"superseding demand convergence");
 require(r.lastSubmittedCanonicalFrame()->frameId.value()>=latest,"superseded frame identity submitted");
 require(r.setViewportZoomAt(1.3F,128,64),"resource demand camera");r.bounded();
 require(!r.attempt(r.nextCanonicalFrameId())&&r.canonicalPresentationRetryable(),"resource demand setup");
 require(r.refreshResourceDerivedState(),"resource update");
 const auto resourceFrame=r.nextCanonicalFrameId();retries=0;
 while(!r.attempt(r.nextCanonicalFrameId()))require(r.canonicalPresentationRetryable()&&++retries<32,"resource demand convergence");
 require(r.lastSubmittedCanonicalFrame()->frameId.value()>=resourceFrame,"resource change retained old demand identity");
 require(r.setViewportZoomAt(1.4F,128,64),"loss demand camera");r.bounded();
 require(!r.attempt(r.nextCanonicalFrameId())&&r.canonicalPresentationRetryable(),"loss demand setup");
 require(r.loseSurface()&&!r.attempt(r.nextCanonicalFrameId()),"lost demand presented");
 require(r.rebindSurface(),"demand restore");r.bounded();const auto restored=r.nextCanonicalFrameId();retries=0;
 while(!r.attempt(r.nextCanonicalFrameId()))require(r.canonicalPresentationRetryable()&&++retries<32,"restored demand convergence");
 require(r.lastSubmittedCanonicalFrame()->frameId.value()>=restored&&
         r.lastSubmittedCanonicalFrame()->surfaceGeneration.value()==r.surface().generation,"old surface demand survived restore");
}
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
 require(r.selectBrushProfile("round-handdrawn",4),"draw brush");
 r.setPlatformPresentationDeferred(true);
 input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=0;s.timestampNs=0;
 const auto pointer=[&](input::PointerPhase phase,float x,float y){
  ++s.sequence;++s.timestampNs;s.phase=phase;s.x=x;s.y=y;
  require(r.acceptPlatformBatch({{s}},s.sequence),"draw sample");
 };
 for(unsigned i=0;i<160;++i){pointer(input::PointerPhase::kDown,30,20+(i%8)*10);pointer(input::PointerPhase::kUp,180,20+(i%8)*10);}
 {unsigned attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
   require(r.canonicalPresentationRetryable()&&++attempts<64,"draw initial exact");}
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
void handPanUsesNavigationFeedback(){
 interaction::CanvasEditingRuntime r;
 require(r.bindSurface(256,128)&&r.registerInputSource(7,"hand-pan"),"hand pan setup");
 require(r.selectBrushProfile("round-handdrawn",4),"hand pan brush");
 r.setPlatformPresentationDeferred(true);
 input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=0;s.timestampNs=0;
 const auto pointer=[&](input::PointerPhase phase,float x,float y){
  ++s.sequence;++s.timestampNs;s.phase=phase;s.x=x;s.y=y;
  require(r.acceptPlatformBatch({{s}},s.sequence),"hand pan sample");
 };
 pointer(input::PointerPhase::kDown,30,40);pointer(input::PointerPhase::kUp,190,70);
 unsigned attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
  require(r.canonicalPresentationRetryable()&&++attempts<64,"hand pan exact setup");
 interaction::PreviewPresentationCapture capture;std::uint64_t count=0;
 require(r.capturePreviewPresentation(capture)&&r.renderPreviewPresentation(capture,&count)&&
         r.acknowledgePreviewPresentation(capture,count),"hand pan preview setup");
 const auto exact=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({exact.viewId,exact.frameId,exact.surfaceGeneration,
         exact.metricsGeneration,render::PresentOutcome::kPresented,
         render::PresentEvidenceKind::kPlatformQualified,{}}),"hand pan exact setup feedback");
 require(r.setPanTool(true),"hand pan tool");
 attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
  require(r.canonicalPresentationRetryable()&&++attempts<64,"hand pan tool presentation");
 const auto toolFrame=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({toolFrame.viewId,toolFrame.frameId,toolFrame.surfaceGeneration,
         toolFrame.metricsGeneration,render::PresentOutcome::kPresented,
         render::PresentEvidenceKind::kPlatformQualified,{}}),"hand pan tool feedback");
 require(r.setViewportZoomAt(2.0F,128,64),"hand pan zoom");
 bool presented=false;const auto now=std::chrono::steady_clock::time_point{}+std::chrono::seconds(1);
 unsigned steps=0;
 while(!presented){
  require(r.serviceNavigationPresentation(now,presented),"hand pan navigation feedback rejected");
  require(++steps<64,"hand pan navigation feedback stalled");
 }
 require(!r.lastSubmittedCanonicalFrame(),"hand pan emitted exact canonical receipt during navigation");
}
void selectPinchUsesNavigationFeedback(){
 interaction::CanvasEditingRuntime r;
 require(r.bindSurface(256,128)&&r.registerInputSource(7,"select-pinch"),"select pinch setup");
 r.setPlatformPresentationDeferred(true);
 unsigned attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
  require(r.canonicalPresentationRetryable()&&++attempts<64,"select pinch exact setup");
 const auto exact=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({exact.viewId,exact.frameId,exact.surfaceGeneration,
         exact.metricsGeneration,render::PresentOutcome::kPresented,
         render::PresentEvidenceKind::kPlatformQualified,{}}),"select pinch setup feedback");
 std::uint64_t sequence=0;
 const auto batch=[&](std::uint64_t pointer,input::PointerPhase phase,float x,float y){
  input::PlatformPointerSample sample{};sample.source=7;sample.pointer=pointer;
  sample.sequence=++sequence;sample.timestampNs=sequence*1'000'000;
  sample.phase=phase;sample.x=x;sample.y=y;
  require(r.acceptPlatformBatch({{sample}},sample.timestampNs),"select pinch sample");
 };
 batch(1,input::PointerPhase::kDown,70,50);
 batch(2,input::PointerPhase::kDown,150,50);
 const auto beforeCoalesced = r.cameraGeneration();
 input::PlatformPointerBatch coalesced;
 input::PlatformPointerSample first{};first.source=7;first.pointer=1;first.sequence=++sequence;first.timestampNs=sequence*1'000'000;first.phase=input::PointerPhase::kMove;first.x=60;first.y=45;
 input::PlatformPointerSample second{};second.source=7;second.pointer=2;second.sequence=++sequence;second.timestampNs=sequence*1'000'000;second.phase=input::PointerPhase::kMove;second.x=160;second.y=55;
 coalesced.samples={first,second};
 require(r.acceptPlatformBatch(coalesced,second.timestampNs),"coalesced pinch sample");
 require(r.cameraGeneration()==beforeCoalesced+1,"coalesced pinch advanced camera more than once");
 bool presented=false;const auto now=std::chrono::steady_clock::time_point{}+std::chrono::seconds(1);
 unsigned steps=0;
 while(!presented){
  require(r.serviceNavigationPresentation(now,presented),"select pinch navigation feedback rejected");
  require(++steps<64,"select pinch navigation feedback stalled");
 }
 require(!r.lastSubmittedCanonicalFrame(),"select pinch emitted exact canonical receipt during gesture");
 batch(1,input::PointerPhase::kUp,60,45);
 batch(2,input::PointerPhase::kUp,160,55);
}
void selectedWheelNavigationUsesFeedback(){
 interaction::CanvasEditingRuntime r;
 require(r.bindSurface(256,128)&&r.registerInputSource(7,"selected-wheel"),"selected wheel setup");
 r.setPlatformPresentationDeferred(true);
 semantic::ObjectRecord image;
 image.id=foundation::ObjectId::fromUint64(7701);
 image.kind=semantic::ObjectKind::kImage; image.kind_version=1;
 image.placement.order_key=semantic::OrderKey({1});
 semantic::ImageContent content;content.resource_id.value=foundation::ObjectId::fromUint64(7702);
 content.intrinsic_width=64;content.intrinsic_height=48;content.width=64;content.height=48;
 image.content=content;image.transform.tx=48;image.transform.ty=32;
 semantic::Operation insert{semantic::OperationId{foundation::ObjectId::fromUint64(7703)},
     r.documentId(),1,1,semantic::InsertObjectsOp{{image}}};
 require(r.applyExternalOperation(insert,semantic::ApplySource::kRemoteSync).disposition==semantic::ApplyDisposition::kApplied,"selected wheel image");
 unsigned attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
  require(r.canonicalPresentationRetryable()&&++attempts<64,"selected wheel exact setup");
 const auto exact=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({exact.viewId,exact.frameId,exact.surfaceGeneration,
         exact.metricsGeneration,render::PresentOutcome::kPresented,
         render::PresentEvidenceKind::kPlatformQualified,{}}),"selected wheel exact feedback");
 require(r.setSelectionMode(true)&&r.selectAtViewPoint(60,44),"selected wheel selection");
 attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
  require(r.canonicalPresentationRetryable()&&++attempts<64,"selected wheel overlay frame");
 const auto selected=*r.lastSubmittedCanonicalFrame();
 require(r.receivePlatformPresented({selected.viewId,selected.frameId,selected.surfaceGeneration,
         selected.metricsGeneration,render::PresentOutcome::kPresented,
         render::PresentEvidenceKind::kPlatformQualified,{}}),"selected wheel overlay feedback");
 const auto before=r.cameraGeneration();
 require(r.setViewportZoomAt(1.35F,128,64),"selected wheel zoom");
 require(r.cameraGeneration()==before+1,"selected wheel camera");
 bool presented=false;const auto now=std::chrono::steady_clock::time_point{}+std::chrono::seconds(1);
 unsigned steps=0;
 while(!presented){
  require(r.serviceNavigationPresentation(now,presented),"selected wheel navigation rejected");
  require(++steps<128,"selected wheel navigation stalled");
 }
 require(!r.lastSubmittedCanonicalFrame(),"selected wheel emitted canonical receipt");
 require(r.selectionOverlay() && r.selectionOverlay()->viewGeneration()==r.cameraGeneration(),
         "selected wheel selection chrome did not follow navigation camera");
 const auto semantic=r.semanticGeneration();const auto revision=r.sceneRevision();
 // Native trackpads send wheel pan, then absolute zoom controls for both
 // pinch directions. None of these events owns a touch contact.
 for(unsigned i=0;i<24;++i){
  if(i%3==0)require(r.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,3,-2}),"selected wheel pan");
  else require(r.setViewportZoomAt(i%3==1?1.5F:1.15F,128,64),"selected wheel alternating zoom");
  require(r.serviceNavigationPresentation(now+std::chrono::milliseconds(i*16),presented),"selected wheel lost feedback on repeated input");
  require(!r.lastSubmittedCanonicalFrame(),"selected wheel rebuilt exact frame during navigation");
  require(r.selectionOverlay()&&r.selectionOverlay()->viewGeneration()==r.cameraGeneration(),"selected wheel chrome lags repeated camera");
 }
 steps=0;
 while(r.serviceNavigationPresentation(now+std::chrono::seconds(2),presented))
  require(++steps<128,"selected wheel never reaches exact settle");
 attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
  require(r.canonicalPresentationRetryable()&&++attempts<64,"selected wheel exact settle stalled");
 require(r.semanticGeneration()==semantic&&r.sceneRevision()==revision&&r.selectedPrimaryObject()==image.id,
         "selected wheel navigation changed selection or document");
}
void pendingCanonicalNavigationPreservesCompletion(){
 runtime::RuntimeCompositionCore c;require(c.open(),"pending navigation open");
 auto target=std::make_unique<render::RasterSkiaSurfaceProvider>();
 require(target->resize(256,128).code==render::BackendSubmissionCode::kAccepted&&
         c.registerSurfaceProvider("pending-target",std::move(target))&&
         c.bindSurface(256,128)&&c.setPlatformPresentationDeferred(true),"pending navigation setup");
 runtime::RuntimeFrameOpportunity initial;
 require(c.serviceFrameOpportunity(true,initial)&&initial.canonicalWork,"pending initial submission");
 const auto original=c.lastSubmittedCanonicalWork();require(original.has_value(),"pending work missing");
 auto& facade=c.controlFacade();runtime::CanvasControlRequest request{};
 request.clientId=991;request.requestId=1;request.target=facade.mountedCanvasTargets().front();
 request.payload.kind=runtime::CanvasControlPayloadKind::kZoomAt;
 request.payload.scale=2;request.payload.x=128;request.payload.y=64;
 require(facade.submitCanvasControl(request).state==runtime::ProductControlState::kQueued&&c.ownerPump(),"pending camera");
 runtime::RuntimeFrameOpportunity opportunity;
 require(c.serviceFrameOpportunity(false,opportunity)&&opportunity.navigationPending,
         "pending canonical prevented navigation feedback");
 require(!opportunity.canonicalRequested&&!opportunity.canonicalWork,
         "navigation duplicated an in-flight canonical completion");
 require(c.lastSubmittedCanonicalWork()==original,"navigation replaced pending canonical identity");
 require(c.receivePlatformPresented({*original,render::PresentOutcome::kPresented,
         render::PresentEvidenceKind::kPlatformQualified,{}}),"navigation lost original completion");
}
int main(){
 pendingCanonicalNavigationPreservesCompletion();
 retainedDemand();
 deferredShapeDrag();
 drawAfterZoom(false,false);drawAfterZoom(true,false);drawAfterZoom(false,true);handPanUsesNavigationFeedback();selectPinchUsesNavigationFeedback();selectedWheelNavigationUsesFeedback();
 interaction::CanvasEditingRuntime r;
 require(r.bindSurface(256,128)&&r.registerInputSource(7,"navigation"),"prepare");
 require(r.selectBrushProfile("round-handdrawn",1),"compact brush");
 r.setPlatformPresentationDeferred(true);
 input::PlatformPointerSample s;s.source=7;s.pointer=1;s.sequence=1;s.timestampNs=1;s.x=30;s.y=50;s.phase=input::PointerPhase::kDown;
 require(r.acceptPlatformBatch({{s}},1),"down");s.sequence=2;s.timestampNs=2;s.x=120;s.phase=input::PointerPhase::kUp;
 require(r.acceptPlatformBatch({{s}},2),"up");
 for(unsigned i=1;i<80;++i){
  s.phase=input::PointerPhase::kDown;s.x=30;s.y=20+(i%8)*10;s.sequence=2*i+1;s.timestampNs=s.sequence;
  require(r.acceptPlatformBatch({{s}},s.sequence),"extra down");
  s.phase=input::PointerPhase::kUp;s.x=120;++s.sequence;s.timestampNs=s.sequence;
  require(r.acceptPlatformBatch({{s}},s.sequence),"extra up");
 }
 {unsigned attempts=0;while(!r.presentCanonicalFrame(r.nextCanonicalFrameId(),0))
   require(r.canonicalPresentationRetryable()&&++attempts<64,"exact");}
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

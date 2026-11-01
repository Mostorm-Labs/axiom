#include "canvas/interaction/canvas_editing_runtime.hpp"
#include <stdexcept>
#include <iostream>
using namespace canvas;
void require(bool ok,const char* msg){if(!ok)throw std::runtime_error(msg);}
int main(){
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
 require(r.serviceNavigationPresentation(start,presented)&&presented,"navigation feedback missing");
 require(!r.lastSubmittedCanonicalFrame()&&r.canonicalPresentationDirty(),"navigation created canonical coverage");
 presented=false;require(r.serviceNavigationPresentation(start+std::chrono::milliseconds(10),presented)&&!presented,"clean navigation submitted twice");
 require(!r.serviceNavigationPresentation(start+std::chrono::seconds(1),presented),"navigation never settles");
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

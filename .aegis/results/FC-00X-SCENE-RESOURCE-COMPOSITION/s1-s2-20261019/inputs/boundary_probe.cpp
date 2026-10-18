#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <cstdio>
using namespace canvas;
class FailResizeProvider final:public render::SkiaSurfaceProvider {
 public:
  render::RasterSkiaSurfaceProvider real;
  bool failResize=false;
  render::RenderTargetInfo describe() const noexcept override{return real.describe();}
  render::SkiaSurfaceAcquireResult acquire() noexcept override{return real.acquire();}
  void release() noexcept override{real.release();}
  render::BackendSubmissionResult lose() noexcept override{return real.lose();}
  render::BackendSubmissionResult present() noexcept override{return real.present();}
  render::BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h) noexcept override{return failResize?render::BackendSubmissionResult::rejected("injected preview resize failure"):real.resize(w,h);}
  render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> d) noexcept override{return real.readbackRgba(d);}
  std::uint64_t generation() const noexcept override{return real.generation();}
  render::BackendSubmissionResult advanceGeneration() noexcept override{return real.advanceGeneration();}
  std::uint64_t readbackCount() const noexcept override{return real.readbackCount();}
  std::uint64_t cpuCopyCount() const noexcept override{return real.cpuCopyCount();}
  std::uint64_t presentCount() const noexcept override{return real.presentCount();}
};
void partialResize() {
  runtime::RuntimeComposition c;assert(c.open());
  auto p=std::make_unique<FailResizeProvider>();auto* preview=p.get();assert(p->resize(256,256).code==render::BackendSubmissionCode::kAccepted);
  assert(c.registerPreviewSurfaceProvider("fault-preview",std::move(p))&&c.bindSurface(256,256));
  const auto old=c.surfaceSnapshot();auto& f=c.controlFacade();const auto oldTarget=f.mountedCanvasTargets().front();
  const auto queued=f.setSelectionMode(true,1,oldTarget.runtimeEpoch);assert(queued.control&&queued.state==runtime::ProductControlState::kQueued);
  preview->failResize=true;const bool resized=c.resizeSurface(320,240);const auto now=c.surfaceSnapshot();const auto actualPreview=preview->describe();
  assert(c.ownerPump());const auto receipt=f.canvasControlReceipt(queued.control->key);assert(receipt);
  std::printf("OBSERVED resize failure: result=%d ready=%d canonicalBefore=%ux%u canonicalAfter=%ux%u previewAfter=%ux%u oldSurface=%llu newSurface=%llu oldAttachment=%llu newAttachment=%llu oldRequestState=%u\n",resized,c.state()==runtime::RuntimeCompositionState::kReady,old.width,old.height,now.width,now.height,actualPreview.metrics.physicalWidth,actualPreview.metrics.physicalHeight,old.surfaceGeneration,now.surfaceGeneration,oldTarget.attachmentEpoch,f.mountedCanvasTargets().front().attachmentEpoch,unsigned(receipt->state));
  assert(!resized&&c.state()==runtime::RuntimeCompositionState::kReady&&now.width==320&&actualPreview.metrics.physicalWidth==256&&now.surfaceGeneration!=old.surfaceGeneration&&oldTarget==f.mountedCanvasTargets().front()&&receipt->state==runtime::CanvasControlReceiptState::kApplied);
}
void replaceReadyProvider() {
  runtime::RuntimeComposition c;assert(c.open()&&c.bindSurface(256,256));auto& f=c.controlFacade();const auto old=f.mountedCanvasTargets().front();
  const auto q=f.setSelectionMode(true,1,old.runtimeEpoch);assert(q.control);
  auto p=std::make_unique<render::RasterSkiaSurfaceProvider>();assert(p->resize(256,256).code==render::BackendSubmissionCode::kAccepted);
  assert(c.registerSurfaceProvider("replacement",std::move(p)));const auto now=c.surfaceSnapshot();const auto target=f.mountedCanvasTargets().front();assert(c.ownerPump());const auto receipt=f.canvasControlReceipt(q.control->key);assert(receipt);
  std::printf("OBSERVED Ready provider replace: surface=%llu oldAttachment=%llu newAttachment=%llu targetUnchanged=%d oldRequestState=%u\n",now.surfaceGeneration,old.attachmentEpoch,target.attachmentEpoch,old==target,unsigned(receipt->state));
  assert(now.surfaceGeneration!=old.attachmentEpoch&&old==target&&receipt->state==runtime::CanvasControlReceiptState::kApplied);
}
void unboundPromotion() {
  runtime::RuntimeComposition c;assert(c.open());auto p=std::make_unique<render::RasterSkiaSurfaceProvider>();assert(p->resize(256,256).code==render::BackendSubmissionCode::kAccepted);
  assert(c.registerSurfaceProvider("prepared",std::move(p)));const auto before=c.state();const bool registered=c.registerSource(1);
  std::printf("OBSERVED pre-bind promotion: stateBefore=%u registeredSource=%d stateAfter=%u readyWithoutBind=%d previewAvailable=%d\n",unsigned(before),registered,unsigned(c.state()),c.state()==runtime::RuntimeCompositionState::kReady,c.previewProviderAvailable());
  assert(registered&&before==runtime::RuntimeCompositionState::kBound&&c.state()==runtime::RuntimeCompositionState::kReady);
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);partialResize();replaceReadyProvider();unboundPromotion();}

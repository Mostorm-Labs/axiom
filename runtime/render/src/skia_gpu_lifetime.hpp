#pragma once
#include "gpu_resource_retirement.hpp"
#include "render_execution.hpp"
#include "include/gpu/ganesh/GrDirectContext.h"
#include <memory>
namespace canvas::render::internal {
// All allocation/metadata happens before GPU recording. Destructor also fences
// early-return writes. Submission is asynchronous; this is not a receipt.
class SkiaGpuLifetime final {
 public:
  SkiaGpuLifetime(GpuResourceRetirement& queue,const std::shared_ptr<resources::ResourceBudgetManager>& budget,
      SkiaSurfaceProvider& provider,GrDirectContext* context,
      resources::ResourceClassMask classes=resources::kPixelResourceClasses,
      resources::ResourceClassMask existing=resources::kPixelResourceClasses)noexcept
      :context_(context),queue_(queue),budget_(budget),provider_(provider),classes_(classes),existing_(existing){
    admit();
  }
  bool restart()noexcept {
    if(ticket_)return false;
    ready_=false;used_=false;success_=true;admit();return ready_;
  }
 private:
  void admit()noexcept {
    if(!context_){ready_=true;return;}
    try{
      auto payload=std::make_unique<std::shared_ptr<GpuResourceRetirement::Ticket>>();
      ticket_=queue_.begin(budget_,provider_.gpuCompletionService(),classes_,existing_);
      if(!ticket_)return;
      ticket_->expectSubmission();
      *payload=ticket_;payload_=std::move(payload);ready_=true;
    }catch(const std::bad_alloc&){}
  }
 public:
  ~SkiaGpuLifetime(){(void)finish();}
  SkiaGpuLifetime(const SkiaGpuLifetime&)=delete;
  bool ready()const noexcept{return ready_;}
  std::uint64_t epoch()const noexcept{return ticket_?ticket_->epoch:0;}
  void bindFrame([[maybe_unused]] const FrameState& f,[[maybe_unused]] std::uint64_t resource)noexcept {
    if(!ticket_)return;
    G5_OBSERVE(g5::Kind::Basis,g5::Tag::resource_lease,true,21,reinterpret_cast<std::uintptr_t>(ticket_.get()),ticket_->epoch,ticket_->state->lifetime,f.viewId.value(),f.frameId.value(),f.sceneGeneration.value(),f.sceneReadToken.value(),f.camera.generation.value(),f.surfaceGeneration.value(),f.metricsGeneration.value(),resource);
  }
  void touch(bool preparation=false)noexcept{used_=true;if(ticket_&&preparation)ticket_->preparation=true;}
  bool publish(resources::ResourceLease& lease,std::size_t resident)noexcept {
    return ticket_?ticket_->add(lease.completedShrink(resident)):lease.shrink(resident);
  }
  bool finish()noexcept {
    if(!ticket_)return success_;
    ticket_->end();
    if(used_||(execution_&&execution_->gpuWorkRecorded())){
      GrFlushInfo info;info.fFinishedContext=payload_.release();
      info.fFinishedProc=[](void* raw){std::unique_ptr<std::shared_ptr<GpuResourceRetirement::Ticket>> p(
          static_cast<std::shared_ptr<GpuResourceRetirement::Ticket>*>(raw));(*p)->finish();};
      #if defined(AXIOM_G5_UR0_DIAGNOSTICS)
      g5::gpuTicketPrepared(reinterpret_cast<std::uintptr_t>(ticket_.get()),reinterpret_cast<std::uintptr_t>(context_),ticket_->epoch);
      G5_OBSERVE(g5::Kind::Basis,g5::Tag::resource_lease,true,20,reinterpret_cast<std::uintptr_t>(ticket_.get()),ticket_->epoch,ticket_->state->lifetime);
#endif
      context_->flush(info);
      success_=context_->submit(GrSyncCpu::kNo)&&!ticket_->state->abandoned.load();
#if defined(AXIOM_G5_UR0_DIAGNOSTICS)
      g5::gpuTicketSubmitted(reinterpret_cast<std::uintptr_t>(ticket_.get()),success_);
#endif
      ticket_->submitted(success_);
      if(!success_)ticket_->finish(); // Failed submission, not successful GPU completion.
    }else ticket_->unused();
    ticket_.reset();payload_.reset();
    return success_;
  }
 private:
  GrDirectContext* context_;bool ready_=false,used_=false,success_=true;
  GpuResourceRetirement& queue_;
  const std::shared_ptr<resources::ResourceBudgetManager> budget_;
  SkiaSurfaceProvider& provider_;
  resources::ResourceClassMask classes_,existing_;
  RenderExecutionScope* execution_=RenderExecutionScope::current();
  std::shared_ptr<GpuResourceRetirement::Ticket> ticket_;
  std::unique_ptr<std::shared_ptr<GpuResourceRetirement::Ticket>> payload_;
};
}

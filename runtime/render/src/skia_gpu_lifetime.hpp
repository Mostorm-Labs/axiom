#pragma once
#include "gpu_resource_retirement.hpp"
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
      resources::ResourceClassMask existing=resources::kPixelResourceClasses)noexcept:context_(context){
    if(!context_){ready_=true;return;}
    try{
      auto payload=std::make_unique<std::shared_ptr<GpuResourceRetirement::Ticket>>();
      ticket_=queue.begin(budget,provider.gpuCompletionService(),classes,existing);
      if(!ticket_)return;
      *payload=ticket_;payload_=std::move(payload);ready_=true;
    }catch(const std::bad_alloc&){}
  }
  ~SkiaGpuLifetime(){finish();}
  SkiaGpuLifetime(const SkiaGpuLifetime&)=delete;
  bool ready()const noexcept{return ready_;}
  void touch(bool preparation=false)noexcept{used_=true;if(ticket_&&preparation)ticket_->preparation=true;}
  bool publish(resources::ResourceLease& lease,std::size_t resident)noexcept {
    return ticket_?ticket_->add(lease.completedShrink(resident)):lease.shrink(resident);
  }
  void finish()noexcept {
    if(!ticket_)return;
    ticket_->end();
    if(used_){
      GrFlushInfo info;info.fFinishedContext=payload_.release();
      info.fFinishedProc=[](void* raw){std::unique_ptr<std::shared_ptr<GpuResourceRetirement::Ticket>> p(
          static_cast<std::shared_ptr<GpuResourceRetirement::Ticket>*>(raw));(*p)->finish();};
      context_->flush(info);(void)context_->submit(GrSyncCpu::kNo);
    }else ticket_->finish();
    ticket_.reset();payload_.reset();
  }
 private:
  GrDirectContext* context_;bool ready_=false,used_=false;
  std::shared_ptr<GpuResourceRetirement::Ticket> ticket_;
  std::unique_ptr<std::shared_ptr<GpuResourceRetirement::Ticket>> payload_;
};
}

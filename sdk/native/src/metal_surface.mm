#include "metal_surface.hpp"
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#include "include/core/SkColorSpace.h"
#include "include/core/SkSurface.h"
#include "include/core/SkImage.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendContext.h"
#include "include/gpu/ganesh/mtl/GrMtlDirectContext.h"
#include "include/gpu/ganesh/mtl/GrMtlBackendSurface.h"
#include "include/ports/SkCFObject.h"
#include <cmath>
#include <thread>

namespace canvas::sdk {
namespace {
class Surface final:public render::SkiaSurfaceProvider {
 CAMetalLayer* layer_;
 id<MTLDevice> device_;
 id<MTLCommandQueue> queue_;
 id<MTLTexture> texture_;
 sk_sp<GrDirectContext> context_;
 sk_sp<SkSurface> surface_;
 NativeView view_;
 std::string profile_;
 bool overlay_,visible_=true,lost_=false;
 std::uint64_t generation_=1,presents_=0,readbacks_=0;
 const std::thread::id thread_=std::this_thread::get_id();
 bool create(unsigned w,unsigned h){
  MTLTextureDescriptor* desc=[MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm width:w height:h mipmapped:NO];
  desc.usage=MTLTextureUsageRenderTarget|MTLTextureUsageShaderRead;desc.storageMode=MTLStorageModePrivate;
  texture_=[device_ newTextureWithDescriptor:desc];if(!texture_)return false;
  GrMtlTextureInfo info;info.fTexture=sk_ret_cfp((__bridge GrMTLHandle)texture_);
  const auto target=GrBackendRenderTargets::MakeMtl(static_cast<int>(w),static_cast<int>(h),info);
  surface_=SkSurfaces::WrapBackendRenderTarget(context_.get(),target,kTopLeft_GrSurfaceOrigin,kBGRA_8888_SkColorType,SkColorSpace::MakeSRGB(),nullptr);
  return bool(surface_);
 }
public:
 Surface(CAMetalLayer* layer,NativeView view,bool overlay):layer_(layer),view_(view),profile_(overlay?"native-metal-preview":"native-metal-canonical"),overlay_(overlay){
  device_=MTLCreateSystemDefaultDevice();queue_=[device_ newCommandQueue];if(!device_||!queue_||!layer_)return;
  layer_.device=device_;layer_.pixelFormat=MTLPixelFormatBGRA8Unorm;layer_.framebufferOnly=NO;layer_.opaque=!overlay;layer_.drawableSize=CGSizeMake(std::round(view.width*view.dpr),std::round(view.height*view.dpr));
  GrMtlBackendContext backend;backend.fDevice=sk_ret_cfp((__bridge GrMTLHandle)device_);backend.fQueue=sk_ret_cfp((__bridge GrMTLHandle)queue_);context_=GrDirectContexts::MakeMetal(backend);
  if(context_)create(static_cast<unsigned>(layer_.drawableSize.width),static_cast<unsigned>(layer_.drawableSize.height));
 }
 ~Surface()override{surface_.reset();if(context_)context_->releaseResourcesAndAbandonContext();}
 bool ready()const{return bool(surface_);}
 render::RenderTargetInfo describe()const noexcept override{return {profile_,render::RenderTargetKind::kGpuWindow,render::RenderTargetBackend::kMetal,render::RenderTargetFormat::kBgra8888,{view_.width,view_.height,static_cast<unsigned>(std::round(view_.width*view_.dpr)),static_cast<unsigned>(std::round(view_.height*view_.dpr)),view_.dpr,1},{true,false,false,true,true,true}};}
 render::SkiaSurfaceAcquireResult acquire()noexcept override{
  if(std::this_thread::get_id()!=thread_||!surface_||lost_)return render::SkiaSurfaceAcquireResult::rejected(render::SkiaSurfaceAcquireCode::kLost,"Metal binding unavailable");
  return render::SkiaSurfaceAcquireResult::acquired({surface_.get(),generation_});
 }
 void release()noexcept override{}
 render::BackendSubmissionResult lose()noexcept override{lost_=true;surface_.reset();texture_=nil;if(context_)context_->releaseResourcesAndAbandonContext();context_.reset();layer_.hidden=YES;return render::BackendSubmissionResult::accepted();}
 render::BackendSubmissionResult present()noexcept override{@autoreleasepool{
  if(std::this_thread::get_id()!=thread_||lost_||!surface_)return render::BackendSubmissionResult::rejected("Metal surface lost");
  id<CAMetalDrawable> drawable=[layer_ nextDrawable];if(!drawable)return render::BackendSubmissionResult::retry("Metal drawable unavailable");
  context_->flushAndSubmit(surface_.get(),GrSyncCpu::kNo);
  id<MTLCommandBuffer> command=[queue_ commandBuffer];id<MTLBlitCommandEncoder> blit=[command blitCommandEncoder];
  [blit copyFromTexture:texture_ sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0,0,0) sourceSize:MTLSizeMake(texture_.width,texture_.height,1) toTexture:drawable.texture destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0,0,0)];[blit endEncoding];[command presentDrawable:drawable];[command commit];[command waitUntilCompleted];
  if(command.status!=MTLCommandBufferStatusCompleted)return render::BackendSubmissionResult::rejected("Metal presentation failed");++presents_;return render::BackendSubmissionResult::accepted();
 }}
 render::BackendSubmissionResult activateBinding()noexcept override{layer_.hidden=overlay_&&!visible_;return render::BackendSubmissionResult::accepted();}
 void deactivateBinding()noexcept override{layer_.hidden=YES;}
 render::BackendSubmissionResult resize(unsigned w,unsigned h)noexcept override{if(ready()&&w==static_cast<unsigned>(layer_.drawableSize.width)&&h==static_cast<unsigned>(layer_.drawableSize.height))return render::BackendSubmissionResult::accepted();return render::BackendSubmissionResult::rejected("recreate Metal provider for resize");}
 render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t> out)noexcept override{const auto d=describe();if(!surface_||out.size()!=std::size_t(d.metrics.physicalWidth)*d.metrics.physicalHeight*4)return render::BackendSubmissionResult::rejected("readback size");const auto info=SkImageInfo::Make(static_cast<int>(d.metrics.physicalWidth),static_cast<int>(d.metrics.physicalHeight),kRGBA_8888_SkColorType,kPremul_SkAlphaType);if(!surface_->readPixels(info,out.data(),std::size_t(d.metrics.physicalWidth)*4,0,0))return render::BackendSubmissionResult::rejected("Metal readback failed");++readbacks_;return render::BackendSubmissionResult::accepted();}
 std::uint64_t generation()const noexcept override{return generation_;}
 render::BackendSubmissionResult advanceGeneration()noexcept override{if(generation_==UINT64_MAX)return render::BackendSubmissionResult::rejected("generation exhausted");++generation_;return render::BackendSubmissionResult::accepted();}
 std::uint64_t readbackCount()const noexcept override{return readbacks_;}std::uint64_t cpuCopyCount()const noexcept override{return readbacks_;}std::uint64_t presentCount()const noexcept override{return presents_;}
 void setOverlayVisible(bool v)noexcept override{visible_=v;if(overlay_)layer_.hidden=!v;}
 bool overlayVisible()const noexcept override{return visible_;}
};
}
NativeSurfacePair makeMetalSurfaces(const NativeView& view){
 auto canonical=std::make_unique<Surface>((__bridge CAMetalLayer*)view.canonical,view,false);
 auto preview=std::make_unique<Surface>((__bridge CAMetalLayer*)view.preview,view,true);
 if(!canonical->ready()||!preview->ready())return {};
 return {std::move(canonical),std::move(preview),"native-metal-canonical","native-metal-preview"};
}
}

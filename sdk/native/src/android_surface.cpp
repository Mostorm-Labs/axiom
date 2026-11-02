#include "android_surface.hpp"
#include <android/native_window.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include "include/core/SkColorSpace.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkSurface.h"
#include "include/gpu/ganesh/GrBackendSurface.h"
#include "include/gpu/ganesh/GrDirectContext.h"
#include "include/gpu/ganesh/SkSurfaceGanesh.h"
#include "include/gpu/ganesh/gl/GrGLBackendSurface.h"
#include "include/gpu/ganesh/gl/GrGLDirectContext.h"
#include "include/gpu/ganesh/gl/GrGLInterface.h"
#include <cmath>
#include <thread>

namespace canvas::sdk {
namespace {
class Surface final:public render::SkiaSurfaceProvider {
 ANativeWindow* window_;
 EGLDisplay display_=EGL_NO_DISPLAY;EGLContext egl_=EGL_NO_CONTEXT;EGLSurface target_=EGL_NO_SURFACE;
 sk_sp<GrDirectContext> context_;sk_sp<SkSurface> surface_,windowSurface_;
 NativeView view_;std::string profile_;bool lost_=false,visible_=true,overlay_=false;
 std::uint64_t generation_=1,presents_=0;
 const std::thread::id thread_=std::this_thread::get_id();
 bool composite()noexcept{
  if(lost_||!ready()||!current())return false;
  auto* canvas=windowSurface_->getCanvas();canvas->clear(SK_ColorTRANSPARENT);
  if(!overlay_||visible_)canvas->drawImage(surface_->makeImageSnapshot(),0,0);
  context_->flushAndSubmit(windowSurface_.get(),GrSyncCpu::kNo);
  if(!eglSwapBuffers(display_,target_)){lost_=true;return false;}
  ++presents_;return true;
 }
 bool current()const{return std::this_thread::get_id()==thread_&&display_!=EGL_NO_DISPLAY&&eglMakeCurrent(display_,target_,target_,egl_)==EGL_TRUE;}
public:
 Surface(ANativeWindow* window,NativeView view,bool preview):window_(window),view_(view),profile_(preview?"native-gles-preview":"native-gles-canonical"),overlay_(preview){
  if(!window_)return;ANativeWindow_acquire(window_);display_=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(display_==EGL_NO_DISPLAY||!eglInitialize(display_,nullptr,nullptr))return;
  const EGLint attributes[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_STENCIL_SIZE,8,EGL_NONE};
  EGLConfig config;EGLint count=0;if(!eglChooseConfig(display_,attributes,&config,1,&count)||count!=1)return;
  EGLint format=0;eglGetConfigAttrib(display_,config,EGL_NATIVE_VISUAL_ID,&format);ANativeWindow_setBuffersGeometry(window_,int(std::round(view.width*view.dpr)),int(std::round(view.height*view.dpr)),format);
  const EGLint contextAttributes[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};egl_=eglCreateContext(display_,config,EGL_NO_CONTEXT,contextAttributes);target_=eglCreateWindowSurface(display_,config,window_,nullptr);
  if(egl_==EGL_NO_CONTEXT||target_==EGL_NO_SURFACE||!current())return;
  context_=GrDirectContexts::MakeGL(GrGLMakeNativeInterface());if(!context_)return;
  GrGLFramebufferInfo info{0,GL_RGBA8};const auto target=GrBackendRenderTargets::MakeGL(int(std::round(view.width*view.dpr)),int(std::round(view.height*view.dpr)),1,8,info);
  windowSurface_=SkSurfaces::WrapBackendRenderTarget(context_.get(),target,kBottomLeft_GrSurfaceOrigin,kRGBA_8888_SkColorType,SkColorSpace::MakeSRGB(),nullptr);
  // Retain GPU content independently of the window back buffer. Overlay hide
  // presents transparent pixels; show restores exactly the retained preview.
  surface_=SkSurfaces::RenderTarget(context_.get(),skgpu::Budgeted::kYes,SkImageInfo::MakeN32Premul(int(std::round(view.width*view.dpr)),int(std::round(view.height*view.dpr))));
  if(surface_)surface_->getCanvas()->clear(SK_ColorTRANSPARENT);
 }
 ~Surface()override{if(current()){surface_.reset();windowSurface_.reset();if(context_)context_->releaseResourcesAndAbandonContext();context_.reset();}else{surface_.reset();windowSurface_.reset();if(context_)context_->abandonContext();context_.reset();}if(display_!=EGL_NO_DISPLAY){eglMakeCurrent(display_,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(target_!=EGL_NO_SURFACE)eglDestroySurface(display_,target_);if(egl_!=EGL_NO_CONTEXT)eglDestroyContext(display_,egl_);}if(window_)ANativeWindow_release(window_);}
 bool ready()const{return bool(surface_)&&bool(windowSurface_);}
 render::RenderTargetInfo describe()const noexcept override{return {profile_,render::RenderTargetKind::kGpuWindow,render::RenderTargetBackend::kOpenGL,render::RenderTargetFormat::kRgba8888,{view_.width,view_.height,unsigned(std::round(view_.width*view_.dpr)),unsigned(std::round(view_.height*view_.dpr)),view_.dpr,1},{true,false,false,false,true,true}};}
 render::SkiaSurfaceAcquireResult acquire()noexcept override{if(lost_||!ready()||!current())return render::SkiaSurfaceAcquireResult::rejected(render::SkiaSurfaceAcquireCode::kLost,"EGL surface lost");return render::SkiaSurfaceAcquireResult::acquired({surface_.get(),generation_});}
 void release()noexcept override{}
 render::BackendSubmissionResult present()noexcept override{return composite()?render::BackendSubmissionResult::accepted():render::BackendSubmissionResult::rejected("EGL presentation unavailable");}
 render::BackendSubmissionResult lose()noexcept override{if(!current())return render::BackendSubmissionResult::rejected("EGL unavailable");surface_.reset();windowSurface_.reset();if(context_)context_->releaseResourcesAndAbandonContext();context_.reset();lost_=true;return render::BackendSubmissionResult::accepted();}
 render::BackendSubmissionResult resize(unsigned w,unsigned h)noexcept override{const auto m=describe().metrics;return ready()&&m.physicalWidth==w&&m.physicalHeight==h?render::BackendSubmissionResult::accepted():render::BackendSubmissionResult::rejected("recreate EGL provider for resize");}
 render::BackendSubmissionResult readbackRgba(std::span<std::uint8_t>)noexcept override{return render::BackendSubmissionResult::rejected("EGL readback unavailable");}
 std::uint64_t generation()const noexcept override{return generation_;}render::BackendSubmissionResult advanceGeneration()noexcept override{if(generation_==UINT64_MAX)return render::BackendSubmissionResult::rejected("generation exhausted");++generation_;return render::BackendSubmissionResult::accepted();}
 std::uint64_t readbackCount()const noexcept override{return 0;}std::uint64_t cpuCopyCount()const noexcept override{return 0;}std::uint64_t presentCount()const noexcept override{return presents_;}
 void setOverlayVisible(bool v)noexcept override{if(visible_==v)return;visible_=v;if(overlay_)(void)composite();}bool overlayVisible()const noexcept override{return visible_;}
};
}
NativeSurfacePair makeAndroidSurfaces(const NativeView& view){auto canonical=std::make_unique<Surface>(static_cast<ANativeWindow*>(view.canonical),view,false);auto preview=std::make_unique<Surface>(static_cast<ANativeWindow*>(view.preview),view,true);if(!canonical->ready()||!preview->ready())return {};return {std::move(canonical),std::move(preview),"native-gles-canonical","native-gles-preview"};}
}

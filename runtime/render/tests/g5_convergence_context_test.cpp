// Real renderer release ordering. A SkSurface release callback independently
// observes the current provider identity; no product-private state is exposed.
#include "canvas/render/skia_renderer.hpp"
#include "../src/transient_display_state.hpp"
#include "include/core/SkSurface.h"
#include <cassert>
#include <vector>
#include <iostream>
using namespace canvas;using namespace canvas::render;
namespace {
int currentContext=1;unsigned releases=0,foreignDeletes=0;
class ContextTarget final:public SkiaSurfaceProvider {
 RasterSkiaSurfaceProvider actual_;
 public:
 bool unavailable=false;bool stale=false;unsigned acquired=0,released=0;
 ContextTarget(){assert(actual_.resize(64,64).code==BackendSubmissionCode::kAccepted);}
 RenderTargetInfo describe()const noexcept override{return actual_.describe();}
 SkiaSurfaceAcquireResult acquire()noexcept override{if(unavailable)return {};currentContext=1;++acquired;auto result=actual_.acquire();if(stale)++result.frame.generation;return result;}
 void release()noexcept override{++released;actual_.release();}
 BackendSubmissionResult present()noexcept override{return actual_.present();}
 BackendSubmissionResult resize(std::uint32_t w,std::uint32_t h)noexcept override{return actual_.resize(w,h);}
 BackendSubmissionResult readbackRgba(std::span<std::uint8_t>o)noexcept override{return actual_.readbackRgba(o);}
 std::uint64_t generation()const noexcept override{return actual_.generation();}
 BackendSubmissionResult advanceGeneration()noexcept override{return actual_.advanceGeneration();}
 std::uint64_t readbackCount()const noexcept override{return actual_.readbackCount();}
 std::uint64_t cpuCopyCount()const noexcept override{return actual_.cpuCopyCount();}
 std::uint64_t presentCount()const noexcept override{return actual_.presentCount();}
};
FramePlan plan(){
 FrameState f{ViewId{1},CameraState{{32,32},1,0,CameraGeneration{1}},{0,0,64,64},{64,64,64,64,1,1},semantic::SemanticGeneration{1},SceneRevision{1},SurfaceGeneration{1},MetricsGeneration{1},FrameId{1},{0,0,64,64}};
 ReferenceDrawList list{f,{},0,0,{},{},{},{},ReferenceDrawList::kCanonicalEncodingVersion,CanonicalByteOrder::kLittleEndian,{},{}};list.viewportClip=f.viewportClip;
 return FramePlan{f,std::move(list),{},false,true};
}
void retainedAttemptRelease(unsigned route){
 ContextTarget target;SkiaRenderer renderer;const auto frame=plan();assert(renderer.renderFrame(target,frame).code==BackendSubmissionCode::kAccepted);EditingOverlay overlay{frame.frame};assert(renderer.renderTransientOverlay(target,frame,&overlay).code==BackendSubmissionCode::kAccepted);
 auto attempt=internal::transientDisplayAttempt.lock();assert(attempt);auto pixels=new std::vector<std::uint8_t>(64*64*4);
 attempt->surface=SkSurfaces::WrapPixels(SkImageInfo::MakeN32Premul(64,64),pixels->data(),64*4,[](void*,void* raw){++releases;if(currentContext!=1)++foreignDeletes;delete static_cast<std::vector<std::uint8_t>*>(raw);},pixels);
 attempt.reset();currentContext=2;
 auto invoke=[&]{if(route==0)return renderer.renderFrame(target,frame);
 if(route==1)return renderer.renderNavigation(target,{1,0,0},frame.frame);
 NavigationSourceQuery query=[](const foundation::WorldRect&,std::vector<ReferenceTraversalEntry>&){return true;};return renderer.renderNavigation(target,{1,0,0},frame.frame,query);};
 const auto before=releases;target.unavailable=true;(void)invoke();assert(releases==before&&"failed activation must preserve old resources");target.unavailable=false;
 target.stale=true;(void)invoke();assert(releases==before&&"stale target must preserve old resources");target.stale=false;
 (void)invoke();assert(target.acquired==target.released);
 std::cout<<"route="<<route<<" releases="<<releases<<" foreign="<<foreignDeletes<<'\n';assert(releases==route+1);assert(foreignDeletes==0&&"renderer released canonical-owned attempt while preview context was current");
}
}
int main(){for(unsigned route=0;route<3;++route)retainedAttemptRelease(route);}

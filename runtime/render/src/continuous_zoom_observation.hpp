#pragma once
// Private optical/native verification only. Normal consumers compile out all
// state and calls. No SDK export, charged-class field, or Runtime decision.
#if defined(AXIOM_CONTINUOUS_ZOOM_OBSERVATION)
#include "canvas/render/frame_state.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include <functional>
#include <cstdio>
namespace canvas::render::internal::zoom_observation {
struct Quality {double total=0,low=0,severe=0,debt=0,maximum=0;double contentTotal=0,contentLow=0,contentSevere=0,contentDebt=0,contentMaximum=0;};
inline thread_local std::function<double(float,float,float,float)> contentIntersection;
inline void contentPatch(float l,float t,float r,float b,double upscale);
struct Present {const void* target;const FrameState* frame;CanonicalViewportTransform camera;std::uint64_t resource=0,sequence=0;const char* role;Quality quality;std::size_t used=0,pending=0,staging=0;};
inline thread_local Quality current;
inline thread_local std::function<void(const Present&)> observer;
inline thread_local std::uint64_t sequence=0;
inline void begin(){current={};}
inline void patch(double area,double upscale){current.total+=area;current.low+=upscale>1.25?area:0;current.severe+=upscale>4?area:0;current.debt+=area*std::max(0.,upscale/1.25-1);current.maximum=std::max(current.maximum,upscale);}
inline void contentPatch(float l,float t,float r,float b,double upscale){if(!contentIntersection)return;double a=contentIntersection(l,t,r,b);current.contentTotal+=a;current.contentLow+=upscale>1.25?a:0;current.contentSevere+=upscale>4?a:0;current.contentDebt+=a*std::max(0.,upscale/1.25-1);if(a>0)current.contentMaximum=std::max(current.contentMaximum,upscale);}
inline void present(const void*target,const FrameState&f,CanonicalViewportTransform c,std::uint64_t resource,const char*role,const std::shared_ptr<resources::ResourceBudgetManager>&b,bool nav){
 Quality q=nav?current:Quality{1,0,0,0,1};if(q.total){q.low/=q.total;q.severe/=q.total;q.debt/=q.total;}if(q.contentTotal){q.contentLow/=q.contentTotal;q.contentSevere/=q.contentTotal;q.contentDebt/=q.contentTotal;}
 Present p{target,&f,c,resource,++sequence,role,q,b?b->used():0,0,0};if(b)for(unsigned i=0;i<unsigned(resources::ResourceClass::kCount);++i){auto s=b->snapshot(resources::ResourceClass(i));p.pending+=s.pendingRelease;p.staging+=s.staging;}
 if(observer){observer(p);return;}
 std::fprintf(stderr,"AXIOM_ZQ_PRESENT {\"view\":\"%llu\",\"semantic\":\"%llu\",\"scene\":\"%llu\",\"resource\":\"%llu\",\"surface\":\"%llu\",\"metrics\":\"%llu\",\"cameraGeneration\":\"%llu\",\"frame\":\"%llu\",\"sequence\":\"%llu\",\"target\":\"%llu\",\"role\":\"%s\",\"affine\":[%.17g,0,0,%.17g,%.17g,%.17g],\"dimensions\":[%u,%u],\"upscale\":%.17g,\"low\":%.17g,\"severe\":%.17g,\"debt\":%.17g,\"used\":%zu,\"pending\":%zu,\"staging\":%zu}\n",(unsigned long long)f.viewId.value(),(unsigned long long)f.sceneGeneration.value(),(unsigned long long)f.sceneReadToken.value(),(unsigned long long)resource,(unsigned long long)f.surfaceGeneration.value(),(unsigned long long)f.metricsGeneration.value(),(unsigned long long)f.camera.generation.value(),(unsigned long long)f.frameId.value(),(unsigned long long)p.sequence,(unsigned long long)reinterpret_cast<std::uintptr_t>(target),role,c.scale,c.scale,c.translationX,c.translationY,f.metrics.physicalWidth,f.metrics.physicalHeight,q.maximum,q.low,q.severe,q.debt,p.used,p.pending,p.staging);
}
}
#define ZQ_BEGIN() ::canvas::render::internal::zoom_observation::begin()
#define ZQ_CONTENT_PATCH(r,u) ::canvas::render::internal::zoom_observation::contentPatch(r.left,r.top,r.right,r.bottom,u)
#define ZQ_PATCH(a,u) ::canvas::render::internal::zoom_observation::patch(a,u)
#define ZQ_PRESENT(t,f,c,r,role,b,nav) ::canvas::render::internal::zoom_observation::present(t,f,c,r,role,b,nav)
#else
#define ZQ_BEGIN() ((void)0)
#define ZQ_CONTENT_PATCH(...) ((void)0)
#define ZQ_PATCH(...) ((void)0)
#define ZQ_PRESENT(...) ((void)0)
#endif

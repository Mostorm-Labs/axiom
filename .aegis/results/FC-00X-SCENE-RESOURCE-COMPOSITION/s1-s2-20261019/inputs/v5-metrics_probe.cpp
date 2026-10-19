#include "web_surface_host.hpp"
#include <cassert>
#include <cstdio>
using namespace canvas;
int main(){
  std::setvbuf(stdout,nullptr,_IONBF,0);
  web_demo::WebSurfaceHost host({render::ViewId{301},render::SurfaceGeneration{1},render::MetricsGeneration{1},{100,80,200,160,2,1}});
  auto print=[&](const char* event){const auto s=host.composition().surfaceSnapshot();const auto p=host.lifecycle().current();
    std::printf("%s Runtime logical=%.0fx%.0f physical=%ux%u dpr=%.1f generation=%llu; platform logical=%.0fx%.0f physical=%ux%u dpr=%.1f generation=%llu\n",event,s.logicalWidth,s.logicalHeight,s.width,s.height,s.devicePixelRatio,s.surfaceGeneration,p.metrics.logicalWidth,p.metrics.logicalHeight,p.metrics.physicalWidth,p.metrics.physicalHeight,p.metrics.devicePixelRatio,p.surfaceGeneration.value());
  };
  print("initial");auto old=host.lifecycle().current();
  const auto result=host.bind({old.viewId,render::SurfaceGeneration{old.surfaceGeneration.value()+1},render::MetricsGeneration{old.metricsGeneration.value()+1},{100,80,200,160,2,1}});
  assert(result==web_demo::WebSurfaceDisposition::kRebound);print("rebind");
  const auto r=host.composition().surfaceSnapshot();const auto p=host.lifecycle().current();
  assert(r.logicalWidth!=p.metrics.logicalWidth&&r.devicePixelRatio!=p.metrics.devicePixelRatio);
  std::puts("OBSERVED: accepted platform metrics and Runtime metrics differ; this is not a PASS oracle.");
}

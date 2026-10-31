#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include <nlohmann/json.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <stdexcept>

using namespace canvas;
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
int main(int argc,char** argv){
  try{
    const auto count=argc>1?std::stoul(argv[1]):128;
    const auto profile=argc>2?argv[2]:"round-handdrawn";
    const bool offscreen=argc>3&&std::string(argv[3])=="offscreen";
    interaction::CanvasEditingRuntime runtime;
    require(runtime.bindSurface(800,500)&&runtime.registerInputSource(71,"g5-accumulation")&&runtime.selectBrushProfile(profile,1),"prepare");
    runtime.setPlatformPresentationDeferred(true);
    nlohmann::json runs=nlohmann::json::array();
    std::uint64_t sequence=0;
    for(unsigned n=0;n<count;++n){
      nlohmann::json moves=nlohmann::json::array();
      double down=0,up=0;
      for(unsigned i=0;i<16;++i){
        input::PlatformPointerSample s;s.source=71;s.pointer=1;s.sequence=++sequence;s.timestampNs=sequence*4'000'000;
        s.x=30+float(i)*12;s.y=(offscreen&&n+1<count?10000.F:40.F)+float(n%16)*22+float(std::sin(i*.4))*5;
        s.phase=i==0?input::PointerPhase::kDown:i==15?input::PointerPhase::kUp:input::PointerPhase::kMove;
        auto start=Clock::now();const auto ok=runtime.acceptPlatformBatch({{s}},s.timestampNs);const auto ms=elapsed(start);require(ok,"input");
        if(i==0)down=ms;else if(i==15)up=ms;else moves.push_back(ms);
      }
      require(runtime.semanticObjectCount()==n+1,"lost commit");
      auto start=Clock::now();require(runtime.presentCanonicalFrame(runtime.nextCanonicalFrameId(),0),"canonical present");
      auto render=elapsed(start);
      runs.push_back({{"objects",n+1},{"down_ms",down},{"move_ms",moves},{"up_commit_scene_ms",up},{"frame_plan_skia_present_ms",render}});
    }
    std::puts(nlohmann::json{{"profile",profile},{"offscreen",offscreen},{"scope","shared Runtime native raster; not physical latency"},{"runs",runs}}.dump(2).c_str());
    return 0;
  }catch(const std::exception& e){std::fprintf(stderr,"%s\n",e.what());return 2;}
}

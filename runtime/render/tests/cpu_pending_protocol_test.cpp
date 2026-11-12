#include "../src/cpu_worker_protocol.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include <stdexcept>
#include <iostream>
using namespace canvas;using namespace canvas::render::internal;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(){try{CpuRequest r;r.identity.words={1,2,0,3,4,5,6,7};std::vector<foundation::WorldPoint> trace;
 for(unsigned i=0;i<=8192;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/8192)});
 r.geometry=*interaction::partialEraseGeometry(trace,{1,0,0,1,0,0},16);auto b=cpuEncodeRequest(r);need(b.size()==655488,"8192Sweep must fit1MiB unchanged");auto d=cpuDecodeRequest(b);need(d&&d->geometry==r.geometry&&d->identity==r.identity,"8192losslessroundtrip");
 std::cout<<"8192Sweep655488bounded\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

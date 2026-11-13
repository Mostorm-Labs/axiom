#include "../src/cpu_worker_protocol.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace canvas;using namespace canvas::render::internal;
void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
int main(int argc,char**argv){try{
 need(argc==2,"output");std::vector<foundation::WorldPoint> trace;for(unsigned i=0;i<=7200;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/7200)});
 auto geometry=interaction::partialEraseGeometry(trace,{1,0,0,1,0,0},16);need(bool(geometry),"geometry");CpuRequest request{{{1,1,0,1,1,1,1,1}},*geometry};auto wire=cpuEncodeRequest(request);need(wire.size()==576128,"7200wire");
 CpuExactPathJob job(*geometry);unsigned turns=0;while(!job.ready()){need(++turns<100000&&job.advance(8,2000),"bounded advance");}
 // Independent original monolithic f32 segment stroking/sequential leftfold.
 SkPath original;for(const auto& s:std::get<semantic::SweptCircleMask>(*geometry).segments){SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));p.setStrokeCap(SkPaint::kRound_Cap);p.setStrokeJoin(SkPaint::kRound_Join);auto stroked=skpathutils::FillPathWithPaint(b.detach(),p);auto next=Op(original,stroked,kUnion_SkPathOp);need(bool(next),"original union");original=*next;}
 need(original==job.path(),"7200 original path mismatch");std::vector<std::uint8_t> bytes(original.writeToMemory(nullptr));original.writeToMemory(bytes.data());std::ofstream(argv[1],std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
 std::cout<<"{\"segments\":7200,\"wire_bytes\":"<<wire.size()<<",\"turns\":"<<turns<<",\"path_bytes\":"<<bytes.size()<<",\"estimate\":"<<*cpuConstructionEstimate(7200,wire.size(),true)<<",\"equal\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

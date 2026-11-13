// Expected values use the independently pinned pre-Worker implementation.
#include "../src/cpu_exact_path_kernel.hpp"
#include "../src/cpu_worker_protocol.hpp"
#include "../../interaction/src/partial_erase_geometry.hpp"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
#include <cmath>
#include <stdexcept>
#include <iostream>
#include <filesystem>
#include <fstream>
using namespace canvas;
void require(bool x,const char*m){if(!x)throw std::runtime_error(m);}
SkPath original(const semantic::EraseMaskGeometry& geometry){
 SkPath out;
 if(const auto*fill=std::get_if<semantic::FilledPathMask>(&geometry)){
  SkPathBuilder p(fill->path.fill_rule==semantic::FillRule::kEvenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding);
  for(const auto&cmd:fill->path.commands)std::visit([&](const auto&v){using T=std::decay_t<decltype(v)>;if constexpr(std::is_same_v<T,semantic::MoveTo>)p.moveTo(float(v.point.x),float(v.point.y));else if constexpr(std::is_same_v<T,semantic::LineTo>)p.lineTo(float(v.end.x),float(v.end.y));else if constexpr(std::is_same_v<T,semantic::QuadTo>)p.quadTo(float(v.control.x),float(v.control.y),float(v.end.x),float(v.end.y));else if constexpr(std::is_same_v<T,semantic::CubicTo>)p.cubicTo(float(v.control1.x),float(v.control1.y),float(v.control2.x),float(v.control2.y),float(v.end.x),float(v.end.y));else p.close();},cmd);
  return p.detach();
 }
 if(const auto*g=std::get_if<semantic::SweptCircleMask>(&geometry))for(const auto&s:g->segments){
  SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));
  SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));p.setStrokeCap(SkPaint::kRound_Cap);p.setStrokeJoin(SkPaint::kRound_Join);
  auto part=skpathutils::FillPathWithPaint(b.detach(),p);auto next=Op(out,part,kUnion_SkPathOp);require(bool(next),"independent original Op");out=*next;
 }
 return out;
}
int main(int argc,char**argv){try{
 std::vector<foundation::WorldPoint> trace{{420,160}};for(unsigned i=1;i<=1600;++i)trace.push_back({float(420+7*std::sin(i*.17)),float(160+170.*i/1600)});
 for(unsigned local=0;local<16;++local){const auto mask=interaction::partialEraseGeometry(trace,{1,0,0,1,local*.125,local*.25},16);require(bool(mask),"local geometry");const auto expected=original(*mask);const auto actual=render::internal::cpuExactPath(*mask);require(actual&&*actual==expected,"shared monolithic changed original numeric path");
  render::internal::CpuExactPathJob job{*mask};for(unsigned step=0;!job.ready();++step){require(step<2048,"job progress");require(job.advance(3,2000),"job advance");}require(job.path()==expected,"resumable path differs original");
  if(argc==2){std::filesystem::create_directories(argv[1]);const auto prefix=std::filesystem::path(argv[1])/std::to_string(local);render::internal::CpuRequest request;request.identity.words={1,1,1,0,1,local+1,1,1};request.geometry=*mask;auto input=render::internal::cpuEncodeRequest(request);std::ofstream a(prefix.string()+".request",std::ios::binary);a.write(reinterpret_cast<const char*>(input.data()),input.size());std::vector<std::uint8_t>bytes(expected.writeToMemory(nullptr));expected.writeToMemory(bytes.data());std::ofstream b(prefix.string()+".path",std::ios::binary);b.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());}
 }
 if(argc==2){
  const auto mask=*interaction::partialEraseGeometry(trace,{1,0,0,1,0,0},16);
  auto append=mask;std::get<semantic::SweptCircleMask>(append).segments.resize(800);
  auto fork=mask;std::get<semantic::SweptCircleMask>(fork).segments[799].control1.x+=.25;
  semantic::FilledPathMask fill;fill.path.fill_rule=semantic::FillRule::kEvenOdd;fill.path.commands={semantic::MoveTo{{0,0}},semantic::LineTo{{50,0}},semantic::LineTo{{50,50}},semantic::ClosePath{}};
  std::array<semantic::EraseMaskGeometry,3> extra{append,fork,fill};
  for(unsigned i=0;i<3;++i){const auto n=16+i;render::internal::CpuRequest r;r.identity.words={1,1,1,0,1,n+1,1,1};r.geometry=extra[i];const auto bytes=render::internal::cpuEncodeRequest(r);const auto path=original(extra[i]);const auto candidate=render::internal::cpuExactPath(extra[i]);require(candidate&&*candidate==path,"independent extra reference");const auto prefix=std::filesystem::path(argv[1])/std::to_string(n);std::ofstream a(prefix.string()+".request",std::ios::binary);a.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());std::vector<std::uint8_t>data(path.writeToMemory(nullptr));path.writeToMemory(data.data());std::ofstream b(prefix.string()+".path",std::ios::binary);b.write(reinterpret_cast<const char*>(data.data()),data.size());}
 }
 std::cout<<"{\"original_path_parity\":true,\"local_queries\":16,\"segments\":1600}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}

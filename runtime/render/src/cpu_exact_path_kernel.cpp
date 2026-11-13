#include "cpu_exact_path_kernel.hpp"
#include "cpu_history_observer.hpp"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathUtils.h"
#include "include/pathops/SkPathOps.h"
#include <algorithm>
#include <type_traits>
namespace canvas::render::internal {
namespace {
void appendCommand(SkPathBuilder& path,const semantic::PathCommand& command){
 std::visit([&](const auto& v){using T=std::decay_t<decltype(v)>;
  if constexpr(std::is_same_v<T,semantic::MoveTo>)path.moveTo(float(v.point.x),float(v.point.y));
  else if constexpr(std::is_same_v<T,semantic::LineTo>)path.lineTo(float(v.end.x),float(v.end.y));
  else if constexpr(std::is_same_v<T,semantic::QuadTo>)path.quadTo(float(v.control.x),float(v.control.y),float(v.end.x),float(v.end.y));
  else if constexpr(std::is_same_v<T,semantic::CubicTo>)path.cubicTo(float(v.control1.x),float(v.control1.y),float(v.control2.x),float(v.control2.y),float(v.end.x),float(v.end.y));
  else path.close();
 },command);
}
}
bool cpuExactUnionSegment(SkPath& accumulated,const semantic::EraseCubicSegment& s){
 SkPathBuilder b;b.moveTo(float(s.p0.position.x),float(s.p0.position.y));
 b.cubicTo(float(s.control1.x),float(s.control1.y),float(s.control2.x),float(s.control2.y),float(s.p1.position.x),float(s.p1.position.y));
 SkPaint p;p.setStyle(SkPaint::kStroke_Style);p.setStrokeWidth(float(2*std::max(s.p0.radius,s.p1.radius)));
 p.setStrokeCap(SkPaint::kRound_Cap);p.setStrokeJoin(SkPaint::kRound_Join);
 const auto stroke=skpathutils::FillPathWithPaint(b.detach(),p);
 auto next=Op(accumulated,stroke,kUnion_SkPathOp);if(!next)return false;
 accumulated=*next;return true;
}
SkPath cpuExactFilledPath(const semantic::VectorPathGeometry& geometry){
 SkPathBuilder path(geometry.fill_rule==semantic::FillRule::kEvenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding);
 for(const auto& command:geometry.commands)appendCommand(path,command);
 return path.detach();
}
std::optional<SkPath> cpuExactPath(const semantic::EraseMaskGeometry& geometry,std::uint64_t* unions){
#if defined(AXIOM_UR2D_TESTS)
 const auto began=std::chrono::steady_clock::now();std::size_t done=0;
 struct FullObservation {const semantic::EraseMaskGeometry& geometry;std::size_t& done;std::chrono::steady_clock::time_point began;
 ~FullObservation(){observeCpuHistoryKernel({std::visit([](const auto& g)->std::size_t{if constexpr(requires{g.segments;})return g.segments.size();else return g.path.commands.size();},geometry),done,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-began).count())});}} observation{geometry,done,began};
#endif
 SkPath out;if(const auto* fill=std::get_if<semantic::FilledPathMask>(&geometry))return cpuExactFilledPath(fill->path);
 for(const auto& s:std::get<semantic::SweptCircleMask>(geometry).segments){if(unions)++*unions;
#if defined(AXIOM_UR2D_TESTS)
 ++done;
#endif
 if(!cpuExactUnionSegment(out,s))return {};}
 return out;
}
CpuExactPathJob::CpuExactPathJob(semantic::EraseMaskGeometry input):input_(std::move(input)),filled_(std::holds_alternative<semantic::FilledPathMask>(input_)&&std::get<semantic::FilledPathMask>(input_).path.fill_rule==semantic::FillRule::kEvenOdd?SkPathFillType::kEvenOdd:SkPathFillType::kWinding){}
CpuExactPathJob::CpuExactPathJob(semantic::EraseMaskGeometry input,std::size_t prefix,const SkPath& path)
 :CpuExactPathJob(std::move(input)){
 if(!prefix)return;
 const auto* sweep=std::get_if<semantic::SweptCircleMask>(&input_);
 if(!sweep||prefix>sweep->segments.size()||!path.isFinite()){failed_=true;return;}
 cursor_=prefix;path_=path;ready_=prefix==sweep->segments.size();
}
bool CpuExactPathJob::advance(std::size_t maxSteps,std::uint64_t maxMicroseconds){
 if(cancelled_||failed_)return false;if(ready_)return true;
 if(!maxSteps||!maxMicroseconds)return true;
 const auto begin=std::chrono::steady_clock::now();
 if(const auto* filled=std::get_if<semantic::FilledPathMask>(&input_)){
  for(std::size_t steps=0;cursor_<filled->path.commands.size()&&steps<maxSteps;++steps){
   if(std::chrono::steady_clock::now()-begin>=std::chrono::microseconds(maxMicroseconds))break;
   appendCommand(filled_,filled->path.commands[cursor_++]);
  }
  ready_=cursor_==filled->path.commands.size();if(ready_){path_=filled_.detach();failed_=!path_.isFinite();}return !failed_;
 }
 const auto& segments=std::get<semantic::SweptCircleMask>(input_).segments;
 for(std::size_t steps=0;cursor_<segments.size()&&steps<maxSteps;++steps){
  if(std::chrono::steady_clock::now()-begin>=std::chrono::microseconds(maxMicroseconds))break;
  const auto started=std::chrono::steady_clock::now();
  if(!cpuExactUnionSegment(path_,segments[cursor_])){failed_=true;return false;}
  maximum_=std::max(maximum_,std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-started).count()));++cursor_;
 }
 ready_=cursor_==segments.size();if(ready_&&!path_.isFinite()){failed_=true;return false;}return true;
}
}

#pragma once
// Private CPU-only exact geometry. No renderer, cache, document or GPU owner.
#include "canvas/semantic/erase_mask.hpp"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include <chrono>
#include <optional>
namespace canvas::render::internal {
bool cpuExactUnionSegment(SkPath&,const semantic::EraseCubicSegment&);
SkPath cpuExactFilledPath(const semantic::VectorPathGeometry&);
std::optional<SkPath> cpuExactPath(const semantic::EraseMaskGeometry&,std::uint64_t* unions=nullptr);
class CpuExactPathJob final {
 public:
 explicit CpuExactPathJob(semantic::EraseMaskGeometry input);
 CpuExactPathJob(semantic::EraseMaskGeometry input,std::size_t prefix,const SkPath& path);
 bool advance(std::size_t maxSteps,std::uint64_t maxMicroseconds);
 void cancel()noexcept{cancelled_=true;path_=SkPath{};}
 bool ready()const noexcept{return ready_;}
 bool failed()const noexcept{return failed_;}
 bool cancelled()const noexcept{return cancelled_;}
 std::size_t cursor()const noexcept{return cursor_;}
 std::uint64_t maxUnionNanoseconds()const noexcept{return maximum_;}
 const SkPath& path()const noexcept{return path_;}
 private:
 semantic::EraseMaskGeometry input_;SkPath path_;SkPathBuilder filled_;
 std::size_t cursor_=0;std::uint64_t maximum_=0;
 bool ready_=false,failed_=false,cancelled_=false;
};
}

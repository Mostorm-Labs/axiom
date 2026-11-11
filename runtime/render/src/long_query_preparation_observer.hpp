#pragma once
// Private bounded native observations; no consumer API or ordinary trace.
#if defined(AXIOM_UR2D_TESTS)
#include <cstddef>
#include <cstdint>
#include "include/core/SkPath.h"
#include "selection_region_geometry_internal.hpp"
namespace canvas::render::internal {
struct LongQueryPreparationObservation {
 std::uint64_t completed=0,compared=0,resumed=0,denied=0;
 std::size_t available=0,resident=0,steps=0;
 std::uint64_t elapsedNs=0,maxUnionNs=0;
 bool final=false;
 bool ownerInvocation=false;
};
using LongQueryPreparationObserver=void(*)(const LongQueryPreparationObservation&);
inline thread_local LongQueryPreparationObserver longQueryPreparationObserver=nullptr;
inline void observeLongQueryPreparation(const LongQueryPreparationObservation& v){if(longQueryPreparationObserver)longQueryPreparationObserver(v);}
SkPath longQueryPreparationPath(const std::shared_ptr<const EraseMaskPathCache::Entry>&);
}
#define LQP_OBSERVE(...) ::canvas::render::internal::observeLongQueryPreparation({__VA_ARGS__})
#else
#define LQP_OBSERVE(...) ((void)0)
#endif

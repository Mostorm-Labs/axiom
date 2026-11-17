#pragma once
// Private compile-time verification only. No release counters or SDK surface.
#if defined(AXIOM_ENABLE_FRAME_TIMING)
#include <cstdint>
namespace canvas::foundation::internal::reference_validation_metrics {
struct Counts {std::uint64_t bodyCalls=0,compactRequestedKnots=0,maskArrays=0,maskSegments=0,maskPathCommands=0,historySegments=0,historyPathCommands=0,activeSegments=0,activePathCommands=0,bodyHits=0,historyHits=0,misses=0,denials=0,evictions=0,identityProbes=0;};
inline thread_local Counts counts{};
}
#define AXIOM_VALIDATION_ADD(name,value) (::canvas::foundation::internal::reference_validation_metrics::counts.name+=(value))
#else
#define AXIOM_VALIDATION_ADD(name,value) ((void)0)
#endif

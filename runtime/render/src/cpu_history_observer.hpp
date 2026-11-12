#pragma once
// Private mechanism observation at the actual full-path kernel entry, so a
// cache-denial fallback cannot disappear behind cache-only work counters.
#include <cstddef>
#include <cstdint>
namespace canvas::render::internal {
struct CpuHistoryKernelObservation {std::size_t inputs=0,unions=0;std::uint64_t nanoseconds=0;};
#if defined(AXIOM_UR2D_TESTS)
using CpuHistoryKernelObserver=void(*)(CpuHistoryKernelObservation);
inline thread_local CpuHistoryKernelObserver cpuHistoryKernelObserver=nullptr;
inline void observeCpuHistoryKernel(CpuHistoryKernelObservation v){if(cpuHistoryKernelObserver)cpuHistoryKernelObserver(v);}
#else
inline void observeCpuHistoryKernel(CpuHistoryKernelObservation){}
#endif
}

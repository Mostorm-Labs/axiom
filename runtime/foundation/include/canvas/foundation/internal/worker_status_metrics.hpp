#pragma once
// Private verification counters. Normal builds evaluate no arguments and
// expose no accessor or payload. Counts observe actual copies/serialization.
#if defined(AXIOM_ENABLE_FRAME_TIMING)
#include <cstdint>
namespace canvas::foundation::internal::worker_status_metrics {
struct Counts {std::uint64_t facts=0,terminalRows=0,jobRows=0,jsonTerminalRows=0,jsonJobRows=0,status=0,terminalQueries=0,terminalCopies=0;};
inline thread_local Counts counts{};
}
#define AXIOM_WORKER_STATUS_ADD(name,value) (::canvas::foundation::internal::worker_status_metrics::counts.name+=(value))
#else
#define AXIOM_WORKER_STATUS_ADD(name,value) ((void)0)
#endif

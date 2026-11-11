#pragma once
// Bounded native qualification seam only. No consumer API or ordinary tracing.
#if defined(AXIOM_UR2D_TESTS)
#include "canvas/semantic/erase_mask.hpp"
#include <cstddef>
#include <cstdint>
namespace canvas::render::internal {
enum class HistoryPathEvent {kRequest,kEvict,kProofMiss,kBuild,kAdmit,kDeny,kConstruction,kTransfer};
struct HistoryPathObservation {
 HistoryPathEvent event;const void* entry=nullptr;const semantic::EraseMaskGeometry* geometry=nullptr;
 std::size_t local=0,estimate=0,actual=0,cardinality=0;bool historical=false;
};
using HistoryPathObserver=void(*)(const HistoryPathObservation&);
inline thread_local HistoryPathObserver historyPathObserver=nullptr;
inline void observeHistoryPath(const HistoryPathObservation& o){if(historyPathObserver)historyPathObserver(o);}
}
#define HP_OBSERVE(...) ::canvas::render::internal::observeHistoryPath({__VA_ARGS__})
#else
#define HP_OBSERVE(...) ((void)0)
#endif

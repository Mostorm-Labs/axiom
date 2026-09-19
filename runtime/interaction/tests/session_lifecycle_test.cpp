#include "canvas/interaction/session_manager.hpp"
#include <cassert>
namespace { struct T final : canvas::interaction::TransientPresentationPort { int calls=0; void cancel(std::uint64_t) noexcept override {++calls;} }; }
int main(){T t; canvas::interaction::InteractionSessionManager m(t); assert(m.start(1)); assert(m.suspend(1)); assert(m.activeCount()==0); assert(m.lastCancellationReason()==canvas::interaction::CancellationReason::kSurfaceSuspended); assert(t.calls==1); assert(!m.suspend(1)); assert(m.start(2)); assert(m.sourceLost(2)); assert(m.activeCount()==0); assert(m.lastCancellationReason()==canvas::interaction::CancellationReason::kSourceLost); assert(t.calls==2);}

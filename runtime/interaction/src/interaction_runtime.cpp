#include "canvas/interaction/interaction_runtime.hpp"

namespace canvas::interaction {

InteractionRuntime::InteractionRuntime(SemanticReadPort& read, SceneQueryPort& query,
                                       ViewStatePort& view, OperationSubmitPort& submit,
                                       TransientPresentationPort& transient) noexcept
    : editor_(read, query, view, submit), manager_(transient) {}

bool InteractionRuntime::startSession(std::uint64_t sessionId) noexcept {
    return editor_.canStart() && manager_.start(sessionId);
}

bool InteractionRuntime::commit(std::uint64_t sessionId, const OperationRequest& request) noexcept {
    if (manager_.activeCount() == 0 || !editor_.commit(request)) return false;
    return manager_.finish(sessionId);
}

void InteractionRuntime::documentDetached() noexcept { manager_.cancelAll(); }

} // namespace canvas::interaction

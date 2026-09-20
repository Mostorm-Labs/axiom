#include "canvas/interaction/interaction_runtime.hpp"

namespace canvas::interaction {

InteractionRuntime::InteractionRuntime(SemanticReadPort& read, SceneQueryPort& query,
                                       ViewStatePort& view, OperationSubmitPort& submit,
                                       TransientPresentationPort& transient) noexcept
    : editor_(read, query, view, submit), manager_(transient) {}

bool InteractionRuntime::startSession(std::uint64_t sessionId) noexcept {
    return editor_.canStart() && manager_.start(sessionId);
}

bool InteractionRuntime::startSession(const input::PointerKey& key, std::uint64_t sessionId) noexcept {
    return editor_.canStart() && manager_.start(key, sessionId);
}

bool InteractionRuntime::commit(std::uint64_t sessionId, const OperationRequest& request) noexcept {
    if (manager_.activeCount() == 0 || !editor_.commit(request)) return false;
    return manager_.finish(sessionId);
}

bool InteractionRuntime::commit(const input::PointerKey& key, std::uint64_t sessionId,
                                const OperationRequest& request) noexcept {
    if (!editor_.commit(request)) return false;
    return manager_.finish(key, sessionId);
}

void InteractionRuntime::documentDetached() noexcept {
    manager_.cancelAll(CancellationReason::kDocumentDetached);
    manager_.cancelAllKeyed(CancellationReason::kDocumentDetached);
}

void InteractionRuntime::cancelKeyedSessions(CancellationReason reason) noexcept {
    manager_.cancelAllKeyed(reason);
}

} // namespace canvas::interaction

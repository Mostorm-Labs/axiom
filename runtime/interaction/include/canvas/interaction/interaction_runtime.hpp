#pragma once

#include "canvas/interaction/editor_session.hpp"
#include "canvas/interaction/session_manager.hpp"

namespace canvas::interaction {

class InteractionRuntime final {
  public:
    InteractionRuntime(SemanticReadPort& read, SceneQueryPort& query, ViewStatePort& view,
                       OperationSubmitPort& submit, TransientPresentationPort& transient) noexcept;
    [[nodiscard]] bool startSession(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool commit(std::uint64_t sessionId, const OperationRequest& request) noexcept;
    void documentDetached() noexcept;
    [[nodiscard]] const InteractionSessionManager& manager() const noexcept { return manager_; }

  private:
    EditorSession editor_;
    InteractionSessionManager manager_;
};

} // namespace canvas::interaction

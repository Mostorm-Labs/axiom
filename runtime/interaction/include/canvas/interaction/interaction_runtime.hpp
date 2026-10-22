#pragma once

#include "canvas/interaction/editor_session.hpp"
#include "canvas/interaction/session_manager.hpp"
#include <memory>

namespace canvas::interaction {
class TextEditPort;
class TextEditSession;

class InteractionRuntime final {
  public:
    InteractionRuntime(SemanticReadPort& read, SceneQueryPort& query, ViewStatePort& view,
                       OperationSubmitPort& submit, TransientPresentationPort& transient, TextEditPort* textOwner = nullptr) noexcept;
    ~InteractionRuntime();
    [[nodiscard]] TextEditSession* textSession() noexcept { return text_.get(); }
    [[nodiscard]] bool startSession(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool startSession(const input::PointerKey& key, std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool commit(std::uint64_t sessionId, const OperationRequest& request) noexcept;
    [[nodiscard]] bool commit(const input::PointerKey& key, std::uint64_t sessionId,
                              const OperationRequest& request) noexcept;
    [[nodiscard]] bool cancel(const input::PointerKey& key, std::uint64_t sessionId) noexcept;
    void documentDetached() noexcept;
    void cancelKeyedSessions(CancellationReason reason) noexcept;
    [[nodiscard]] const InteractionSessionManager& manager() const noexcept { return manager_; }

  private:
    EditorSession editor_;
    InteractionSessionManager manager_;
    std::unique_ptr<TextEditSession> text_;
};

} // namespace canvas::interaction

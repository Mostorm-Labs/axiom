#include "canvas/interaction/editor_session.hpp"

namespace canvas::interaction {

EditorSession::EditorSession(SemanticReadPort& read, SceneQueryPort& query, ViewStatePort& view,
                             OperationSubmitPort& submit) noexcept
    : read_(read), query_(query), view_(view), submit_(submit) {}

bool EditorSession::canStart() const noexcept {
    return read_.attached() && query_.available() && view_.generation() != 0;
}

bool EditorSession::commit(const OperationRequest& request) noexcept {
    return submit_.submit(request).accepted;
}

} // namespace canvas::interaction

#pragma once

#include "canvas/interaction/interaction_ports.hpp"

#include <cstdint>

namespace canvas::interaction {

class EditorSession final {
  public:
    EditorSession(SemanticReadPort& read, SceneQueryPort& query, ViewStatePort& view,
                  OperationSubmitPort& submit) noexcept;
    [[nodiscard]] bool canStart() const noexcept;
    [[nodiscard]] bool commit(const OperationRequest& request) noexcept;

  private:
    SemanticReadPort& read_;
    SceneQueryPort& query_;
    ViewStatePort& view_;
    OperationSubmitPort& submit_;
};

} // namespace canvas::interaction

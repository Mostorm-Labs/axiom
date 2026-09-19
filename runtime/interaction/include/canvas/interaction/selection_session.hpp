#pragma once

#include "canvas/interaction/hit_test_service.hpp"
#include "canvas/interaction/selection_state.hpp"
#include "canvas/semantic/change_set.hpp"

#include <span>

namespace canvas::interaction {

class SelectionSession final {
  public:
    [[nodiscard]] bool click(foundation::ObjectId objectId);
    [[nodiscard]] bool toggle(foundation::ObjectId objectId);
    [[nodiscard]] bool marquee(std::span<const HitCandidate> candidates);
    void onChangeSet(const semantic::ChangeSet& changes);
    void cancel() noexcept;

    [[nodiscard]] std::span<const foundation::ObjectId> selected() const noexcept {
        return state_.selected();
    }
    [[nodiscard]] foundation::ObjectId primary() const noexcept { return state_.primary(); }
    [[nodiscard]] SelectionSummary summary() const noexcept { return state_.summary(); }

  private:
    HitTestService hitTest_;
    SelectionState state_;
};

} // namespace canvas::interaction

#pragma once

#include "canvas/interaction/hit_test_service.hpp"
#include "canvas/interaction/selection_state.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/object_store.hpp"

#include <span>

namespace canvas::interaction {

class SelectionSession final {
  public:
    [[nodiscard]] bool click(foundation::ObjectId objectId);
    [[nodiscard]] bool toggle(foundation::ObjectId objectId);
    [[nodiscard]] bool marquee(std::span<const HitCandidate> candidates);
    void onChangeSet(const semantic::ChangeSet& changes);
    void cancel() noexcept;
    [[nodiscard]] foundation::ObjectId resolveHit(foundation::ObjectId,const semantic::ObjectStore&) const;
    [[nodiscard]] bool enterGroupScope(foundation::ObjectId,const semantic::ObjectStore&);
    void leaveGroupScope(const semantic::ObjectStore&);
    [[nodiscard]] std::optional<foundation::ObjectId> groupScope() const noexcept { return groupScope_; }
    [[nodiscard]] std::uint64_t revision() const noexcept { return revision_; }

    [[nodiscard]] std::span<const foundation::ObjectId> selected() const noexcept {
        return state_.selected();
    }
    [[nodiscard]] foundation::ObjectId primary() const noexcept { return state_.primary(); }
    [[nodiscard]] SelectionSummary summary() const noexcept { return state_.summary(); }

  private:
    HitTestService hitTest_;
    SelectionState state_;
    std::optional<foundation::ObjectId> groupScope_;
    std::uint64_t revision_ = 1;
};

} // namespace canvas::interaction

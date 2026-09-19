#include "canvas/interaction/selection_session.hpp"

#include <algorithm>
#include <cstdint>

namespace canvas::interaction {

bool SelectionSession::click(foundation::ObjectId objectId) {
    if (objectId.isZero()) return false;
    state_.selected_ = {objectId};
    return true;
}

bool SelectionSession::toggle(foundation::ObjectId objectId) {
    if (objectId.isZero()) return false;
    const auto found = std::find(state_.selected_.begin(), state_.selected_.end(), objectId);
    if (found == state_.selected_.end()) {
        state_.selected_.push_back(objectId);
    } else {
        state_.selected_.erase(found);
    }
    return true;
}

bool SelectionSession::marquee(std::span<const HitCandidate> candidates) {
    state_.selected_ = hitTest_.selectable(candidates);
    return true;
}

void SelectionSession::onChangeSet(const semantic::ChangeSet& changes) {
    for (const semantic::ObjectSemanticChange& change : changes.objects()) {
        const auto flags = static_cast<std::uint8_t>(change.flags);
        const auto deleted = static_cast<std::uint8_t>(semantic::SemanticChangeFlags::kDeleted);
        if ((flags & deleted) == 0U) continue;
        state_.selected_.erase(
            std::remove(state_.selected_.begin(), state_.selected_.end(), change.object_id),
            state_.selected_.end());
    }
}

void SelectionSession::cancel() noexcept { state_.selected_.clear(); }

} // namespace canvas::interaction

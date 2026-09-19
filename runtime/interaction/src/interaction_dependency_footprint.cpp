#include "canvas/interaction/interaction_dependency_footprint.hpp"
#include <algorithm>
namespace canvas::interaction {
bool InteractionDependencyFootprint::track(foundation::ObjectId id) {
    if (id.isZero() || dependsOn(id)) return false;
    objects_.push_back(id);
    return true;
}
bool InteractionDependencyFootprint::dependsOn(foundation::ObjectId id) const noexcept {
    return std::find(objects_.begin(), objects_.end(), id) != objects_.end();
}
ConflictDecision InteractionDependencyFootprint::evaluate(const semantic::ChangeSet& changes,
                                                           CancellationReason* reason) const noexcept {
    if (reason != nullptr) *reason = CancellationReason::kNone;
    bool relevant = false;
    for (const auto& change : changes.objects()) {
        if (!dependsOn(change.object_id)) continue;
        const auto flags = static_cast<std::uint8_t>(change.flags);
        if ((flags & static_cast<std::uint8_t>(semantic::SemanticChangeFlags::kDeleted)) != 0U) {
            if (reason != nullptr) *reason = CancellationReason::kTargetDeleted;
            return ConflictDecision::kCancel;
        }
        relevant = true;
    }
    return relevant ? ConflictDecision::kReResolve : ConflictDecision::kContinue;
}
} // namespace canvas::interaction

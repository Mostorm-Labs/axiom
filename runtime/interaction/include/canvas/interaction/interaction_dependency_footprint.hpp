#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/semantic/change_set.hpp"

#include <span>
#include <vector>

namespace canvas::interaction {
enum class ConflictDecision { kContinue, kReResolve, kCancel };
enum class CancellationReason { kNone, kTargetDeleted, kDocumentDetached, kSurfaceSuspended, kSourceLost };
class InteractionDependencyFootprint final {
  public:
    bool track(foundation::ObjectId id);
    [[nodiscard]] bool dependsOn(foundation::ObjectId id) const noexcept;
    [[nodiscard]] ConflictDecision evaluate(const semantic::ChangeSet& changes,
                                             CancellationReason* reason = nullptr) const noexcept;
    [[nodiscard]] std::span<const foundation::ObjectId> objects() const noexcept { return objects_; }
    void clear() noexcept { objects_.clear(); }
  private:
    std::vector<foundation::ObjectId> objects_;
};
} // namespace canvas::interaction

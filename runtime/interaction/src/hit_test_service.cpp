#include "canvas/interaction/hit_test_service.hpp"

#include <algorithm>

namespace canvas::interaction {

std::vector<foundation::ObjectId>
HitTestService::selectable(std::span<const HitCandidate> candidates) const {
    std::vector<foundation::ObjectId> result;
    result.reserve(candidates.size());
    for (const HitCandidate& candidate : candidates) {
        if (!candidate.objectId.isZero() && candidate.visible && !candidate.locked &&
            std::find(result.begin(), result.end(), candidate.objectId) == result.end()) {
            result.push_back(candidate.objectId);
        }
    }
    return result;
}

} // namespace canvas::interaction

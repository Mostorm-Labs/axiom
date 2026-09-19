#pragma once

#include "canvas/foundation/object_id.hpp"

#include <span>
#include <vector>

namespace canvas::interaction {

struct HitCandidate final {
    foundation::ObjectId objectId{};
    bool visible = true;
    bool locked = false;
};

class HitTestService final {
  public:
    [[nodiscard]] std::vector<foundation::ObjectId>
    selectable(std::span<const HitCandidate> candidates) const;
};

} // namespace canvas::interaction

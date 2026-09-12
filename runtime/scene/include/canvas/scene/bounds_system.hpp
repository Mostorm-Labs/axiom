#pragma once

#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_record.hpp"

namespace canvas::scene {

struct BoundsResult final {
    foundation::WorldRect geometry{};
    foundation::WorldRect visual{};
    foundation::WorldRect world{};
    bool finite = true;
};

[[nodiscard]] BoundsResult computeBounds(const semantic::ObjectRecord& record) noexcept;

} // namespace canvas::scene

namespace canvas {
using scene::BoundsResult;
using scene::computeBounds;
}

#pragma once

#include "canvas/foundation/object_id.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace axiom::demo {

struct DemoTransientSnapshot final {
    std::optional<canvas::foundation::ObjectId> selectedObject;
    std::uint64_t selectionGeneration = 0;
    std::string overlayDigest;
};

} // namespace axiom::demo

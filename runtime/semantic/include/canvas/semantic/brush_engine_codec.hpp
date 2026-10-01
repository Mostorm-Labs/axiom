#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation.hpp"

namespace canvas::semantic {

struct BrushEngineCodec final {
    // Accept only the frozen new branch. This is a pre-mutation check and does
    // not alter the legacy V1 envelope validator.
    [[nodiscard]] static bool acceptsNewObject(const ObjectRecord&) noexcept;
    [[nodiscard]] static bool acceptsNewAddStroke(const Operation&) noexcept;
    [[nodiscard]] static bool rejectsLegacyObjectAsNewAuthoring(const ObjectRecord&) noexcept;
};

}  // namespace canvas::semantic

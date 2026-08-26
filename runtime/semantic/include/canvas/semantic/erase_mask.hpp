#pragma once

#include "canvas/semantic/semantic_geometry.hpp"
#include "canvas/semantic/semantic_id.hpp"

#include <variant>
#include <vector>

namespace canvas::semantic {

struct EraseKnot final {
    Vec2 position{};
    double radius = 0.0;

    bool operator==(const EraseKnot&) const = default;
};

struct EraseCubicSegment final {
    EraseKnot p0{};
    EraseKnot p1{};
    Vec2 control1{};
    Vec2 control2{};

    bool operator==(const EraseCubicSegment&) const = default;
};

struct SweptCircleMask final {
    std::vector<EraseCubicSegment> segments;

    bool operator==(const SweptCircleMask&) const = default;
};

struct FilledPathMask final {
    VectorPathGeometry path{};

    bool operator==(const FilledPathMask&) const = default;
};

// Frozen V1 renderer-neutral erase geometry. Runtime clip, texture, tile and
// renderer-path representations are deliberately excluded from canonical state.
using EraseMaskGeometry = std::variant<SweptCircleMask, FilledPathMask>;

struct EraseMaskRecord final {
    ObjectId id{};
    EraseMaskGeometry geometry{};

    bool operator==(const EraseMaskRecord&) const = default;
};

} // namespace canvas::semantic

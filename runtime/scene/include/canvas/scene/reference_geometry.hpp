#pragma once

#include "canvas/foundation/world_geometry.hpp"

#include <initializer_list>
#include <vector>

namespace canvas::scene {

enum class ReferencePathCommand : unsigned char { kMoveTo, kLineTo, kQuadTo, kCubicTo, kClose };

struct ReferencePathElement final {
    ReferencePathCommand command = ReferencePathCommand::kMoveTo;
    double x = 0.0;
    double y = 0.0;
    double control_x = 0.0;
    double control_y = 0.0;
    double control2_x = 0.0;
    double control2_y = 0.0;
};

struct ReferenceGeometry final {
    enum class Kind : unsigned char { kEmpty, kRectangle, kPath };
    Kind kind = Kind::kEmpty;
    double width = 0.0;
    double height = 0.0;
    std::vector<ReferencePathElement> elements;

    static ReferenceGeometry rectangle(double width, double height);
    static ReferenceGeometry path(std::initializer_list<ReferencePathElement> elements);
    static ReferenceGeometry path(std::vector<ReferencePathElement> elements);
};

[[nodiscard]] foundation::WorldRect geometryBounds(const ReferenceGeometry& geometry) noexcept;

} // namespace canvas::scene

namespace canvas {
using scene::ReferenceGeometry;
using scene::ReferencePathCommand;
using scene::ReferencePathElement;
using scene::geometryBounds;
}

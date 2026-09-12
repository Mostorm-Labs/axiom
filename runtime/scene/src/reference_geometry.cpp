#include "canvas/scene/reference_geometry.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::scene {

ReferenceGeometry ReferenceGeometry::rectangle(double width, double height) {
    return ReferenceGeometry{Kind::kRectangle, width, height, {}};
}

ReferenceGeometry ReferenceGeometry::path(std::initializer_list<ReferencePathElement> values) {
    return path(std::vector<ReferencePathElement>(values));
}

ReferenceGeometry ReferenceGeometry::path(std::vector<ReferencePathElement> values) {
    ReferenceGeometry result;
    result.kind = Kind::kPath;
    result.elements = std::move(values);
    return result;
}

foundation::WorldRect geometryBounds(const ReferenceGeometry& geometry) noexcept {
    if (geometry.kind == ReferenceGeometry::Kind::kRectangle) {
        const float width = static_cast<float>(geometry.width);
        const float height = static_cast<float>(geometry.height);
        return foundation::canonicalizeRect({0.0F, 0.0F, width, height});
    }
    if (geometry.kind != ReferenceGeometry::Kind::kPath || geometry.elements.empty()) {
        return {};
    }
    double left = std::numeric_limits<double>::infinity();
    double top = std::numeric_limits<double>::infinity();
    double right = -std::numeric_limits<double>::infinity();
    double bottom = -std::numeric_limits<double>::infinity();
    for (const auto& element : geometry.elements) {
        const bool has_control = element.command == ReferencePathCommand::kQuadTo ||
                                 element.command == ReferencePathCommand::kCubicTo;
        const double xs[] = {element.x, has_control ? element.control_x : element.x};
        const double ys[] = {element.y, has_control ? element.control_y : element.y};
        for (double x : xs) {
            left = std::min(left, x); right = std::max(right, x);
        }
        for (double y : ys) {
            top = std::min(top, y); bottom = std::max(bottom, y);
        }
    }
    return foundation::canonicalizeRect({static_cast<float>(left), static_cast<float>(top),
                                          static_cast<float>(right), static_cast<float>(bottom)});
}

} // namespace canvas::scene

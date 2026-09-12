#include "canvas/scene/reference_geometry.hpp"

#include <cassert>
#include <cmath>
#include <limits>

int main() {
    using namespace canvas::scene;
    const ReferenceGeometry rectangle = ReferenceGeometry::rectangle(10.0, 20.0);
    const auto bounds = geometryBounds(rectangle);
    assert(bounds.left == 0.0F && bounds.top == 0.0F);
    assert(bounds.right == 10.0F && bounds.bottom == 20.0F);

    const ReferenceGeometry path = ReferenceGeometry::path({
        {ReferencePathCommand::kMoveTo, 2.0, 3.0, 0.0, 0.0},
        {ReferencePathCommand::kLineTo, -4.0, 8.0, 0.0, 0.0},
    });
    const auto pathBounds = geometryBounds(path);
    assert(pathBounds.left == -4.0F && pathBounds.top == 3.0F);
    assert(pathBounds.right == 2.0F && pathBounds.bottom == 8.0F);

    const ReferenceGeometry cubic = ReferenceGeometry::path({
        {ReferencePathCommand::kCubicTo, 4.0, 5.0, -10.0, 12.0, 18.0, -9.0},
    });
    const auto cubicBounds = geometryBounds(cubic);
    assert(cubicBounds.left == -10.0F && cubicBounds.top == -9.0F);
    assert(cubicBounds.right == 18.0F && cubicBounds.bottom == 12.0F);

    const auto invalid = ReferenceGeometry::rectangle(-1.0, 2.0);
    assert(!geometryBounds(invalid).isFiniteAndOrdered());

    const auto empty = geometryBounds(ReferenceGeometry::path({}));
    assert(empty == canvas::foundation::WorldRect{});

    const auto nonfinite = geometryBounds(ReferenceGeometry::rectangle(
        std::numeric_limits<double>::infinity(), 2.0));
    assert(!nonfinite.isFiniteAndOrdered());
    return 0;
}

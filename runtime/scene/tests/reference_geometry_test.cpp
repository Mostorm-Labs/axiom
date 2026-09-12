#include "canvas/scene/reference_geometry.hpp"

#include <cassert>
#include <cmath>

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

    const auto invalid = ReferenceGeometry::rectangle(-1.0, 2.0);
    assert(!geometryBounds(invalid).isFiniteAndOrdered());
    return 0;
}

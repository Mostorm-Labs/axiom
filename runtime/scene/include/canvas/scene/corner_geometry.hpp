#pragma once
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_content.hpp"
#include <span>
namespace canvas::scene {
// Source-profile immutable geometry values; not an ordinary SDK entry point.
semantic::VectorPathGeometry roundedPolyline(std::span<const semantic::Vec2>, bool closed,
                                             double radius,
                                             std::span<const foundation::WorldRect> obstacles = {});
bool roundedShapePerimeter(const semantic::ShapeContent &, double radius, semantic::Vec2 direction,
                           semantic::Vec2 &position, semantic::Vec2 &normal);
} // namespace canvas::scene

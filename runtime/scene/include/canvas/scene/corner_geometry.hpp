#pragma once
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_content.hpp"
#include <array>
#include <optional>
#include <span>
namespace canvas::semantic { struct Transform2D; }
namespace canvas::scene {
// Source-profile immutable geometry values; not an ordinary SDK entry point.
semantic::VectorPathGeometry roundedPolyline(std::span<const semantic::Vec2>, bool closed,
                                             double radius,
                                             std::span<const foundation::WorldRect> obstacles = {},
                                             std::optional<std::array<semantic::Vec2,2>> shaftEnds = {});
// Direction in the target's normalized box for the nearest canonical contour point.
std::optional<semantic::Vec2> nearestShapePerimeterHint(const semantic::ShapeContent&, double radius,
    const semantic::Transform2D& world, foundation::WorldPoint pointer);
bool roundedShapePerimeter(const semantic::ShapeContent &, double radius, semantic::Vec2 direction,
                           semantic::Vec2 &position, semantic::Vec2 &normal);
} // namespace canvas::scene

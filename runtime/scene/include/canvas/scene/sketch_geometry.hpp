#pragma once
#include "canvas/scene/derived_geometry.hpp"
#include <memory>
namespace canvas::scene {
// Immutable renderer-neutral local paths. Cache excludes paint and transforms.
struct SketchGeometry final {
    semantic::VectorPathGeometry body{}, outline{}, hatches{}, arrowFill{};
    foundation::WorldRect bounds{};
    double outlineWidthScale = 1;
    bool operator==(const SketchGeometry &) const = default;
};
[[nodiscard]] std::shared_ptr<const SketchGeometry>
sketchGeometry(const semantic::ObjectContent &, const semantic::PropertyBag &,
               const ConnectorGeometry * = nullptr);
} // namespace canvas::scene

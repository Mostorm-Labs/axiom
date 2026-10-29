#pragma once
#include "canvas/scene/scene_types.hpp"
#include <span>
namespace canvas::render {
enum class SelectionGeometryHit : std::uint8_t { kHit, kMiss, kUnsupported, kInvalid };
// Uses the existing published geometry/layout. Never measures text or routes connectors.
[[nodiscard]] SelectionGeometryHit intersectVisibleSelectionGeometry(
    const RuntimeSceneRecord&,std::span<const foundation::WorldPoint>) noexcept;
}

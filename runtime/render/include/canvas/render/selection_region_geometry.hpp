#pragma once
#include "canvas/scene/scene_types.hpp"
#include <span>
namespace canvas::render {
enum class SelectionGeometryHit : std::uint8_t { kHit, kMiss, kUnsupported, kInvalid };
struct EraseGeometryWork final {std::size_t historicalDifferences=0,componentIntersections=0;};
// Uses the existing published geometry/layout. Never measures text or routes connectors.
[[nodiscard]] SelectionGeometryHit intersectVisibleSelectionGeometry(
    const RuntimeSceneRecord&,std::span<const foundation::WorldPoint>) noexcept;
// Object-local canonical erase geometry, against the same visible ink body.
[[nodiscard]] SelectionGeometryHit intersectEraseGeometry(
    const RuntimeSceneRecord&,const semantic::EraseMaskGeometry&,EraseGeometryWork* = nullptr) noexcept;
}

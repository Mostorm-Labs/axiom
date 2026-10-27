#pragma once
#include "canvas/interaction/region_selection_session.hpp"
#include "canvas/semantic/object_record.hpp"

namespace canvas::interaction {
enum class RegionIntersection : std::uint8_t { kHit, kMiss, kUnsupported, kInvalid };
// The object's transform must be its composed world transform. AABB candidates
// are supplied by Scene; this is the precise, renderer-independent narrow phase.
[[nodiscard]] RegionIntersection intersectSelectionRegion(
    const semantic::ObjectRecord&,std::span<const RegionPoint>) noexcept;
} // namespace canvas::interaction

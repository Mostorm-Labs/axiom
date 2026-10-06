#pragma once

#include "canvas/ink/vector_stroke_reference.hpp"
#include "canvas/semantic/semantic_geometry.hpp"

#include "include/core/SkPath.h"

#include <cstdint>
#include <span>

namespace canvas::render {

// Builds the Skia path used by the common Vector Brush render paths.  The
// point conversion, line ordering, fill rule, and closure are intentionally
// centralized so preview and canonical qualification cannot drift.
[[nodiscard]] SkPath buildVectorBrushOutlineSkPath(
    std::span<const ink::reference::StrokeOutlinePoint> outline,
    std::uint32_t fillRule = 1U, bool closed = true);

[[nodiscard]] SkPath buildVectorBrushOutlineSkPath(
    std::span<const semantic::Vec2> outline,
    std::uint32_t fillRule = 1U, bool closed = true);

}  // namespace canvas::render

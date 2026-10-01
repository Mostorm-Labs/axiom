#pragma once

#include "canvas/ink/programmable_brush.hpp"

#include <span>

class SkCanvas;

namespace canvas::render::internal {

// Renderer-only programmable-DAB rasterization. BrushRuntime has already
// evaluated BrushDefinition into immutable primitives; this helper must not
// inspect or reinterpret authoring definitions.
void drawBrushPrimitivesToSkCanvas(
    SkCanvas& canvas,
    std::span<const canvas::ink::BrushPrimitive> primitives);

}  // namespace canvas::render::internal

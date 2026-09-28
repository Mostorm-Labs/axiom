#pragma once

#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/ink/brush_preview_delta.hpp"

#include <span>

class SkCanvas;

namespace canvas::render::internal {

// Shared Skia scene contract used by both the headless reference backend and
// SkiaRenderer/provider production path. The implementation is kept in the
// render target so there is one validation and nine-command mapping.
[[nodiscard]] BackendSubmissionResult drawReferencePlanToSkCanvas(
    SkCanvas& canvas, const FramePlan& plan);

void drawDabInstancesToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color);

void drawPreviewDabsToSkCanvas(
    SkCanvas& canvas, std::span<const ink::BrushDab> dabs,
    float red, float green, float blue, float alpha);

} // namespace canvas::render::internal

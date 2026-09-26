#pragma once

#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"

class SkCanvas;

namespace canvas::render::internal {

// Shared Skia scene contract used by both the headless reference backend and
// SkiaRenderer/provider production path. The implementation is kept in the
// render target so there is one validation and nine-command mapping.
[[nodiscard]] BackendSubmissionResult drawReferencePlanToSkCanvas(
    SkCanvas& canvas, const FramePlan& plan);

} // namespace canvas::render::internal

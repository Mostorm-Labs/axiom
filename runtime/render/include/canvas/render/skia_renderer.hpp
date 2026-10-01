#pragma once

#include "canvas/render/brush_render_point.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <span>
#include <vector>

class SkCanvas;

namespace canvas::render {

struct CanonicalStrokePoint;
struct CanonicalViewportTransform;

// The only common drawing adapter. It consumes runtime-owned render data and
// submits Skia draw calls to a platform-owned SkiaSurfaceProvider.
class SkiaRenderer final {
  public:
    [[nodiscard]] BackendSubmissionResult renderFrame(
        SkiaSurfaceProvider& provider, const FramePlan& plan);
    [[nodiscard]] BackendSubmissionResult renderPreview(
        SkiaSurfaceProvider& provider, const PreviewGeometry& geometry,
        const PreviewStyleOverride& style = {});
    [[nodiscard]] BackendSubmissionResult clearPreview(
        SkiaSurfaceProvider& provider, SurfaceGeneration generation);
    [[nodiscard]] BackendSubmissionResult renderPrimitives(
        SkiaSurfaceProvider& provider,
        std::span<const canvas::ink::BrushPrimitive> primitives,
        float scale = 1.0F, float translationX = 0.0F,
        float translationY = 0.0F);
    [[nodiscard]] BackendSubmissionResult renderBrushPoints(
        SkiaSurfaceProvider& provider,
        std::span<const BrushRenderPoint> points,
        float scale = 1.0F, float translationX = 0.0F,
        float translationY = 0.0F);
    [[nodiscard]] BackendSubmissionResult renderStrokes(
        SkiaSurfaceProvider& provider,
        std::span<const std::vector<CanonicalStrokePoint>> strokes,
        float scale = 1.0F, float translationX = 0.0F,
        float translationY = 0.0F);

    [[nodiscard]] std::uint64_t submissionCount() const noexcept { return submissions_; }
    [[nodiscard]] std::uint64_t rasterizationCount() const noexcept { return rasterizations_; }

  private:
    std::uint64_t submissions_ = 0;
    std::uint64_t rasterizations_ = 0;
};

} // namespace canvas::render

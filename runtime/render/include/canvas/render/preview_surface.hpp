#pragma once

#include "canvas/ink/brush_preview_delta.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <cstdint>
#include <vector>

namespace canvas::render {

class SkiaRenderer;

// Render-only styling. It never mutates BrushPackage, ResolvedBrushState or
// semantic BrushStrokeContent.
struct PreviewStyleOverride final {
    float red = 1.0F;
    float green = 0.85F;
    float blue = 0.0F;
    float alpha = 0.55F;
    float opacityMultiplier = 0.55F;
    bool overrideColor = true;
    bool overrideOpacity = true;
    // SkBlendMode::kClear is 0; kSrcOver is 1. Keep the wire value explicit
    // so the amber overlay cannot silently erase its own surface.
    std::uint8_t blendMode = 1U; // SkBlendMode::kSrcOver
};

struct PreviewGeometry final {
    std::uint64_t documentEpoch = 0;
    std::uint64_t session = 0;
    std::uint64_t sessionGeneration = 0;
    std::uint64_t revision = 0;
    SurfaceGeneration surfaceGeneration{};
    CanonicalViewportTransform viewport{};
    // Multiple active BrushSessions share one transparent preview surface.
    // Each contour remains owned by its session so a second contact cannot
    // replace the first contact's retained geometry.
    struct Contour final {
        std::uint64_t session = 0;
        std::uint64_t revision = 0;
        std::vector<ink::reference::StrokeOutlinePoint> outline;
    };
    std::vector<Contour> contours;
    std::vector<ink::reference::StrokeOutlinePoint> outline;
};

struct SurfaceRenderState final {
    bool dirty = false;
    std::uint64_t contentRevision = 0;
    std::uint64_t submittedRevision = 0;
    std::uint64_t generation = 0;
    std::uint64_t submissionCount = 0;
    std::uint64_t presentCount = 0;
};

class PreviewSurfaceController final {
  public:
    PreviewSurfaceController(SkiaRenderer& renderer,
                             SkiaSurfaceProvider& provider) noexcept;

    [[nodiscard]] bool begin(std::uint64_t documentEpoch,
                             std::uint64_t session,
                             std::uint64_t sessionGeneration,
                             std::uint64_t surfaceGeneration) noexcept;
    [[nodiscard]] bool update(const ink::BrushPreviewDelta& delta,
                              float scale = 1.0F,
                              float translationX = 0.0F,
                              float translationY = 0.0F) noexcept;
    [[nodiscard]] bool updateForSession(
        std::uint64_t session, const ink::BrushPreviewDelta& delta,
        float scale = 1.0F, float translationX = 0.0F,
        float translationY = 0.0F) noexcept;
    // Re-render the retained geometry when the viewport changes without a
    // new brush revision (for example during a two-finger pinch).
    [[nodiscard]] bool updateViewport(float scale,
                                      float translationX,
                                      float translationY) noexcept;
    [[nodiscard]] bool cancel() noexcept;
    [[nodiscard]] bool cancelSession(std::uint64_t session) noexcept;
    [[nodiscard]] bool retireSession(std::uint64_t session,
                                     std::uint64_t surfaceGeneration) noexcept;
    // Rebind retains the latest geometry but makes the new provider generation
    // the only valid target.  The retained preview is dirty until presented.
    [[nodiscard]] bool rebind(std::uint64_t surfaceGeneration) noexcept;
    [[nodiscard]] bool renderIfDirty(
        const PreviewStyleOverride& style = {}) noexcept;
    [[nodiscard]] bool clearAfterCanonicalVisible(
        std::uint64_t surfaceGeneration) noexcept;
    [[nodiscard]] bool clearAfterCanonicalVisible(
        std::uint64_t documentEpoch, std::uint64_t session,
        std::uint64_t sessionGeneration,
        std::uint64_t surfaceGeneration) noexcept;

    [[nodiscard]] const SurfaceRenderState& state() const noexcept { return state_; }
    [[nodiscard]] const PreviewGeometry& geometry() const noexcept { return geometry_; }
    [[nodiscard]] bool active() const noexcept { return active_; }

  private:
    SkiaRenderer& renderer_;
    SkiaSurfaceProvider& provider_;
    PreviewGeometry geometry_{};
    SurfaceRenderState state_{};
    bool active_ = false;
    std::uint64_t defaultSession_ = 0;
};

} // namespace canvas::render

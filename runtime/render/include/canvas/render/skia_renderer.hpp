#pragma once
#include "canvas/render/skia_text_resources.hpp"

#include "canvas/render/brush_render_point.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/editing_overlay.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/render/navigation_sources.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/render/damage_tracker.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <span>
#include <vector>

class SkCanvas;

namespace canvas::render {
namespace internal {class SkiaRenderCache;class SkiaNavigationTiles;class SourceRetentionPool;class GpuResourceRetirement;}

struct CanonicalStrokePoint;
struct CanonicalViewportTransform;

// The only common drawing adapter. It consumes runtime-owned render data and
// submits Skia draw calls to a platform-owned SkiaSurfaceProvider.
class SkiaRenderer final {
  public:
    SkiaRenderer();
    ~SkiaRenderer();
    SkiaRenderer(const SkiaRenderer&)=delete;
    SkiaRenderer& operator=(const SkiaRenderer&)=delete;
    // Private/source owner opportunity only; no GPU handle or consumer API.
    void serviceResourceRetirements()noexcept;
    explicit SkiaRenderer(ImageResourceResolver* images) : SkiaRenderer() { images_ = images; }
    void setImageResolver(ImageResourceResolver* images) noexcept { images_ = images; ++resourceEpoch_; clearRenderCache(); }
    [[nodiscard]] BackendSubmissionResult renderFrame(
        SkiaSurfaceProvider& provider, const FramePlan& plan,
        const EditingOverlay* selectionOverlay = nullptr,
        const RenderPreparationBudget* preparation = nullptr);
    // Approximate navigation feedback only; never a canonical coverage receipt.
    [[nodiscard]] BackendSubmissionResult renderNavigation(
        SkiaSurfaceProvider&, const CanonicalViewportTransform&, const FrameState&,
        const EditingOverlay* selectionOverlay=nullptr);
    [[nodiscard]] BackendSubmissionResult renderNavigation(
        SkiaSurfaceProvider&, const CanonicalViewportTransform&, const FrameState&,
        const NavigationSourceQuery&, bool allowPrewarm=false,
        const EditingOverlay* selectionOverlay=nullptr,
        const NavigationBackgroundQuery& backgroundOnly={});
    [[nodiscard]] NavigationWork navigationWork() const noexcept;
    void cancelNavigationWork() noexcept;
    [[nodiscard]] BackendSubmissionResult renderTransientOverlay(
        SkiaSurfaceProvider& provider, const FramePlan& plan,
        const EditingOverlay* selectionOverlay);
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
    struct CompactWork final {std::uint64_t generatedKnots=0,retainedBytes=0,temporaryBytes=0;};
    [[nodiscard]] CompactWork compactWork()const noexcept{return compactWork_;}
    void clearCompactCache() noexcept {compactCache_.reset();compactWork_.retainedBytes=0;}
    void setRenderCacheOptions(RenderCacheOptions options);
    void clearRenderCache() noexcept;
    [[nodiscard]] RenderCacheWork renderCacheWork() const noexcept;
    [[nodiscard]] RenderDamage renderDamage() const;

  private:
    std::shared_ptr<internal::GpuResourceRetirement> gpuRetirements_;
    std::shared_ptr<internal::SkiaNavigationTiles> navigationTiles_;
    struct PartialEraseFrame;
    std::shared_ptr<PartialEraseFrame> partialEraseFrame_;
    struct NavigationCache;
    std::shared_ptr<NavigationCache> navigationCache_;
    std::shared_ptr<NavigationCache> navigationFallback_;
    std::shared_ptr<resources::ResourceBudgetManager> resourceBudget_;
    std::shared_ptr<internal::SourceRetentionPool> sourceRetention_;
    ImageResourceResolver* images_ = nullptr;
    SkiaTextResources textResources_;
    std::uint64_t submissions_ = 0;
    std::uint64_t rasterizations_ = 0;
    struct CompactCache;
    std::shared_ptr<CompactCache> compactCache_;
    CompactWork compactWork_{};
    std::shared_ptr<internal::SkiaRenderCache> renderCache_;
    RenderCacheOptions renderCacheOptions_{};
    std::uint64_t resourceEpoch_=0;
};

} // namespace canvas::render

#pragma once
#include "canvas/render/skia_text_resources.hpp"

#include "canvas/render/brush_render_point.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/preview_surface.hpp"
#include "canvas/render/editing_overlay.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_cache.hpp"
#include "canvas/render/selection_region_geometry.hpp"
#include "canvas/render/navigation_sources.hpp"
#include "canvas/resources/resource_budget_manager.hpp"
#include "canvas/render/damage_tracker.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <span>
#include <vector>
#include <map>

class SkCanvas;

namespace canvas::render {
namespace internal {struct TransientDisplaySurface;class SkiaRenderCache;class SkiaNavigationTiles;class SourceRetentionPool;class PreparedGeometryCache;class GpuResourceRetirement;class EraseMaskPathCache;}
namespace internal {struct ObjectInteractionWork;}

struct CanonicalStrokePoint;
struct CanonicalViewportTransform;
// Private/source lifecycle hint; not a JS/DLL consumer capability.
enum class ResourceServiceMode { kOwnerOpportunity, kBeginSurfaceTransition, kEndSurfaceTransition };

// The only common drawing adapter. It consumes runtime-owned render data and
// submits Skia draw calls to a platform-owned SkiaSurfaceProvider.
class SkiaRenderer final {
  public:
    SkiaRenderer();
    ~SkiaRenderer();
    SkiaRenderer(const SkiaRenderer&)=delete;
    SkiaRenderer& operator=(const SkiaRenderer&)=delete;
    // Private/source owner opportunity only; no GPU handle or consumer API.
    void serviceResourceRetirements(ResourceServiceMode = ResourceServiceMode::kOwnerOpportunity)noexcept;
    [[nodiscard]] bool resourceRetirementPending()const noexcept;
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
        const NavigationBackgroundQuery& backgroundOnly={},
        const FramePlan* currentComposition=nullptr);
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
#if defined(AXIOM_TRANSIENT_DISPLAY_TESTS) || defined(AXIOM_G5_UR0_DIAGNOSTICS)
    [[nodiscard]] internal::ObjectInteractionWork objectInteractionWork() const noexcept;
#endif
    // Owner-thread derived exact-query reuse. Returns only a value decision;
    // canonical target selection still belongs to the interaction owner.
    [[nodiscard]] SelectionGeometryHit intersectEraseGeometry(const RuntimeSceneRecord&,
        const semantic::EraseMaskGeometry&) const noexcept;
    [[nodiscard]] std::vector<std::uint8_t> encodeCpuPendingRequest(
        const semantic::EraseMaskGeometry&,const std::array<std::uint64_t,8>&)const;
    void beginCpuPendingOpportunity()noexcept;
    void setCpuPendingPreparation(bool value)noexcept;
    std::optional<resources::ResourceLease> reserveCpuPendingMetadata(std::size_t)noexcept;
    using CpuPathPin=std::shared_ptr<const void>;
    [[nodiscard]] CpuPathPin lookupCpuPendingPath(const semantic::EraseMaskGeometry&)const noexcept;
    [[nodiscard]] CpuPathPin importCpuPendingPath(const semantic::EraseMaskGeometry&,
        std::span<const std::uint8_t>,const std::array<std::uint64_t,8>&)const noexcept;
    [[nodiscard]] SelectionGeometryHit confirmCpuPendingErase(const RuntimeSceneRecord&,
        const semantic::EraseMaskGeometry&,const CpuPathPin&)const noexcept;
    bool retainCpuPendingHandoff(std::uint64_t,const std::vector<CpuPathPin>&)noexcept;
    void retireCpuPendingHandoff(std::uint64_t)noexcept;
    void retireAllCpuPendingHandoffs()noexcept;
    // Private same-build result consumer; bytes stay pinned by the owner until
    // this decision returns. No raw Skia object crosses the transport.
    [[nodiscard]] SelectionGeometryHit intersectCpuPendingErase(const RuntimeSceneRecord&,
        const semantic::EraseMaskGeometry&,std::span<const std::uint8_t>,
        const std::array<std::uint64_t,8>&) const noexcept;
    class ExactQueryCohort final {
        friend class SkiaRenderer;
        std::shared_ptr<internal::EraseMaskPathCache> owner_;
        explicit ExactQueryCohort(std::shared_ptr<internal::EraseMaskPathCache>,std::function<void()> = {}) noexcept;
      public:
        ~ExactQueryCohort();
        ExactQueryCohort(const ExactQueryCohort&)=delete;
        ExactQueryCohort& operator=(const ExactQueryCohort&)=delete;
    };
    // Private source profile only; not installed/exported in consumer SDKs.
    [[nodiscard]] ExactQueryCohort exactQueryCohort(bool active=true) const noexcept;
    // Existing interaction owner cancels disposable partial exact preparation.
    void discardEraseQueryPreparation() noexcept;
    // Private platform quota forwarding. No manager/context is returned.
    std::uint64_t reserveCpuWorkerGrant(std::uint64_t executor,std::size_t bytes) noexcept;
    bool releaseCpuWorkerGrant(std::uint64_t executor,std::uint64_t token) noexcept;
    std::size_t cpuWorkerGrantBytes() const noexcept;
    [[nodiscard]] SelectionGeometryHit intersectSelectionGeometry(const RuntimeSceneRecord&,
        std::span<const foundation::WorldPoint>) const noexcept;
    [[nodiscard]] RenderDamage renderDamage() const;

  private:
    std::shared_ptr<internal::GpuResourceRetirement> gpuRetirements_;
    std::shared_ptr<internal::SkiaNavigationTiles> navigationTiles_;
    struct PartialEraseFrame;
    std::shared_ptr<PartialEraseFrame> partialEraseFrame_;
    std::shared_ptr<internal::TransientDisplaySurface> transientDisplay_;
    struct NavigationCache;
    std::shared_ptr<NavigationCache> navigationCache_;
    std::shared_ptr<NavigationCache> navigationFallback_;
    mutable std::shared_ptr<resources::ResourceBudgetManager> resourceBudget_;
    struct CpuGrant {std::uint64_t executor;resources::ResourceLease lease;std::size_t bytes;};
    std::map<std::uint64_t,CpuGrant> cpuGrants_;
    std::uint64_t nextCpuGrant_=0;
    mutable std::shared_ptr<internal::EraseMaskPathCache> eraseQueries_;
    mutable std::shared_ptr<internal::SourceRetentionPool> sourceRetention_;
    mutable std::shared_ptr<internal::PreparedGeometryCache> preparedGeometry_;
    ImageResourceResolver* images_ = nullptr;
    SkiaTextResources textResources_;
    std::uint64_t submissions_ = 0;
    std::uint64_t rasterizations_ = 0;
    RenderCacheWork executionWork_;
    struct CompactCache;
    std::shared_ptr<CompactCache> compactCache_;
    CompactWork compactWork_{};
    std::shared_ptr<internal::SkiaRenderCache> renderCache_;
    RenderCacheOptions renderCacheOptions_{};
    std::uint64_t resourceEpoch_=0;
    bool preserveWorldPreparation_=false;
    bool cpuPendingPreparation_=false;
    struct CpuPathHandoff {std::vector<CpuPathPin> pins;resources::ResourceLease metadata;};
    std::map<std::uint64_t,CpuPathHandoff> cpuPathHandoffs_;
    std::optional<std::chrono::steady_clock::time_point> cpuPendingOpportunity_;
};

} // namespace canvas::render

#pragma once

#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/ink/brush_preview_delta.hpp"

#include <span>

class SkCanvas;
class SkImage;
class SkPicture;
class SkPath;
namespace canvas::render { class ImageResourceResolver; }
namespace canvas::render { class SkiaTextResources; }
namespace canvas::render { class EditingOverlay; }

namespace canvas::render::internal {
class SkiaRenderCache;
void drawEditingOverlayToSkCanvas(SkCanvas&, const SurfaceMetrics&, const EditingOverlay&);
struct CompactRasterWork;
class CompactCoverageTargets;
class PreparedNavigationGeometry;
struct CompactDeviceGrid final {int left=0,top=0,right=0,bottom=0;};
struct CompactStrokeRange final {
    std::size_t begin = 0;
    std::size_t end = 0;
    double startDistance = 0.0;
    double totalDistance = -1.0;
    bool allowStartCap = false;
    bool allowEndCap = false;
};
// Only accepts an already validated isolated compact command; no second
// geometry implementation is owned by the cache materializer.
void drawValidatedIsolatedEntryToSkCanvas(SkCanvas&,const ReferenceTraversalEntry&,const SkPicture* coverage = nullptr, const CompactDeviceGrid* grid = nullptr, CompactRasterWork* work = nullptr, bool compatibleCoverage = false, CompactCoverageTargets* targets = nullptr, ImageResourceResolver* images = nullptr, SkiaTextResources* textResources = nullptr, const SkPath* preparedVectorPath = nullptr);
struct CompactRasterWork final {
    // Live owners may explicitly yield expected quota denial without throwing
    // through a platform library that disables exception unwinding.
    bool resourcePending=false;
    std::uint64_t allocatedTiles = 0;
    std::uint64_t allocatedPixels = 0;
    std::uint64_t compositedPixels = 0;
    std::uint64_t gpuTiles=0,cpuTiles=0,targetReuses=0,gpuInteriorPixels=0,cpuEdgePixels=0;
    std::uint64_t compiledContourKnots=0,contourRecordReplays=0;
    std::uint64_t contourRecordCandidates=0,contourRecordsRejected=0,contourRecordGroups=0,contourSelectionYields=0;
    std::uint64_t contourCheckpointReplays=0,checkpointReplayKnots=0;
    std::uint64_t scalarCoverageTiles=0;
};
void drawCompactStrokeToSkCanvas(SkCanvas&,const foundation::InkBrushConfig&,
                                std::span<const foundation::InkKnot>,std::uint64_t seed=0,
                                CompactRasterWork* work=nullptr, const SkPicture* coverage=nullptr, const CompactDeviceGrid* grid=nullptr,bool compatibleCoverage=false, CompactCoverageTargets* targets=nullptr,
                                const CompactStrokeRange* range=nullptr,const PreparedNavigationGeometry* prepared=nullptr,bool yieldOnAllocation=false);
// Disposable scalar distance coverage. The material is evaluated once after
// stable prefix and replaceable tail have been combined with max coverage.
void drawCompactContourCoverageToSkCanvas(SkCanvas&,const foundation::InkBrushConfig&,const ink::CompactContour&,bool gpuWhiteCoverage=false);
void drawCompactCoverageToSkCanvas(SkCanvas&,const foundation::InkBrushConfig&,
                                  std::span<const foundation::InkKnot>,double startDistance=0,double totalDistance=-1,std::uint64_t seed=0,
                                  bool allowStartCap=true,bool allowEndCap=true,bool gpuWhiteCoverage=false);
// Same coverage equation, with original predecessor at a resumed offset.
void drawCompactCoverageRangeToSkCanvas(SkCanvas&,const foundation::InkBrushConfig&,
    std::span<const foundation::InkKnot>,std::size_t begin,std::size_t end,double beginDistance=-1,double totalDistance=-1,std::uint64_t seed=0);
void compositeCompactCoverageToSkCanvas(SkCanvas&,const SkImage&,
                                       const foundation::InkBrushConfig&,std::uint64_t seed=0,
                                       int deviceX=0,int deviceY=0);

[[nodiscard]] BackendSubmissionResult validateReferencePlan(const FramePlan&);

// Shared Skia scene contract used by both the headless reference backend and
// SkiaRenderer/provider production path. The implementation is kept in the
// render target so there is one validation and nine-command mapping.
// Validated subranges share the exact traversal/isolation mapping with Direct.
// Used only for bounded offscreen composition, never partial target publication.
void drawReferenceBackgroundToSkCanvas(SkCanvas&,const FramePlan&);
// Shared destination/isolation policy for exact drawing and cooperative work.
bool requiresInkIsolation(const FramePlan&);
void drawValidatedReferenceRangeToSkCanvas(SkCanvas&,const FramePlan&,
    std::size_t begin,std::size_t end,ImageResourceResolver*,SkiaTextResources*,
    bool includeInk=true,SkiaRenderCache* cache=nullptr,const CompactDeviceGrid* grid=nullptr);
void drawEraseMasksToSkCanvas(SkCanvas&, const ReferenceTraversalEntry&);
// Shared command mapping for isolated damage work; masks are applied with the
// original full-device clip so their AA evaluator is identical to full painting.
void drawInkBodyToSkCanvas(SkCanvas&,const ReferenceTraversalEntry&,ImageResourceResolver*,SkiaTextResources*,const CompactDeviceGrid*,
    CompactRasterWork* work=nullptr,bool compatibleCoverage=false,CompactCoverageTargets* targets=nullptr,bool yieldOnAllocation=false);
void drawPresentationEraseMasksToSkCanvas(SkCanvas&,const ReferenceTraversalEntry&);
[[nodiscard]] BackendSubmissionResult drawReferencePlanToSkCanvas(
    SkCanvas& canvas, const FramePlan& plan, ImageResourceResolver* images = nullptr,
    SkiaTextResources* textResources = nullptr, bool includeInk = true,
    SkiaRenderCache* cache = nullptr, const CompactDeviceGrid* grid = nullptr);
// Repaint the dedicated ink lane after a transient creation object. Ink is
// intentionally topmost while a Shape/Connector is being authored.
void drawInkEntriesToSkCanvas(
    SkCanvas& canvas, const FramePlan& plan, ImageResourceResolver* images = nullptr,
    SkiaTextResources* textResources = nullptr, SkiaRenderCache* cache = nullptr,
    bool approximateRetainedRuns = false);
// Reuse the canonical renderer command mapping for a view-owned provisional
// object. This value never enters Semantic or RuntimeScene storage.
void drawTransientObjectToSkCanvas(SkCanvas& canvas, const FramePlan& plan,
                                  const semantic::ObjectRecord& object,
                                  const scene::ConnectorGeometry* route = nullptr);
void drawTransientEntryToSkCanvas(SkCanvas& canvas, const FramePlan& plan,
                                  const ReferenceTraversalEntry& entry);

void drawDabInstancesToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color);

void drawPreviewDabsToSkCanvas(
    SkCanvas& canvas, std::span<const ink::BrushDab> dabs,
    float red, float green, float blue, float alpha);

void drawChalkDabsToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color, std::uint32_t materialRevision);

void drawPreviewChalkDabsToSkCanvas(
    SkCanvas& canvas, std::span<const ink::BrushDab> dabs,
    float red, float green, float blue, float alpha,
    std::uint32_t materialRevision);

void drawMembraneDabsToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color);

void drawPreviewMembraneDabsToSkCanvas(
    SkCanvas& canvas, std::span<const ink::BrushDab> dabs,
    float red, float green, float blue, float alpha);

void drawContinuousChalkDabsToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color, std::uint32_t materialRevision);

} // namespace canvas::render::internal

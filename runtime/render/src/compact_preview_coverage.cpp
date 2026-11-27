#include "compact_preview_coverage.hpp"
#include "compact_contour_skia_path.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkImage.h"

namespace canvas::render::internal {
namespace {
// Match the canonical CPU reference grid. Moving a raster clip edge changes
// Skia analytic path AA, even several pixels inside that edge.
constexpr int tileSize = 256;
constexpr std::size_t tileBytes = tileSize * tileSize * 4;

SkIRect visibleBounds(const ink::CompactContour& contour, const SkMatrix& matrix,
                      const SkIRect& viewport) {
    // Quadratic paths stay in their control hull. Inspect the coordinates
    // directly so the bounds query never builds a second set of SkPaths.
    auto world = SkRect::MakeEmpty();
    if (contour.patches.size() >= 2) {
        float left = SK_ScalarInfinity, top = SK_ScalarInfinity;
        float right = -SK_ScalarInfinity, bottom = -SK_ScalarInfinity;
        for (const auto& patch : contour.patches) {
            for (const auto p : {patch.leftStart, patch.leftControl, patch.leftEnd,
                                 patch.rightStart, patch.rightControl, patch.rightEnd}) {
                left = std::min(left, float(p.x));
                right = std::max(right, float(p.x));
                top = std::min(top, float(p.y));
                bottom = std::max(bottom, float(p.y));
            }
        }
        world = SkRect::MakeLTRB(left, top, right, bottom);
    }
    for (const auto& cap : contour.caps) world.join(compactContourCapBounds(cap));
    if (world.isEmpty()) return SkIRect::MakeEmpty();
    auto device = matrix.mapRect(world);
    if (!device.isFinite()) return viewport;
    device.outset(2, 2); // AA coverage and the linear material sampler.
    if (!device.intersect(SkRect::Make(viewport))) return SkIRect::MakeEmpty();
    return device.roundOut();
}

template<class F>
void forTiles(const SkIRect& bounds, F visit) {
    if (bounds.isEmpty()) return;
    for (int y = bounds.top() / tileSize; y <= (bounds.bottom() - 1) / tileSize; ++y)
        for (int x = bounds.left() / tileSize; x <= (bounds.right() - 1) / tileSize; ++x)
            visit(std::pair{x, y}, SkIRect::MakeXYWH(x * tileSize, y * tileSize, tileSize, tileSize));
}

void raster(SkSurface& target, const SkIRect& rect, const SkMatrix& matrix,
            const foundation::InkBrushConfig& config, const ink::CompactContour& contour) {
    auto& canvas = *target.getCanvas();
    SkAutoCanvasRestore restore(&canvas, true);
    canvas.clipIRect(SkIRect::MakeWH(rect.width(), rect.height()));
    canvas.translate(-float(rect.left()), -float(rect.top()));
    canvas.concat(matrix);
    drawCompactContourCoverageToSkCanvas(canvas, config, contour);
}
}

std::size_t CompactPreviewCoverage::bytes() const {
    return stable_.size() * tileBytes;
}

void CompactPreviewCoverage::setByteLimit(std::size_t bytes) {
    const auto slots = bytes / tileBytes;
    if (slots == tileLimit_) return;
    if (slots > tileLimit_ && overflow_) {
        // A previously uncacheable cell might be admitted now. Rebuild on the
        // next append so the newly admitted cell includes the entire prefix.
        *this = CompactPreviewCoverage{};
    }
    tileLimit_ = slots;
    while (stable_.size() > slots) {
        stable_.erase(stable_.begin());
        overflow_ = true;
    }
}

// When a cell was previously omitted or evicted it may be missing any number
// of stable chunks. Replay the immutable chain in paint order. Scratch work
// may be clipped to damage; a resident cell must recover its entire device grid.
void CompactPreviewCoverage::replay(SkSurface& surface, const SkIRect& rasterRect,
                                    const SkIRect& demand, const SkIRect& viewport) {
    std::vector<const Chunk*> intersecting;
    for (auto p = prefix_.get(); p; p = p->previous.get()) {
        ++historyWork_.checkedChunks;
        if (SkIRect::Intersects(demand, visibleBounds(p->contour, matrix_, viewport)))
            intersecting.push_back(p);
    }
    for (auto it = intersecting.rbegin(); it != intersecting.rend(); ++it)
        raster(surface, rasterRect, matrix_, config_, (*it)->contour);
    historyWork_.rasterizedChunks += intersecting.size();
}

CompactPreviewCoverage::TileMap::iterator CompactPreviewCoverage::promote(
        Cell cell, const SkIRect& rasterRect, const SkIRect& tailBounds,
        const SkIRect& damage, const SkIRect& viewport) {
    auto victim = stable_.end();
    bool victimInDamage = true;
    for (auto it = stable_.begin(); it != stable_.end(); ++it) {
        const auto [x, y] = it->first;
        const auto bounds = SkIRect::MakeXYWH(x * tileSize, y * tileSize, tileSize, tileSize);
        if (SkIRect::Intersects(bounds, tailBounds)) continue;
        const bool inDamage = SkIRect::Intersects(bounds, damage);
        if (victim == stable_.end() || (victimInDamage && !inDamage) ||
            (victimInDamage == inDamage && it->second.lastTailUse < victim->second.lastTailUse)) {
            victim = it;
            victimInDamage = inDamage;
        }
    }
    if (victim == stable_.end()) return victim;

    // Reuse the existing tile allocation. Only immutable prefix pixels are
    // installed; the replaceable tail is blended into scratch afterwards.
    auto node = stable_.extract(victim);
    auto& surface = *node.mapped().surface;
    surface.getCanvas()->clear(SK_ColorBLACK);
    replay(surface, rasterRect, rasterRect, viewport);
    node.key() = cell;
    node.mapped().lastTailUse = drawSerial_;
    ++historyWork_.promotedTiles;
    return stable_.insert(std::move(node)).position;
}

SkISize CompactPreviewCoverage::scratchSize(const PreviewGeometry& geometry, const SkMatrix& matrix,
                                                const SkIRect& viewport) {
    int width = 0, height = 0;
    for (const auto& contour : geometry.contours) {
        if (!contour.compactConfig || contour.compactConfig->algorithmVersion != 4 || !contour.compactChunks) continue;
        const auto& tail = *contour.compactChunks;
        const auto bounds = tail.previous ? viewport : visibleBounds(tail.contour, matrix, viewport);
        width = std::max(width, std::min(tileSize, bounds.width()));
        height = std::max(height, std::min(tileSize, bounds.height()));
    }
    return SkISize::Make(width, height);
}

void CompactPreviewCoverage::draw(SkiaSurfaceProvider& provider, SkSurface& surface,
                                  const PreviewGeometry& geometry, const PreviewGeometry::Contour& contour,
                                  const foundation::InkBrushConfig& config, bool soleContour,
                                  SkSurface& scratch, SkiaRenderer::CompactWork& work) {
    auto& canvas = *surface.getCanvas();
    const auto matrix = canvas.getTotalMatrix();
    const auto viewport = canvas.getDeviceClipBounds();
    const auto& tail = *contour.compactChunks;
    auto newest = tail.previous;
    ++drawSerial_;
    const bool sameBasis = provider_ == &provider && surface_ == &surface && matrix_ == matrix &&
        generation_ == geometry.surfaceGeneration.value() && session_ == contour.session &&
        document_ == geometry.documentEpoch && sessionGeneration_ == geometry.sessionGeneration &&
        config_ == config;
    std::vector<const Chunk*> added;
    auto ancestor = newest;
    while (ancestor && (!sameBasis || ancestor != prefix_)) {
        added.push_back(ancestor.get());
        ancestor = ancestor->previous;
    }
    if (!sameBasis || ancestor != prefix_) {
        stable_.clear();
        stableBounds_ = SkIRect::MakeEmpty();
        lastTail_ = SkIRect::MakeEmpty();
        invalidateDisplay();
        overflow_ = false;
    }
    provider_ = &provider;
    surface_ = &surface;
    matrix_ = matrix;
    config_ = config;
    generation_ = geometry.surfaceGeneration.value();
    session_ = contour.session;
    document_ = geometry.documentEpoch;
    sessionGeneration_ = geometry.sessionGeneration;

    const bool partial = soleContour && presentedContents_ && presents_ == provider.presentCount() &&
        presentedContents_ == surface.generationID() && provider.describe().capabilities.supportsPartialUpdate;
    invalidateDisplay();
    const SkIRect currentTail = visibleBounds(tail.contour, matrix, viewport);
    const auto previousTail = lastTail_;
    auto changed = currentTail;
    for (auto it = added.rbegin(); it != added.rend(); ++it) {
        const auto& chunk = **it;
        const auto bounds = visibleBounds(chunk.contour, matrix, viewport);
        changed.join(bounds);
        stableBounds_.join(bounds);
        work.generatedKnots += chunk.knots.size();
        forTiles(bounds, [&](Cell cell, SkIRect rect) {
            rect.intersect(viewport);
            auto found = stable_.find(cell);
            if (found == stable_.end()) {
                // After capacity is reached, uncached cells use ordered replay.
                if (overflow_ || stable_.size() >= tileLimit_) { overflow_ = true; return; }
                auto tile = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(tileSize, tileSize));
                if (!tile) { overflow_ = true; return; }
                tile->getCanvas()->clear(SK_ColorBLACK);
                found = stable_.emplace(cell, StableTile{std::move(tile), 0}).first;
            }
            raster(*found->second.surface, rect, matrix, config, chunk.contour);
        });
    }
    prefix_ = newest;
    lastTail_ = currentTail;
    work.generatedKnots += tail.knots.size();
    auto visible = stableBounds_;
    visible.join(currentTail);
    const auto paint = [&](SkIRect damage) {
        if (damage.isEmpty()) return;
        if (partial) {
            SkAutoCanvasRestore restore(&canvas, true);
            canvas.resetMatrix();
            canvas.clipIRect(damage);
            canvas.clear(SK_ColorTRANSPARENT);
        }
        if (damage.intersect(visible))
            compositeRegion(canvas, scratch, tail, contour.compactSeed, damage, currentTail, viewport);
    };
    if (!partial) {
        if (soleContour) canvas.clear(SK_ColorTRANSPARENT);
        paint(visible);
    } else if (SkIRect::Intersects(previousTail, changed)) {
        changed.join(previousTail);
        paint(changed);
    } else {
        // A replaced tail may jump across many unchanged, nonresident cells.
        // Keep disjoint damage separate; the complete pixels between them are valid.
        paint(previousTail);
        paint(changed);
    }
}

void CompactPreviewCoverage::compositeRegion(SkCanvas& canvas, SkSurface& scratch,
        const Chunk& tail, std::uint64_t seed, const SkIRect& damage,
        const SkIRect& currentTail, const SkIRect& viewport) {
    forTiles(damage, [&](Cell cell, SkIRect tile) {
        SkIRect rect = tile;
        if (!rect.intersect(damage)) return;
        auto found = stable_.find(cell);
        const bool hasTail = SkIRect::Intersects(rect, currentTail);
        sk_sp<SkImage> image;
        int originX = tile.left(), originY = tile.top();
        if (hasTail || (overflow_ && found == stable_.end() && SkIRect::Intersects(rect, stableBounds_))) {
            // For a short standalone tail use just its visible support. Once
            // a prefix is involved, raster on the canonical cell's clip/grid.
            auto rasterRect = tail.previous ? tile : rect;
            rasterRect.intersect(viewport);
            auto* combined = scratch.getCanvas();
            SkAutoCanvasRestore clip(combined, true);
            combined->clipIRect(SkIRect::MakeWH(rasterRect.width(), rasterRect.height()));
            combined->clear(SK_ColorBLACK);
            if (found == stable_.end() && hasTail && prefix_ && overflow_ &&
                SkIRect::Intersects(rasterRect, stableBounds_))
                found = promote(cell, rasterRect, currentTail, damage, viewport);
            if (found != stable_.end()) {
                if (hasTail) found->second.lastTailUse = drawSerial_;
                combined->drawImage(found->second.surface->makeImageSnapshot(),
                                    float(originX - rasterRect.left()), float(originY - rasterRect.top()));
            } else if (overflow_ && SkIRect::Intersects(rect, stableBounds_)) {
                replay(scratch, rasterRect, rect, viewport);
            }
            if (hasTail) raster(scratch, rasterRect, matrix_, config_, tail.contour);
            image = scratch.makeImageSnapshot();
            originX = rasterRect.left();
            originY = rasterRect.top();
        } else if (found != stable_.end()) {
            image = found->second.surface->makeImageSnapshot();
        }
        SkAutoCanvasRestore restore(&canvas, true);
        canvas.resetMatrix();
        canvas.clipIRect(rect);
        if (image) {
            canvas.setMatrix(matrix_);
            compositeCompactCoverageToSkCanvas(canvas, *image, config_, seed, originX, originY);
        }
    });
}

void CompactPreviewCoverage::presented(const SkiaSurfaceProvider& provider, std::uint32_t contents) {
    presents_ = provider.presentCount();
    presentedContents_ = contents;
}
} // namespace canvas::render::internal

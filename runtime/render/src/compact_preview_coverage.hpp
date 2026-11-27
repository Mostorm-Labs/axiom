#pragma once

#include "canvas/render/skia_renderer.hpp"
#include "include/core/SkSurface.h"
#include "include/core/SkMatrix.h"
#include <map>

namespace canvas::render::internal {

// Device-aligned coverage of immutable v4 chunks. The replaceable tail is
// combined with these pixels before applying the stroke paint once.
class CompactPreviewCoverage final {
public:
    // Reserve the largest required temporary before clearing any provider pixels.
    static SkISize scratchSize(const PreviewGeometry&, const SkMatrix&, const SkIRect& viewport);
    void draw(SkiaSurfaceProvider&, SkSurface&, const PreviewGeometry&,
              const PreviewGeometry::Contour&, const foundation::InkBrushConfig&,
              bool soleContour, SkSurface& scratch, SkiaRenderer::CompactWork&);
    void presented(const SkiaSurfaceProvider&, std::uint32_t contents);
    void setByteLimit(std::size_t bytes);
    [[nodiscard]] std::size_t bytes() const;
    struct HistoryWork {
        std::uint64_t checkedChunks = 0;
        std::uint64_t rasterizedChunks = 0;
        std::uint64_t promotedTiles = 0;
    };
    [[nodiscard]] HistoryWork historyWork() const noexcept { return historyWork_; }
    void invalidateDisplay() { presentedContents_ = 0; }

private:
    using Chunk = ink::BrushPreviewDelta::Chunk;
    using Cell = std::pair<int,int>;
    struct StableTile {
        sk_sp<SkSurface> surface;
        std::uint64_t lastTailUse = 0;
    };
    using TileMap = std::map<Cell, StableTile>;
    TileMap::iterator promote(Cell, const SkIRect& rasterRect, const SkIRect& tailBounds,
                              const SkIRect& damage, const SkIRect& viewport);
    void replay(SkSurface&, const SkIRect& rasterRect, const SkIRect& demand, const SkIRect& viewport);
    void compositeRegion(SkCanvas&, SkSurface& scratch, const Chunk& tail, std::uint64_t seed,
                         const SkIRect& damage, const SkIRect& tailBounds, const SkIRect& viewport);
    HistoryWork historyWork_;
    std::uint64_t drawSerial_ = 0;
    TileMap stable_;
    std::shared_ptr<const Chunk> prefix_;
    SkIRect stableBounds_ = SkIRect::MakeEmpty();
    SkIRect lastTail_ = SkIRect::MakeEmpty();
    const SkiaSurfaceProvider* provider_ = nullptr;
    const SkSurface* surface_ = nullptr;
    SkMatrix matrix_ = SkMatrix::I();
    foundation::InkBrushConfig config_;
    std::uint64_t generation_ = 0, document_ = 0, session_ = 0, sessionGeneration_ = 0;
    std::uint64_t presents_ = 0;
    std::uint32_t presentedContents_ = 0;
    std::size_t tileLimit_ = 0;
    bool overflow_ = false;
};

} // namespace canvas::render::internal

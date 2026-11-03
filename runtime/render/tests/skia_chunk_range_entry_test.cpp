#include "canvas/render/skia_scene_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"
#include "canvas/scene/geometry_chunk.hpp"
#include "canvas/semantic/compact_stroke.hpp"
#include "canvas/ink/compact_geometry.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int kWidth = 256;
constexpr int kHeight = 128;

using canvas::RuntimeSceneRecord;
using canvas::foundation::ObjectId;
using canvas::foundation::WorldRect;
using canvas::render::CompactStrokeReferenceCommand;
using canvas::render::ReferenceTraversalEntry;

struct Pixels final {
    std::vector<std::uint8_t> rgba =
        std::vector<std::uint8_t>(kWidth * kHeight * 4U);
};

Pixels read(const sk_sp<SkSurface>& surface) {
    assert(surface != nullptr);
    Pixels result;
    assert(surface->readPixels(surface->imageInfo(), result.rgba.data(),
                               kWidth * 4, 0, 0));
    return result;
}

std::vector<canvas::foundation::InkKnot> knots() {
    std::vector<canvas::foundation::InkKnot> result;
    result.reserve(600U);
    for (unsigned i = 0; i < 600U; ++i) {
        result.push_back({8.0 + static_cast<double>(i) * 0.24,
                          64.0 + 7.0 * std::sin(static_cast<double>(i) * 0.035),
                          static_cast<std::uint16_t>(18000U + (i % 180U) * 220U)});
    }
    return result;
}

canvas::foundation::InkBrushConfig config() {
    canvas::foundation::InkBrushConfig result;
    result.version = 2;
    result.algorithmVersion = 3;
    result.signalVersion = 2;
    result.family = canvas::foundation::InkFamily::kRound;
    result.roundMode = canvas::foundation::InkRoundMode::kHanddrawn;
    result.size = 18.0;
    result.thinning = 0.65;
    result.startTaper = 28.0;
    result.endTaper = 46.0;
    result.color = {0.1, 0.3, 0.8, 1.0};
    return result;
}

ReferenceTraversalEntry entry(bool chunked, bool transformed,
                              const std::vector<canvas::foundation::InkKnot>& path) {
    const auto brush = config();
    canvas::semantic::CompactStrokeRecord stroke{brush, 42U, path};
    const canvas::semantic::CompactStrokeContent content{stroke};
    RuntimeSceneRecord record;
    record.objectId = ObjectId::fromUint64(510U);
    record.kind = canvas::semantic::ObjectKind::kVectorStroke;
    record.kindVersion = 3U;
    record.content = content;
    record.geometryBounds = WorldRect{0.0F, 40.0F, 160.0F, 88.0F};
    record.visualBounds = record.geometryBounds;
    record.worldBounds = record.geometryBounds;
    record.geometryChunks = canvas::scene::buildGeometryChunks(record.content);
    if (transformed) record.transform.tx = 1.0;
    ReferenceTraversalEntry result{
        .record = std::move(record),
        .contributesPixels = true,
        .command = CompactStrokeReferenceCommand{content},
    };
    if (chunked) {
        result.geometryChunked = true;
        result.geometryChunkIndices = {1U};
    }
    return result;
}

Pixels fullDirect(const std::vector<canvas::foundation::InkKnot>& path) {
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    canvas::render::internal::drawCompactStrokeToSkCanvas(
        *surface->getCanvas(), config(), path, 42U);
    return read(surface);
}

Pixels validated(const ReferenceTraversalEntry& value) {
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    canvas::render::internal::drawValidatedIsolatedEntryToSkCanvas(
        *surface->getCanvas(), value);
    return read(surface);
}

Pixels directRange(const std::vector<canvas::foundation::InkKnot>& path) {
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kWidth, kHeight));
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    const auto total = canvas::ink::compactLength(path);
    const auto distance = canvas::ink::compactLength(std::span(path).first(256U));
    canvas::render::internal::CompactStrokeRange range{
        256U, 512U, distance, total, false, false};
    canvas::render::internal::drawCompactStrokeToSkCanvas(
        *surface->getCanvas(), config(), path, 42U, nullptr, nullptr, nullptr,
        false, nullptr, &range);
    return read(surface);
}

void eligibleChunkMatchesDirectWithinSelectedBounds() {
    const auto path = knots();
    const auto direct = fullDirect(path);
    const auto selected = validated(entry(true, false, path));
    const auto ranged = directRange(path);
    const auto chunks = canvas::scene::buildGeometryChunks(
        canvas::semantic::ObjectContent{canvas::semantic::CompactStrokeContent{
            canvas::semantic::CompactStrokeRecord{config(), 42U, path}}});
    assert(chunks.size() >= 3U);
    const auto bounds = chunks[1].bounds;
    constexpr int kGutter = 16;
    bool observed = false;
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            if (x < static_cast<int>(std::floor(bounds.left)) + kGutter ||
                x > static_cast<int>(std::ceil(bounds.right)) - kGutter) continue;
            const auto offset = static_cast<std::size_t>(y * kWidth + x) * 4U;
            assert(selected.rgba[offset + 3U] == ranged.rgba[offset + 3U]);
            // Direct parity is required for the interior of the selected
            // chunk; the gutter is deliberately excluded because its pixels
            // may be contributed by an adjacent chunk's finite support.
            assert(selected.rgba[offset + 3U] == direct.rgba[offset + 3U]);
            observed = observed || selected.rgba[offset + 3U] != 0U;
        }
    }
    assert(observed);
}

void unsafeChunkMetadataFallsBackToDirect() {
    const auto path = knots();
    assert(validated(entry(true, true, path)).rgba ==
           validated(entry(false, true, path)).rgba);
}

} // namespace

int main() {
    eligibleChunkMatchesDirectWithinSelectedBounds();
    unsafeChunkMetadataFallsBackToDirect();
    return 0;
}

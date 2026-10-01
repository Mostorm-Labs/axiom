#include "canvas/render/skia_scene_renderer.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cassert>
#include <vector>

int main() {
    constexpr int kSize = 256;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
    assert(surface);
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    const std::vector<canvas::semantic::DabInstance> dabs{
        {{32.0, 128.0}, 32.0, 0.0F, 0.8F},
        {{64.0, 128.0}, 32.0, 0.0F, 0.8F},
        {{96.0, 128.0}, 32.0, 0.0F, 0.8F},
        {{128.0, 128.0}, 32.0, 0.0F, 0.8F},
        {{160.0, 128.0}, 32.0, 0.0F, 0.8F},
        {{192.0, 128.0}, 32.0, 0.0F, 0.8F},
    };
    canvas::render::internal::drawContinuousChalkDabsToSkCanvas(
        *surface->getCanvas(), dabs, {0.1F, 0.1F, 0.1F, 1.0F}, 4U);
    const auto info = SkImageInfo::MakeN32Premul(kSize, kSize);
    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSize) * kSize * 4U);
    assert(surface->readPixels(info, pixels.data(), static_cast<std::size_t>(kSize) * 4U, 0, 0));
    auto alpha = [&](int x, int y) { return pixels[(static_cast<std::size_t>(y) * kSize + x) * 4U + 3U]; };
    // A continuous coverage stroke must not expose independent stamp seams.
    for (int x = 48; x <= 176; x += 16) {
        assert(alpha(x, 128) > 0U);
    }
    // Texture should vary within the covered body, rather than becoming a
    // uniform opaque ribbon or a regular comb of dab edges.
    std::size_t distinct = 0;
    bool seen[256]{};
    for (int x = 24; x <= 200; ++x) {
        const auto a = alpha(x, 128);
        if (!seen[a]) { seen[a] = true; ++distinct; }
    }
    assert(distinct > 4U);

    // The screenshot-derived shape is intentionally asymmetric.  A continuous
    // chalk stroke must carry that directional coverage into the body; a plain
    // round stroke with only a repeating grain texture produces matching
    // coverage at these mirrored samples.
    std::size_t asymmetricSamples = 0U;
    for (int x = 48; x <= 176; x += 8) {
        if (alpha(x, 112) != alpha(x, 144)) ++asymmetricSamples;
    }
    assert(asymmetricSamples >= 4U);
    return 0;
}

#include "canvas/render/skia_scene_renderer.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkSurface.h"

#include <cassert>
#include <array>
#include <cstdint>
#include <vector>

int main() {
    constexpr int kSize = 160;
    auto surface = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
    assert(surface);
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);

    std::vector<canvas::semantic::DabInstance> dabs{
        canvas::semantic::DabInstance{{80.0, 80.0}, 72.0, 0.0F, 1.0F}};
    canvas::render::internal::drawChalkDabsToSkCanvas(
        *surface->getCanvas(), dabs, {0.1F, 0.2F, 0.9F, 1.0F}, 3U);

    std::vector<std::uint8_t> pixels(static_cast<std::size_t>(kSize) * kSize * 4U);
    const auto info = SkImageInfo::Make(kSize, kSize, kRGBA_8888_SkColorType,
                                        kUnpremul_SkAlphaType,
                                        SkColorSpace::MakeSRGB());
    assert(surface->readPixels(info, pixels.data(), static_cast<std::size_t>(kSize) * 4U, 0, 0));
    auto alphaAt = [&](int x, int y) -> std::uint8_t {
        return pixels[(static_cast<std::size_t>(y) * kSize + x) * 4U + 3U];
    };
    // The real texture must have a continuous, non-grid edge: neighboring
    // samples should contain more than a binary 8x8 cell transition.
    std::size_t nonZero = 0U;
    std::size_t partial = 0U;
    std::array<bool, 256> seen{};
    std::size_t distinct = 0U;
    for (int y = 20; y < 140; ++y) {
        for (int x = 20; x < 140; ++x) {
            const auto alpha = alphaAt(x, y);
            nonZero += alpha != 0U;
            partial += alpha > 0U && alpha < 245U;
            if (!seen[alpha]) { seen[alpha] = true; ++distinct; }
        }
    }
    assert(nonZero > 100U);
    assert(partial > 100U);
    assert(distinct > 60U);
    // The source shape has an empty background. Its dab must not expose the
    // rectangular image bounds, even where the sampled grain is nonzero.
    assert(alphaAt(50, 50) == 0U);

    // A stroke must preserve the BrushEngine's varying dab sizes and the
    // screenshot-derived shape mask; replacing it with one average-width
    // line is explicitly forbidden.
    auto varying = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
    assert(varying);
    varying->getCanvas()->clear(SK_ColorTRANSPARENT);
    const std::vector<canvas::semantic::DabInstance> varyingDabs{
        {{40.0, 80.0}, 30.0, 0.0F, 0.8F},
        {{100.0, 80.0}, 8.0, 0.0F, 0.8F}};
    canvas::render::internal::drawChalkDabsToSkCanvas(
        *varying->getCanvas(), varyingDabs, {0.1F, 0.2F, 0.9F, 1.0F}, 3U);
    std::vector<std::uint8_t> varyingPixels(static_cast<std::size_t>(kSize) * kSize * 4U);
    assert(varying->readPixels(info, varyingPixels.data(), static_cast<std::size_t>(kSize) * 4U, 0, 0));
    const auto varyingAlpha = [&](int x, int y) {
        return varyingPixels[(static_cast<std::size_t>(y) * kSize + x) * 4U + 3U];
    };
    std::size_t firstCoverage = 0U;
    std::size_t secondCoverage = 0U;
    for (int y = 70; y <= 90; ++y) {
        for (int x = 30; x <= 50; ++x) firstCoverage += varyingAlpha(x, y);
        for (int x = 90; x <= 110; ++x) secondCoverage += varyingAlpha(x, y);
    }
    assert(firstCoverage > secondCoverage + 100U);

    // Membrane's extracted 1024px shape contains real transparent/low-alpha
    // holes.  Mapping the full resource into a dab must preserve those holes;
    // using the old 256px scale only samples a corner of the source image.
    auto membrane = SkSurfaces::Raster(SkImageInfo::MakeN32Premul(kSize, kSize));
    assert(membrane);
    membrane->getCanvas()->clear(SK_ColorTRANSPARENT);
    const std::vector<canvas::semantic::DabInstance> membraneDabs{
        {{80.0, 80.0}, 120.0, 0.0F, 1.0F}};
    canvas::render::internal::drawMembraneDabsToSkCanvas(
        *membrane->getCanvas(), membraneDabs, {0.1F, 0.2F, 0.9F, 1.0F});
    std::vector<std::uint8_t> membranePixels(static_cast<std::size_t>(kSize) * kSize * 4U);
    assert(membrane->readPixels(info, membranePixels.data(), static_cast<std::size_t>(kSize) * 4U, 0, 0));
    const auto membraneAlpha = [&](int x, int y) {
        return membranePixels[(static_cast<std::size_t>(y) * kSize + x) * 4U + 3U];
    };
    assert(membraneAlpha(64, 51) < 32U);
    assert(membraneAlpha(72, 51) > membraneAlpha(64, 51) + 32U);
    return 0;
}

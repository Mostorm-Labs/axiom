#include "retained_preview_test_target.hpp"
#include "../src/compact_preview_coverage.hpp"
#include "canvas/ink/brush_package_catalog.hpp"
#include "canvas/render/skia_scene_renderer.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkImage.h"
#include <iostream>
#include <stdexcept>

using namespace canvas;
namespace {
using Chunk = ink::BrushPreviewDelta::Chunk;
using Coverage = render::internal::CompactPreviewCoverage;

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}
void accepted(const render::BackendSubmissionResult& result) {
    check(result.code == render::BackendSubmissionCode::kAccepted, "raster target unavailable");
}
std::shared_ptr<const Chunk> withCircle(std::shared_ptr<const Chunk> previous, double x, double y) {
    auto chunk = std::make_shared<Chunk>();
    chunk->previous = std::move(previous);
    chunk->contour.caps.push_back({x, y, 5.5});
    return chunk;
}

void exercise() {
    constexpr int width = 6 * 256, height = 96;
    constexpr std::size_t byteLimit = 4 * 256 * 256 * 4;
    render::testing::RetainedPreviewTarget target;
    accepted(target.resize(width,height));
    auto config = *ink::BrushPackageCatalog{}.loadDefault("round-handdrawn", 4).package.compactConfig;
    config.opacity = .45;
    config.color = {.15,.6,.25,.8};
    render::PreviewGeometry geometry;
    geometry.documentEpoch = geometry.sessionGeneration = 1;
    geometry.surfaceGeneration = render::SurfaceGeneration{target.generation()};
    render::PreviewGeometry::Contour contour;
    contour.session = 1;
    std::shared_ptr<const Chunk> prefix;
    for (int cell=0; cell<6; ++cell) {
        // The second stable circle lies outside the changing tail damage.
        prefix=withCircle(prefix, cell*256+120, 35);
        prefix=withCircle(prefix, cell*256+175, 65);
    }
    auto scratch=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(256,256));
    auto refCoverage=SkSurfaces::Raster(SkImageInfo::MakeN32Premul(width,height));
    render::RasterSkiaSurfaceProvider reference;
    accepted(reference.resize(width,height));
    Coverage cache;
    cache.setByteLimit(byteLimit);
    render::SkiaRenderer::CompactWork work{};
    std::vector<std::uint8_t> actual(width*height*4),expected(actual.size());
    Coverage::HistoryWork last{};
    for (unsigned step=0; step<20; ++step) {
        const int cell = step < 3 ? 5 : step < 10 ? 4 + (step % 2) : (step % 2) * 5;
        if(step == 2) prefix=withCircle(prefix,5*256+125,35);
        contour.compactChunks=withCircle(prefix,cell*256+120+step%3,34);
        if(step == 17) cache.setByteLimit(0);
        if(step == 18) {
            cache.setByteLimit(byteLimit);
            accepted(target.advanceGeneration());
            geometry.surfaceGeneration=render::SurfaceGeneration{target.generation()};
        }
        auto acquired=target.acquire();
        auto& surface=*acquired.frame.surface;
        cache.draw(target,surface,geometry,contour,config,true,*scratch,work);
        auto contents=surface.generationID();
        target.release();accepted(target.present());cache.presented(target,contents);

        refCoverage->getCanvas()->clear(SK_ColorBLACK);
        std::vector<const Chunk*> history;
        for (auto c=contour.compactChunks.get(); c; c=c->previous.get()) history.push_back(c);
        for (auto it=history.rbegin(); it!=history.rend(); ++it)
            render::internal::drawCompactContourCoverageToSkCanvas(*refCoverage->getCanvas(), config, (*it)->contour);
        auto ref=reference.acquire();
        auto& canvas=*ref.frame.surface->getCanvas();
        canvas.clear(SK_ColorTRANSPARENT);
        render::internal::compositeCompactCoverageToSkCanvas(canvas,*refCoverage->makeImageSnapshot(),config,0);
        reference.release();accepted(reference.present());
        accepted(reference.readbackRgba(expected));accepted(target.readbackRgba(actual));
        check(actual==expected, "partial or stale prefix pixels in full overlay");
        check(cache.bytes() <= byteLimit, "tile promotion exceeded budget");
        auto now=cache.historyWork();
        if (step==1 || step==2 || (step>=5 && step<10) || (step>=12 && step<17)) {
            check(now.checkedChunks==last.checkedChunks && now.rasterizedChunks==last.rasterizedChunks,
                  "repeated moving tail replayed resident stable prefix");
        }
        if(step==9) check(now.promotedTiles>=2, "both hot cells should remain resident");
        if(step==17) check(cache.bytes()==0, "uncached fallback retained a tile");
        last=now;
    }
}
} // namespace

int main() {
    try { exercise(); return 0; }
    catch (const std::exception& error) {std::cerr << error.what() << '\n'; return 1;}
}

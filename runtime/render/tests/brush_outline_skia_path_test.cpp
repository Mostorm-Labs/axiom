#include "canvas/render/brush_outline_skia_path.hpp"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#ifndef G45_SOURCE_ROOT
#error G45_SOURCE_ROOT must be defined for the ownership oracle
#endif

namespace {
std::string read(const char* path) {
    std::ifstream input(std::string(G45_SOURCE_ROOT) + path);
    return {std::istreambuf_iterator<char>(input), {}};
}
}

int main() {
    const auto previewSource = read("/runtime/render/src/skia_renderer.cpp");
    const auto canonicalSource = read("/runtime/render/src/skia_headless_backend.cpp");
    assert(previewSource.find("buildVectorBrushOutlineSkPath") != std::string::npos);
    assert(canonicalSource.find("buildVectorBrushOutlineSkPath") != std::string::npos);
    assert(previewSource.find("drawPreviewDabsToSkCanvas") != std::string::npos);
    assert(previewSource.find("drawPreviewChalkDabsToSkCanvas") != std::string::npos);
    assert(previewSource.find("drawPreviewMembraneDabsToSkCanvas") != std::string::npos);
    assert(canonicalSource.find("drawDabInstancesToSkCanvas") != std::string::npos);
    assert(canonicalSource.find("drawChalkDabsToSkCanvas") != std::string::npos);
    assert(canonicalSource.find("drawMembraneDabsToSkCanvas") != std::string::npos);

    const std::vector<canvas::ink::reference::StrokeOutlinePoint> previewOutline{
        {1.0, 2.0}, {11.0, 2.0}, {11.0, 12.0}, {1.0, 12.0}};
    const auto preview = canvas::render::buildVectorBrushOutlineSkPath(previewOutline, 1U, true);
    assert(preview.countPoints() == 4);
    assert(preview.getFillType() == SkPathFillType::kWinding);
    assert(preview.isLastContourClosed());
    assert(preview.getPoint(0).x() == 1.0F && preview.getPoint(0).y() == 2.0F);
    assert(preview.getPoint(3).x() == 1.0F && preview.getPoint(3).y() == 12.0F);

    const std::vector<canvas::semantic::Vec2> canonicalOutline{
        {0.0, 0.0}, {4.0, 0.0}, {4.0, 4.0}};
    const auto canonical = canvas::render::buildVectorBrushOutlineSkPath(canonicalOutline, 2U, false);
    assert(canonical.countPoints() == 3);
    assert(canonical.getFillType() == SkPathFillType::kEvenOdd);
    assert(!canonical.isLastContourClosed());

    const auto empty = canvas::render::buildVectorBrushOutlineSkPath(
        std::span<const canvas::semantic::Vec2>{}, 1U, true);
    assert(empty.isEmpty());
    return 0;
}

#include "canvas/render/brush_outline_skia_path.hpp"

#include "include/core/SkPathBuilder.h"

namespace canvas::render {
namespace {

SkPathFillType fillType(std::uint32_t fillRule) noexcept {
    return fillRule == 2U ? SkPathFillType::kEvenOdd : SkPathFillType::kWinding;
}

template <typename Point>
SkPath build(std::span<const Point> outline, std::uint32_t fillRule, bool closed) {
    SkPathBuilder path(fillType(fillRule));
    if (outline.empty()) return path.detach();
    path.moveTo(static_cast<float>(outline.front().x),
                static_cast<float>(outline.front().y));
    for (std::size_t index = 1U; index < outline.size(); ++index) {
        path.lineTo(static_cast<float>(outline[index].x),
                    static_cast<float>(outline[index].y));
    }
    if (closed) path.close();
    return path.detach();
}

}  // namespace

SkPath buildVectorBrushOutlineSkPath(
    std::span<const ink::reference::StrokeOutlinePoint> outline,
    std::uint32_t fillRule, bool closed) {
    return build(outline, fillRule, closed);
}

SkPath buildVectorBrushOutlineSkPath(
    std::span<const semantic::Vec2> outline,
    std::uint32_t fillRule, bool closed) {
    return build(outline, fillRule, closed);
}

}  // namespace canvas::render

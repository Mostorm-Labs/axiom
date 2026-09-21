#include "canvas/render/skia_brush_renderer.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkRect.h"

#include <algorithm>
#include <cmath>

namespace canvas::render::internal {

void drawBrushPrimitivesToSkCanvas(
    SkCanvas& canvas,
    std::span<const canvas::ink::BrushPrimitive> primitives) {
    for (const auto& dab : primitives) {
        if (!std::isfinite(dab.x) || !std::isfinite(dab.y) ||
            !std::isfinite(dab.size) || !std::isfinite(dab.rotation) ||
            !std::isfinite(dab.opacity) || dab.size <= 0.0F) {
            continue;
        }
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f({0.10F, 0.36F, 0.95F,
                          std::clamp(dab.opacity, 0.0F, 1.0F)}, nullptr);
        const float radius = std::max(0.5F, dab.size * 0.5F);
        canvas.save();
        canvas.rotate(dab.rotation * 57.2957795F, dab.x, dab.y);
        if (dab.representation == canvas::ink::BrushRepresentation::kVector) {
            canvas.drawCircle(dab.x, dab.y, radius, paint);
        } else {
            const float shapeAspect = dab.shapeResource.valid() &&
                    (dab.shapeResource.value & 1U) ? 0.72F : 1.0F;
            const auto oval = SkRect::MakeLTRB(
                dab.x - radius, dab.y - radius * shapeAspect,
                dab.x + radius, dab.y + radius * shapeAspect);
            canvas.drawOval(oval, paint);
            if (dab.grainResource.valid()) {
                SkPaint grain = paint;
                grain.setAlpha(static_cast<U8CPU>(
                    std::clamp(dab.opacity, 0.0F, 1.0F) * 72.0F));
                const int bands = 3 + static_cast<int>(dab.grainResource.value % 3U);
                for (int i = 0; i < bands; ++i) {
                    const float offset = (static_cast<float>(i) -
                        (bands - 1) * 0.5F) * radius * 0.24F;
                    canvas.drawCircle(dab.x + offset, dab.y - offset * 0.3F,
                                      std::max(0.5F, radius * 0.24F), grain);
                }
            }
        }
        canvas.restore();
    }
}

}  // namespace canvas::render::internal

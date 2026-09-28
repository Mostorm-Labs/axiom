#include "canvas/render/skia_scene_renderer.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkPaint.h"

#include <algorithm>
#include <cmath>

namespace canvas::render::internal {

void drawDabInstancesToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color) {
    for (const auto& dab : dabs) {
        if (!std::isfinite(dab.center.x) || !std::isfinite(dab.center.y) ||
            !std::isfinite(dab.size) || dab.size <= 0.0 ||
            !std::isfinite(dab.rotation) || !std::isfinite(dab.opacity)) continue;
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f({color.r, color.g, color.b,
                          std::clamp(color.a * dab.opacity, 0.0F, 1.0F)}, nullptr);
        canvas.save();
        canvas.rotate(dab.rotation * 57.2957795F,
                      static_cast<float>(dab.center.x),
                      static_cast<float>(dab.center.y));
        canvas.drawOval(SkRect::MakeLTRB(
            static_cast<float>(dab.center.x - dab.size * 0.5),
            static_cast<float>(dab.center.y - dab.size * 0.5),
            static_cast<float>(dab.center.x + dab.size * 0.5),
            static_cast<float>(dab.center.y + dab.size * 0.5)), paint);
        canvas.restore();
    }
}

void drawPreviewDabsToSkCanvas(
    SkCanvas& canvas, std::span<const ink::BrushDab> dabs,
    float red, float green, float blue, float alpha) {
    for (const auto& dab : dabs) {
        if (!std::isfinite(dab.x) || !std::isfinite(dab.y) ||
            !std::isfinite(dab.size) || dab.size <= 0.0 ||
            !std::isfinite(dab.rotation) || !std::isfinite(dab.opacity)) continue;
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f({red, green, blue,
                          std::clamp(alpha * dab.opacity, 0.0F, 1.0F)}, nullptr);
        canvas.save();
        canvas.rotate(dab.rotation * 57.2957795F,
                      static_cast<float>(dab.x), static_cast<float>(dab.y));
        canvas.drawOval(SkRect::MakeLTRB(
            static_cast<float>(dab.x - dab.size * 0.5),
            static_cast<float>(dab.y - dab.size * 0.5),
            static_cast<float>(dab.x + dab.size * 0.5),
            static_cast<float>(dab.y + dab.size * 0.5)), paint);
        canvas.restore();
    }
}

} // namespace canvas::render::internal

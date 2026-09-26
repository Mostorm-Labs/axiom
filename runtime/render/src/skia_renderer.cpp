#include "canvas/render/skia_renderer.hpp"

#include "canvas/render/skia_brush_renderer.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/render/skia_scene_renderer.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"

#include <cmath>
#include <algorithm>

namespace canvas::render {

BackendSubmissionResult SkiaRenderer::renderFrame(
    SkiaSurfaceProvider& provider, const FramePlan& plan) {
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != plan.frame.surfaceGeneration.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale Skia surface generation");
    }
    const auto result = internal::drawReferencePlanToSkCanvas(
        *acquired.frame.surface->getCanvas(), plan);
    provider.release();
    if (result.code != BackendSubmissionCode::kAccepted) return result;
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::renderPreview(
    SkiaSurfaceProvider& provider, const PreviewGeometry& geometry,
    const PreviewStyleOverride& style) {
    if (geometry.revision == 0U || geometry.surfaceGeneration.value() == 0U ||
        !std::isfinite(geometry.viewport.scale) || geometry.viewport.scale <= 0.0F ||
        !std::isfinite(geometry.viewport.translationX) ||
        !std::isfinite(geometry.viewport.translationY) ||
        provider.generation() != geometry.surfaceGeneration.value()) {
        return BackendSubmissionResult::rejected("invalid preview geometry or surface generation");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != geometry.surfaceGeneration.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale preview surface generation");
    }
    auto* canvas = acquired.frame.surface->getCanvas();
    canvas->clear(SK_ColorTRANSPARENT);
    canvas->save();
    canvas->translate(geometry.viewport.translationX, geometry.viewport.translationY);
    canvas->scale(geometry.viewport.scale, geometry.viewport.scale);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    const auto blend = style.blendMode == 0U ? SkBlendMode::kSrcOver
                                              : static_cast<SkBlendMode>(style.blendMode);
    paint.setBlendMode(blend);
    // The preview contract defines opacityMultiplier as the effective
    // overlay opacity. The RGBA style alpha is descriptive metadata.
    const float alpha = style.overrideOpacity
        ? std::clamp(style.opacityMultiplier, 0.0F, 1.0F)
        : 1.0F;
    paint.setColor4f({style.overrideColor ? style.red : 0.10F,
                      style.overrideColor ? style.green : 0.36F,
                      style.overrideColor ? style.blue : 0.95F, alpha}, nullptr);
    auto drawOutline = [&](const auto& outline) {
        if (outline.empty()) return;
        SkPathBuilder path;
        path.moveTo(static_cast<float>(outline.front().x),
                    static_cast<float>(outline.front().y));
        for (std::size_t i = 1; i < outline.size(); ++i) {
            path.lineTo(static_cast<float>(outline[i].x),
                        static_cast<float>(outline[i].y));
        }
        path.close();
        canvas->drawPath(path.detach(), paint);
    };
    if (!geometry.contours.empty()) {
        for (const auto& contour : geometry.contours) drawOutline(contour.outline);
    } else {
        drawOutline(geometry.outline);
    }
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::clearPreview(
    SkiaSurfaceProvider& provider, SurfaceGeneration generation) {
    if (generation.value() == 0U || provider.generation() != generation.value()) {
        return BackendSubmissionResult::rejected("stale preview clear generation");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != generation.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale preview clear surface generation");
    }
    acquired.frame.surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::renderPrimitives(
    SkiaSurfaceProvider& provider,
    std::span<const canvas::ink::BrushPrimitive> primitives,
    float scale, float translationX, float translationY) {
    if (!std::isfinite(scale) || scale <= 0.0F || !std::isfinite(translationX) || !std::isfinite(translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    auto* canvas = acquired.frame.surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    canvas->save();
    canvas->translate(translationX, translationY);
    canvas->scale(scale, scale);
    internal::drawBrushPrimitivesToSkCanvas(*canvas, primitives);
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

BackendSubmissionResult SkiaRenderer::renderBrushPoints(
    SkiaSurfaceProvider& provider, std::span<const BrushRenderPoint> points,
    float scale, float translationX, float translationY) {
    std::vector<canvas::ink::BrushPrimitive> primitives;
    primitives.reserve(points.size());
    for (const auto& point : points) {
        canvas::ink::BrushPrimitive primitive;
        primitive.x = point.x;
        primitive.y = point.y;
        primitive.size = point.size;
        primitive.rotation = point.rotation;
        primitive.opacity = point.opacity;
        primitive.representation = static_cast<canvas::ink::BrushRepresentation>(point.representation);
        primitives.push_back(primitive);
    }
    return renderPrimitives(provider, primitives, scale, translationX, translationY);
}

BackendSubmissionResult SkiaRenderer::renderStrokes(
    SkiaSurfaceProvider& provider,
    std::span<const std::vector<CanonicalStrokePoint>> strokes,
    float scale, float translationX, float translationY) {
    if (!std::isfinite(scale) || scale <= 0.0F || !std::isfinite(translationX) || !std::isfinite(translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    auto* canvas = acquired.frame.surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    canvas->save();
    canvas->translate(translationX, translationY);
    canvas->scale(scale, scale);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLUE);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kRound_Join);
    paint.setStrokeWidth(3.0F);
    for (const auto& stroke : strokes) {
        if (stroke.empty()) continue;
        if (stroke.size() == 1U) { canvas->drawCircle(stroke.front().x, stroke.front().y, 1.5F, paint); continue; }
        SkPathBuilder path;
        path.moveTo(stroke.front().x, stroke.front().y);
        if (stroke.size() == 2U) path.lineTo(stroke.back().x, stroke.back().y);
        else for (std::size_t i = 0; i + 1U < stroke.size(); ++i) {
            const auto& p0 = stroke[i == 0U ? i : i - 1U];
            const auto& p1 = stroke[i];
            const auto& p2 = stroke[i + 1U];
            const auto& p3 = stroke[i + 2U < stroke.size() ? i + 2U : i + 1U];
            path.cubicTo(p1.x + (p2.x - p0.x) / 6.0F, p1.y + (p2.y - p0.y) / 6.0F,
                         p2.x - (p3.x - p1.x) / 6.0F, p2.y - (p3.y - p1.y) / 6.0F, p2.x, p2.y);
        }
        canvas->drawPath(path.detach(), paint);
    }
    canvas->restore();
    provider.release();
    ++submissions_;
    ++rasterizations_;
    return provider.present();
}

} // namespace canvas::render

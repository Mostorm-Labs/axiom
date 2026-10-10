#include "canvas/render/skia_renderer.hpp"

#include "canvas/render/skia_brush_renderer.hpp"
#include "canvas/render/brush_outline_skia_path.hpp"
#include "canvas/render/skia_ink_backend.hpp"
#include "canvas/render/skia_scene_renderer.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkBlendMode.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"

#include <cmath>
#include <algorithm>
#include <type_traits>

namespace canvas::render {

BackendSubmissionResult SkiaRenderer::renderFrame(
    SkiaSurfaceProvider& provider, const FramePlan& plan,
    const EditingOverlay* selectionOverlay) {
    const auto acquired = provider.acquire();
    if (acquired.code != SkiaSurfaceAcquireCode::kAcquired || acquired.frame.surface == nullptr) {
        return BackendSubmissionResult::rejected(acquired.message);
    }
    if (acquired.frame.generation != plan.frame.surfaceGeneration.value()) {
        provider.release();
        return BackendSubmissionResult::rejected("stale Skia surface generation");
    }
    if (plan.referenceDrawList.frame.surfaceGeneration != plan.frame.surfaceGeneration ||
        plan.referenceDrawList.frame.frameId != plan.frame.frameId ||
        plan.referenceDrawList.frame.sceneGeneration != plan.frame.sceneGeneration) {
        provider.release();
        return BackendSubmissionResult::rejected("inconsistent FramePlan identity");
    }
    if (plan.referenceDrawList.entries.empty()) {
        acquired.frame.surface->getCanvas()->clear(SK_ColorWHITE);
        provider.release();
        ++submissions_;
        ++rasterizations_;
        return provider.present();
    }
    const auto result = internal::drawReferencePlanToSkCanvas(
        *acquired.frame.surface->getCanvas(), plan, images_, &textResources_);
    if (result.code == BackendSubmissionCode::kAccepted && selectionOverlay != nullptr &&
        selectionOverlay->selectionOutline().visible) {
        auto* canvas = acquired.frame.surface->getCanvas();
        SkPaint guidePaint;
        guidePaint.setAntiAlias(true);
        guidePaint.setStyle(SkPaint::kStroke_Style);
        guidePaint.setStrokeWidth(1.0F);
        guidePaint.setColor4f({0.18F, 0.72F, 0.86F, 0.78F}, nullptr);
        for (const auto& guide : selectionOverlay->guides()) {
            canvas->drawLine(guide.start.x, guide.start.y, guide.end.x, guide.end.y,
                             guidePaint);
        }
        SkPaint chrome;
        chrome.setAntiAlias(true);
        chrome.setStyle(SkPaint::kStroke_Style);
        chrome.setStrokeWidth(2.0F);
        chrome.setColor4f({0.16F, 0.72F, 0.82F, 0.95F}, nullptr);
        const auto& outline = selectionOverlay->selectionOutline();
        SkPathBuilder path;
        path.moveTo(outline.corners[0].x, outline.corners[0].y);
        path.lineTo(outline.corners[1].x, outline.corners[1].y);
        path.lineTo(outline.corners[2].x, outline.corners[2].y);
        path.lineTo(outline.corners[3].x, outline.corners[3].y);
        path.close();
        canvas->drawPath(path.detach(), chrome);
        chrome.setStyle(SkPaint::kFill_Style);
        for (const auto kind : {HandleKind::kTopLeft, HandleKind::kTop,
                                HandleKind::kTopRight, HandleKind::kRight,
                                HandleKind::kBottomRight, HandleKind::kBottom,
                                HandleKind::kBottomLeft, HandleKind::kLeft,
                                HandleKind::kRotation}) {
            const auto& handle = selectionOverlay->handle(kind);
            if (handle.kind == HandleKind::kNone) continue;
            canvas->drawCircle(handle.center.x, handle.center.y,
                               handle.visualRadius, chrome);
        }
    }
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
    // The default PreviewStyleOverride is a pale blue-green transient tint;
    // callers such as the partial eraser can still provide an explicit color.
    paint.setColor4f({style.overrideColor ? style.red : 0.20F,
                      style.overrideColor ? style.green : 0.78F,
                      style.overrideColor ? style.blue : 0.72F, alpha}, nullptr);
    auto drawOutline = [&](const auto& outline, const SkPaint& outlinePaint) {
        if (outline.empty()) return;
        canvas->drawPath(buildVectorBrushOutlineSkPath(
            std::span<const std::remove_cvref_t<decltype(outline.front())>>(
                outline.data(), outline.size())), outlinePaint);
    };
    auto drawDabs = [&](const auto& dabs, const SkPaint& dabPaint) {
        if constexpr (std::is_same_v<std::decay_t<decltype(dabs)>,
                                     std::vector<canvas::ink::BrushDab>>) {
            const auto materialRevision = dabs.empty() ? 0U : dabs.front().materialRevision;
            const auto materialMode = dabs.empty() ? 0U : dabs.front().materialMode;
            if (materialMode == 4U) {
                internal::drawPreviewMembraneDabsToSkCanvas(*canvas, dabs,
                    dabPaint.getColor4f().fR, dabPaint.getColor4f().fG,
                    dabPaint.getColor4f().fB, dabPaint.getColor4f().fA);
            } else if (materialRevision >= 3U) {
                internal::drawPreviewChalkDabsToSkCanvas(*canvas, dabs,
                    dabPaint.getColor4f().fR, dabPaint.getColor4f().fG,
                    dabPaint.getColor4f().fB, dabPaint.getColor4f().fA, materialRevision);
            } else {
                internal::drawPreviewDabsToSkCanvas(*canvas, dabs,
                    dabPaint.getColor4f().fR, dabPaint.getColor4f().fG,
                    dabPaint.getColor4f().fB, dabPaint.getColor4f().fA);
            }
        }
    };
    auto drawContour = [&](const PreviewGeometry::Contour& contour) {
        SkPaint contourPaint = paint;
        if (contour.hasPaint && style.preferCapturedPaint) {
            const float effectiveAlpha = std::clamp(contour.alpha * contour.opacity, 0.0F, 1.0F);
            contourPaint.setColor4f({contour.red, contour.green, contour.blue,
                                     effectiveAlpha}, nullptr);
        }
        if (contour.dabs.empty()) drawOutline(contour.outline, contourPaint);
        else drawDabs(contour.dabs, contourPaint);
    };
    if (!geometry.contours.empty()) {
        for (const auto& contour : geometry.contours) drawContour(contour);
    } else {
        if (geometry.dabs.empty()) drawOutline(geometry.outline, paint);
        else drawDabs(geometry.dabs, paint);
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

#include "canvas/render/skia_scene_renderer.hpp"

#include "include/codec/SkCodec.h"
#include "include/core/SkBitmap.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkColor.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSamplingOptions.h"
#include "include/core/SkShader.h"
#include "include/core/SkTileMode.h"
#include "include/core/SkPathBuilder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <span>

#include "chalk_grain_png.hpp"
#include "chalk_shape_png.hpp"
#include "membrane_shape_png.hpp"
#include "membrane_grain_png.hpp"

namespace canvas::render::internal {

namespace {
sk_sp<SkImage> decodeTexture(std::span<const std::uint8_t> encoded, bool shape) {
    auto data = SkData::MakeWithoutCopy(encoded.data(), encoded.size());
    auto codec = SkCodec::MakeFromData(std::move(data));
    if (!codec) return nullptr;
    SkBitmap decoded;
    const auto colorInfo = SkImageInfo::MakeN32Premul(codec->dimensions());
    if (!decoded.tryAllocPixels(colorInfo) ||
        codec->getPixels(colorInfo, decoded.getPixels(), decoded.rowBytes()) !=
            SkCodec::Result::kSuccess) {
        return nullptr;
    }
    SkBitmap bitmap;
    if (!bitmap.tryAllocPixels(SkImageInfo::MakeA8(codec->dimensions()))) return nullptr;
    for (int y = 0; y < bitmap.height(); ++y) {
        auto* row = bitmap.getAddr8(0, y);
        const auto* source = decoded.getAddr32(0, y);
        for (int x = 0; x < bitmap.width(); ++x) {
            const float value = static_cast<float>(SkColorGetR(source[x]));
            const float normalized = shape
                ? std::clamp((value - 34.0F) / 205.0F, 0.0F, 1.0F)
                : std::clamp(0.48F + value / 510.0F, 0.0F, 1.0F);
            row[x] = static_cast<std::uint8_t>(normalized * 255.0F + 0.5F);
        }
    }
    return bitmap.asImage();
}

const sk_sp<SkImage>& chalkShapeImage() {
    static const sk_sp<SkImage> image = decodeTexture(
        std::span<const std::uint8_t>(kCanvasChalkShapePng, kCanvasChalkShapePngSize), true);
    return image;
}

const sk_sp<SkImage>& chalkGrainImage() {
    static const sk_sp<SkImage> image = decodeTexture(
        std::span<const std::uint8_t>(kCanvasChalkGrainPng, kCanvasChalkGrainPngSize), false);
    return image;
}

const sk_sp<SkImage>& membraneShapeImage() {
    static const sk_sp<SkImage> image = decodeTexture(
        std::span<const std::uint8_t>(kCanvasMembraneShapePng, kCanvasMembraneShapePngSize), true);
    return image;
}

const sk_sp<SkImage>& membraneGrainImage() {
    static const sk_sp<SkImage> image = decodeTexture(
        std::span<const std::uint8_t>(kCanvasMembraneGrainPng, kCanvasMembraneGrainPngSize), false);
    return image;
}

SkMatrix dabTextureMatrix(float size, float texturePixels, float offsetX = 0.0F,
                          float offsetY = 0.0F) {
    const float pixelsToWorld = std::max(size, 0.001F) / texturePixels;
    const float extent = size * 0.5F;
    // Skia's image local matrix maps source texels to dab-local coordinates.
    // The source origin must land at -extent, so the complete 256/1024px
    // resource occupies the dab instead of only its upper-left quadrant.
    return SkMatrix::MakeAll(
        pixelsToWorld, 0.0F, -extent + offsetX * pixelsToWorld,
        0.0F, pixelsToWorld, -extent + offsetY * pixelsToWorld,
        0.0F, 0.0F, 1.0F);
}

template <typename Dab>
void drawChalkDabs(SkCanvas& canvas, std::span<const Dab> dabs,
                   float red, float green, float blue, float alpha,
                   std::uint32_t materialRevision,
                   float opacityMultiplier = 1.0F) {
    for (const auto& dab : dabs) {
        const double x = [&] { if constexpr (std::is_same_v<Dab, semantic::DabInstance>) return dab.center.x; else return dab.x; }();
        const double y = [&] { if constexpr (std::is_same_v<Dab, semantic::DabInstance>) return dab.center.y; else return dab.y; }();
        const double size = dab.size;
        const float rotation = dab.rotation;
        const float opacity = dab.opacity;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || size <= 0.0 ||
            !std::isfinite(rotation) || !std::isfinite(opacity)) continue;
        canvas.save();
        canvas.translate(static_cast<float>(x), static_cast<float>(y));
        canvas.rotate(rotation * 57.2957795F);
        const float extent = static_cast<float>(size * 0.5);
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f({red, green, blue,
                          std::clamp(alpha * opacity * opacityMultiplier, 0.0F, 1.0F)}, nullptr);
        const auto shape = chalkShapeImage();
        const auto grain = chalkGrainImage();
        if (shape && grain) {
            // Image shader local matrices map source texels into dab-local
            // world units (not world units into source texels).
            const float texturePixels = 256.0F;
            const SkMatrix shapeLocal =
                dabTextureMatrix(static_cast<float>(size), texturePixels);
            const float grainOffset = static_cast<float>((materialRevision * 37U) % 256U);
            const SkMatrix grainLocal = dabTextureMatrix(
                static_cast<float>(size), texturePixels, grainOffset, grainOffset);
            auto shapeShader = shape->makeShader(
                SkTileMode::kClamp, SkTileMode::kClamp,
                SkSamplingOptions(SkFilterMode::kLinear), shapeLocal);
            auto grainShader = grain->makeShader(
                SkTileMode::kRepeat, SkTileMode::kRepeat,
                SkSamplingOptions(SkFilterMode::kLinear), grainLocal);
            // Modulate multiplies both shader color and alpha. Multiply would
            // preserve an opaque union alpha and expose the square texture
            // bounds around every dab.
            paint.setShader(SkShaders::Blend(
                SkBlendMode::kModulate, std::move(shapeShader), std::move(grainShader)));
            canvas.drawRect(SkRect::MakeLTRB(-extent, -extent, extent, extent), paint);
            paint.setShader(nullptr);
        }
        canvas.restore();
    }
}

template <typename Dab>
void drawContinuousChalkDabs(SkCanvas& canvas, std::span<const Dab> dabs,
                             float red, float green, float blue, float alpha,
                             std::uint32_t materialRevision) {
    if (dabs.empty()) return;
    const auto point = [](const auto& dab) {
        if constexpr (std::is_same_v<Dab, semantic::DabInstance>) {
            return SkPoint::Make(static_cast<float>(dab.center.x), static_cast<float>(dab.center.y));
        } else {
            return SkPoint::Make(static_cast<float>(dab.x), static_cast<float>(dab.y));
        }
    };
    float width = 0.0F;
    float opacity = 0.0F;
    for (const auto& dab : dabs) {
        width += static_cast<float>(dab.size);
        opacity += dab.opacity;
    }
    width = std::max(0.5F, width / static_cast<float>(dabs.size()));
    opacity = std::clamp(opacity / static_cast<float>(dabs.size()), 0.0F, 1.0F);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeWidth(width);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kRound_Join);
    paint.setColor4f({red, green, blue,
                      std::clamp(alpha * opacity * 0.72F, 0.0F, 1.0F)}, nullptr);
    const auto grain = chalkGrainImage();
    if (grain) {
        // The stroke body is one continuous coverage field.  Grain is sampled
        // in canvas space, so the interior is filled continuously rather than
        // being rebuilt from a row of visible stamps.
        const float texturePerWorld = 2.0F;
        SkMatrix grainLocal;
        grainLocal.setScale(texturePerWorld, texturePerWorld);
        auto grainShader = grain->makeShader(
            SkTileMode::kRepeat, SkTileMode::kRepeat,
            SkSamplingOptions(SkFilterMode::kLinear), grainLocal);
        paint.setShader(std::move(grainShader));
    }
    SkPathBuilder path;
    path.moveTo(point(dabs.front()));
    for (std::size_t i = 1; i < dabs.size(); ++i) path.lineTo(point(dabs[i]));
    if (dabs.size() == 1U) {
        const auto p = point(dabs.front());
        canvas.drawCircle(p.x(), p.y(), width * 0.5F, paint);
    } else {
        canvas.drawPath(path.detach(), paint);
    }
    paint.setShader(nullptr);

    // Add an intentionally irregular, low-opacity shape pass over the
    // continuous body.  The base stroke supplies uninterrupted coverage; the
    // screenshot-derived shape only perturbs the edge and density.  A stable
    // hash controls skips and normal offsets, so preview/canonical remain
    // identical without forming a periodic dab comb.
    if (chalkShapeImage() && chalkGrainImage() && dabs.size() > 1U) {
        const auto hash = [](std::size_t index, std::uint32_t revision) {
            std::uint32_t value = static_cast<std::uint32_t>(index) * 0x9e3779b9U;
            value ^= revision * 0x85ebca6bU;
            value ^= value >> 16U;
            value *= 0x7feb352dU;
            value ^= value >> 15U;
            return value;
        };
        for (std::size_t i = 0; i < dabs.size(); ++i) {
            const auto random = hash(i, materialRevision);
            if ((random % 2U) == 0U) continue;
            Dab overlay = dabs[i];
            const float jitter = (static_cast<float>(random & 0xffU) / 255.0F - 0.5F) * width * 0.48F;
            const float scale = 0.78F +
                static_cast<float>((random >> 8U) & 0xffU) / 255.0F * 0.72F;
            const float angleJitter =
                (static_cast<float>((random >> 16U) & 0xffU) / 255.0F - 0.5F) * 0.55F;
            if constexpr (std::is_same_v<Dab, semantic::DabInstance>) {
                const float tangent = overlay.rotation;
                overlay.center.x += -std::sin(tangent) * jitter;
                overlay.center.y += std::cos(tangent) * jitter;
                overlay.size *= scale * 1.18F;
                overlay.rotation += angleJitter;
            } else {
                const float tangent = overlay.rotation;
                overlay.x += -std::sin(tangent) * jitter;
                overlay.y += std::cos(tangent) * jitter;
                overlay.size *= scale * 1.18F;
                overlay.rotation += angleJitter;
            }
            drawChalkDabs(canvas, std::span<const Dab>(&overlay, 1U),
                          red, green, blue, alpha, materialRevision, 0.46F);
        }
    }
    (void)materialRevision;
}
} // namespace

void drawDabInstancesToSkCanvas(
    SkCanvas& canvas, std::span<const canvas::semantic::DabInstance> dabs,
    const canvas::semantic::ColorValue& color) {
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
    SkCanvas& canvas, std::span<const canvas::ink::BrushDab> dabs,
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

void drawChalkDabsToSkCanvas(
    SkCanvas& canvas, std::span<const canvas::semantic::DabInstance> dabs,
    const canvas::semantic::ColorValue& color, std::uint32_t materialRevision) {
    if (materialRevision >= 4U) {
        drawContinuousChalkDabs(canvas, dabs, color.r, color.g, color.b, color.a, materialRevision);
    } else {
        drawChalkDabs(canvas, dabs, color.r, color.g, color.b, color.a, materialRevision);
    }
}

void drawPreviewChalkDabsToSkCanvas(
    SkCanvas& canvas, std::span<const canvas::ink::BrushDab> dabs,
    float red, float green, float blue, float alpha,
    std::uint32_t materialRevision) {
    if (materialRevision >= 4U) {
        drawContinuousChalkDabs(canvas, dabs, red, green, blue, alpha, materialRevision);
    } else {
        drawChalkDabs(canvas, dabs, red, green, blue, alpha, materialRevision);
    }
}

template <typename Dab>
void drawMembraneDabs(SkCanvas& canvas, std::span<const Dab> dabs,
                      float red, float green, float blue, float alpha) {
    const auto shape = membraneShapeImage();
    const auto grain = membraneGrainImage();
    for (const auto& dab : dabs) {
        const double x = [&] { if constexpr (std::is_same_v<Dab, semantic::DabInstance>) return dab.center.x; else return dab.x; }();
        const double y = [&] { if constexpr (std::is_same_v<Dab, semantic::DabInstance>) return dab.center.y; else return dab.y; }();
        const double size = dab.size;
        if (!shape || !grain || !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(size) || size <= 0.0) continue;
        const float extent = static_cast<float>(size * 0.5);
        canvas.save();
        canvas.translate(static_cast<float>(x), static_cast<float>(y));
        canvas.rotate(dab.rotation * 57.2957795F);
        SkPaint paint;
        paint.setAntiAlias(true);
        paint.setColor4f({red, green, blue, std::clamp(alpha * dab.opacity, 0.0F, 1.0F)}, nullptr);
        const float texturePixels = 1024.0F;
        const SkMatrix shapeLocal = dabTextureMatrix(static_cast<float>(size), texturePixels);
        const SkMatrix grainLocal = dabTextureMatrix(static_cast<float>(size), texturePixels);
        auto shapeShader = shape->makeShader(SkTileMode::kClamp, SkTileMode::kClamp,
                                             SkSamplingOptions(SkFilterMode::kLinear), shapeLocal);
        auto grainShader = grain->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat,
                                             SkSamplingOptions(SkFilterMode::kLinear), grainLocal);
        paint.setShader(SkShaders::Blend(SkBlendMode::kModulate, std::move(shapeShader), std::move(grainShader)));
        canvas.drawRect(SkRect::MakeLTRB(-extent, -extent, extent, extent), paint);
        canvas.restore();
    }
}

void drawMembraneDabsToSkCanvas(
    SkCanvas& canvas, std::span<const semantic::DabInstance> dabs,
    const semantic::ColorValue& color) {
    drawMembraneDabs(canvas, dabs, color.r, color.g, color.b, color.a);
}

void drawPreviewMembraneDabsToSkCanvas(
    SkCanvas& canvas, std::span<const ink::BrushDab> dabs,
    float red, float green, float blue, float alpha) {
    drawMembraneDabs(canvas, dabs, red, green, blue, alpha);
}

void drawContinuousChalkDabsToSkCanvas(
    SkCanvas& canvas, std::span<const canvas::semantic::DabInstance> dabs,
    const canvas::semantic::ColorValue& color, std::uint32_t materialRevision) {
    drawContinuousChalkDabs(canvas, dabs, color.r, color.g, color.b, color.a, materialRevision);
}

} // namespace canvas::render::internal

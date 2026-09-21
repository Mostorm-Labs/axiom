#include "canvas/render/skia_ink_backend.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkPathBuilder.h"
#include "include/core/SkSurface.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::render {

struct SkiaInkBackend::Impl final {
    sk_sp<SkSurface> surface;
};

SkiaInkBackend::~SkiaInkBackend() { delete impl_; }

BackendSubmissionResult SkiaInkBackend::resize(std::uint32_t width,
                                               std::uint32_t height) {
    if (width == 0U || height == 0U || width > 16384U || height > 16384U) {
        return BackendSubmissionResult::rejected("invalid Skia ink surface dimensions");
    }
    if (static_cast<std::uint64_t>(width) * height >
        std::numeric_limits<std::size_t>::max() / 4U) {
        return BackendSubmissionResult::rejected("Skia ink surface size overflow");
    }
    if (impl_ == nullptr) impl_ = new Impl{};
    const auto info = SkImageInfo::MakeN32Premul(static_cast<int>(width),
                                                 static_cast<int>(height));
    impl_->surface = SkSurfaces::Raster(info);
    if (!impl_->surface) return BackendSubmissionResult::rejected("Skia raster surface creation failed");
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<std::size_t>(width) * height * 4U, 0U);
    return BackendSubmissionResult::accepted();
}

BackendSubmissionResult SkiaInkBackend::submit(
    std::span<const std::vector<CanonicalStrokePoint>> strokes) {
    return submit(strokes, CanonicalViewportTransform{});
}

BackendSubmissionResult SkiaInkBackend::submit(
    std::span<const std::vector<CanonicalStrokePoint>> strokes,
    CanonicalViewportTransform viewport) {
    if (impl_ == nullptr || !impl_->surface || width_ == 0U || height_ == 0U) {
        return BackendSubmissionResult::rejected("Skia ink surface is not initialized");
    }
    if (!std::isfinite(viewport.scale) || viewport.scale <= 0.0F ||
        !std::isfinite(viewport.translationX) || !std::isfinite(viewport.translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    SkCanvas* canvas = impl_->surface->getCanvas();
    canvas->clear(SK_ColorWHITE);
    canvas->save();
    canvas->translate(viewport.translationX, viewport.translationY);
    canvas->scale(viewport.scale, viewport.scale);
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setColor(SK_ColorBLUE);
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setStrokeCap(SkPaint::kRound_Cap);
    paint.setStrokeJoin(SkPaint::kRound_Join);
    paint.setStrokeWidth(3.0F);
    for (const auto& stroke : strokes) {
        if (stroke.empty()) continue;
        if (stroke.size() == 1U) {
            canvas->drawCircle(stroke.front().x, stroke.front().y, 1.5F, paint);
            continue;
        }
        SkPathBuilder path;
        path.moveTo(stroke.front().x, stroke.front().y);
        for (std::size_t i = 1; i < stroke.size(); ++i) path.lineTo(stroke[i].x, stroke[i].y);
        canvas->drawPath(path.detach(), paint);
    }
    canvas->restore();
    const SkImageInfo info = SkImageInfo::Make(static_cast<int>(width_),
                                               static_cast<int>(height_),
                                               kRGBA_8888_SkColorType,
                                               kPremul_SkAlphaType,
                                               SkColorSpace::MakeSRGB());
    if (!impl_->surface->readPixels(info, pixels_.data(), static_cast<size_t>(width_) * 4U, 0, 0)) {
        return BackendSubmissionResult::rejected("Skia canonical readback failed");
    }
    return BackendSubmissionResult::accepted();
}

}  // namespace canvas::render

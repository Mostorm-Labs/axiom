#include "canvas/render/skia_ink_backend.hpp"

#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace canvas::render {

struct SkiaInkBackend::Impl final {
    std::unique_ptr<SkiaSurfaceProvider> provider;
    SkiaRenderer renderer;
};

SkiaInkBackend::SkiaInkBackend()
    : SkiaInkBackend(std::make_unique<RasterSkiaSurfaceProvider>()) {}

SkiaInkBackend::SkiaInkBackend(std::unique_ptr<SkiaSurfaceProvider> provider)
    : impl_(new Impl{std::move(provider), {}}) {}

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
    if (impl_ != nullptr && width_ == width && height_ == height) {
        return BackendSubmissionResult::accepted();
    }
    if (impl_ == nullptr) impl_ = new Impl{};
    if (impl_->provider == nullptr) return BackendSubmissionResult::rejected("Skia surface provider is null");
    const auto resized = impl_->provider->resize(width, height);
    if (resized.code != BackendSubmissionCode::kAccepted) return resized;
    width_ = width;
    height_ = height;
    pixels_.assign(static_cast<std::size_t>(width) * height * 4U, 0U);
    submittedStrokes_.clear();
    submittedPrimitives_.clear();
    hasSubmission_ = false;
    return BackendSubmissionResult::accepted();
}

BackendSubmissionResult SkiaInkBackend::submitPrimitives(
    std::span<const canvas::ink::BrushPrimitive> primitives,
    CanonicalViewportTransform viewport) {
    if (impl_ == nullptr || width_ == 0U || height_ == 0U) {
        return BackendSubmissionResult::rejected("Skia ink surface is not initialized");
    }
    if (!std::isfinite(viewport.scale) || viewport.scale <= 0.0F ||
        !std::isfinite(viewport.translationX) || !std::isfinite(viewport.translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    if (hasSubmission_ && submittedPrimitives_.size() == primitives.size() &&
        std::equal(primitives.begin(), primitives.end(), submittedPrimitives_.begin()) &&
        viewport.scale == submittedViewport_.scale &&
        viewport.translationX == submittedViewport_.translationX &&
        viewport.translationY == submittedViewport_.translationY) {
        return BackendSubmissionResult::accepted();
    }
    const auto rendered = impl_->renderer.renderPrimitives(
        *impl_->provider, primitives, viewport.scale, viewport.translationX, viewport.translationY);
    if (rendered.code != BackendSubmissionCode::kAccepted) return rendered;
    const auto readback = impl_->provider->readbackRgba(pixels_);
    if (readback.code != BackendSubmissionCode::kAccepted) return readback;
    ++submissionCount_;
    ++rasterizationCount_;
    ++readbackCount_;
    ++cpuCopyCount_;
    submittedPrimitives_.assign(primitives.begin(), primitives.end());
    submittedStrokes_.clear();
    submittedViewport_ = viewport;
    hasSubmission_ = true;
    return BackendSubmissionResult::accepted();
}

BackendSubmissionResult SkiaInkBackend::submitBrushPoints(
    std::span<const BrushRenderPoint> points, CanonicalViewportTransform viewport) {
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
    return submitPrimitives(primitives, viewport);
}

BackendSubmissionResult SkiaInkBackend::submit(
    std::span<const std::vector<CanonicalStrokePoint>> strokes) {
    return submit(strokes, CanonicalViewportTransform{});
}

BackendSubmissionResult SkiaInkBackend::submit(
    std::span<const std::vector<CanonicalStrokePoint>> strokes,
    CanonicalViewportTransform viewport) {
    if (impl_ == nullptr || width_ == 0U || height_ == 0U) {
        return BackendSubmissionResult::rejected("Skia ink surface is not initialized");
    }
    if (!std::isfinite(viewport.scale) || viewport.scale <= 0.0F ||
        !std::isfinite(viewport.translationX) || !std::isfinite(viewport.translationY)) {
        return BackendSubmissionResult::rejected("invalid Skia viewport transform");
    }
    const auto samePoint = [](const CanonicalStrokePoint& a,
                              const CanonicalStrokePoint& b) {
        return a.x == b.x && a.y == b.y && a.pressure == b.pressure;
    };
    bool unchanged = hasSubmission_ && viewport.scale == submittedViewport_.scale &&
                     viewport.translationX == submittedViewport_.translationX &&
                     viewport.translationY == submittedViewport_.translationY &&
                     strokes.size() == submittedStrokes_.size();
    for (std::size_t i = 0; i < strokes.size() && unchanged; ++i) {
        if (strokes[i].size() != submittedStrokes_[i].size()) {
            unchanged = false;
            break;
        }
        for (std::size_t j = 0; j < strokes[i].size(); ++j) {
            if (!samePoint(strokes[i][j], submittedStrokes_[i][j])) {
                unchanged = false;
                break;
            }
        }
    }
    if (unchanged) return BackendSubmissionResult::accepted();
    const auto rendered = impl_->renderer.renderStrokes(
        *impl_->provider, strokes, viewport.scale, viewport.translationX, viewport.translationY);
    if (rendered.code != BackendSubmissionCode::kAccepted) return rendered;
    const auto readback = impl_->provider->readbackRgba(pixels_);
    if (readback.code != BackendSubmissionCode::kAccepted) return readback;
    ++submissionCount_;
    ++rasterizationCount_;
    ++readbackCount_;
    ++cpuCopyCount_;
    submittedStrokes_.assign(strokes.begin(), strokes.end());
    submittedViewport_ = viewport;
    hasSubmission_ = true;
    return BackendSubmissionResult::accepted();
}

}  // namespace canvas::render

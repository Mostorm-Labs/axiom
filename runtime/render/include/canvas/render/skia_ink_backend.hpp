#pragma once

#include "canvas/render/render_backend.hpp"
#include "canvas/render/brush_render_point.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace canvas::render {

class SkiaSurfaceProvider;

struct CanonicalStrokePoint final {
    float x = 0.0F;
    float y = 0.0F;
    float pressure = 0.0F;
};

struct CanonicalViewportTransform final {
    float scale = 1.0F;
    float translationX = 0.0F;
    float translationY = 0.0F;
};

// Render Core's small Windows ink surface consumer. The immutable canonical
// stroke list is rasterized by the locked CanvasSkia SDK; the platform host
// only presents the resulting pixels and never evaluates brush semantics.
class [[deprecated("use SkiaRenderer with a registered SkiaSurfaceProvider")]] SkiaInkBackend final {
  public:
    SkiaInkBackend();
    explicit SkiaInkBackend(std::unique_ptr<SkiaSurfaceProvider> provider);
    ~SkiaInkBackend();
    SkiaInkBackend(const SkiaInkBackend&) = delete;
    SkiaInkBackend& operator=(const SkiaInkBackend&) = delete;

    [[nodiscard]] BackendSubmissionResult resize(std::uint32_t width,
                                                 std::uint32_t height);
    [[nodiscard]] BackendSubmissionResult submit(
        std::span<const std::vector<CanonicalStrokePoint>> strokes);
    [[nodiscard]] BackendSubmissionResult submit(
        std::span<const std::vector<CanonicalStrokePoint>> strokes,
        CanonicalViewportTransform viewport);
    [[nodiscard]] BackendSubmissionResult submitPrimitives(
        std::span<const canvas::ink::BrushPrimitive> primitives,
        CanonicalViewportTransform viewport = {});
    [[nodiscard]] BackendSubmissionResult submitBrushPoints(
        std::span<const BrushRenderPoint> points,
        CanonicalViewportTransform viewport = {});
    [[nodiscard]] std::span<const std::uint8_t> rgba() const noexcept { return pixels_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::uint64_t rasterizationCount() const noexcept {
        return rasterizationCount_;
    }
    [[nodiscard]] std::uint64_t submissionCount() const noexcept { return submissionCount_; }
    [[nodiscard]] std::uint64_t readbackCount() const noexcept { return readbackCount_; }
    [[nodiscard]] std::uint64_t cpuCopyCount() const noexcept { return cpuCopyCount_; }

  private:
    struct Impl;
    Impl* impl_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::uint8_t> pixels_;
    std::vector<std::vector<CanonicalStrokePoint>> submittedStrokes_;
    std::vector<canvas::ink::BrushPrimitive> submittedPrimitives_;
    CanonicalViewportTransform submittedViewport_{};
    bool hasSubmission_ = false;
    std::uint64_t rasterizationCount_ = 0;
    std::uint64_t submissionCount_ = 0;
    std::uint64_t readbackCount_ = 0;
    std::uint64_t cpuCopyCount_ = 0;
};

}  // namespace canvas::render

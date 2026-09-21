#pragma once

#include "canvas/render/render_backend.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace canvas::render {

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
class SkiaInkBackend final {
  public:
    SkiaInkBackend() = default;
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
    [[nodiscard]] std::span<const std::uint8_t> rgba() const noexcept { return pixels_; }
    [[nodiscard]] std::uint32_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint32_t height() const noexcept { return height_; }
    [[nodiscard]] std::uint64_t rasterizationCount() const noexcept {
        return rasterizationCount_;
    }

  private:
    struct Impl;
    Impl* impl_ = nullptr;
    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::vector<std::uint8_t> pixels_;
    std::vector<std::vector<CanonicalStrokePoint>> submittedStrokes_;
    CanonicalViewportTransform submittedViewport_{};
    bool hasSubmission_ = false;
    std::uint64_t rasterizationCount_ = 0;
};

}  // namespace canvas::render

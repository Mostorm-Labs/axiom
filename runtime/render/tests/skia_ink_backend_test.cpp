#include "canvas/render/skia_ink_backend.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {
bool isBlue(std::span<const std::uint8_t> pixels, std::uint32_t width,
            std::uint32_t x, std::uint32_t y) {
  const std::size_t offset = (static_cast<std::size_t>(y) * width + x) * 4U;
  return pixels[offset] < 32U && pixels[offset + 1U] < 128U &&
         pixels[offset + 2U] > 192U && pixels[offset + 3U] > 192U;
}

bool hasPreviewPixels(std::span<const std::uint8_t> pixels, std::uint32_t width,
                      std::uint32_t height) {
  for (std::uint32_t y = 0; y < height; ++y)
    for (std::uint32_t x = 0; x < width; ++x)
      if (pixels[(static_cast<std::size_t>(y) * width + x) * 4U + 3U] != 0U) return true;
  return false;
}
}

int main() {
  canvas::render::SkiaInkBackend backend;
  assert(backend.resize(64U, 64U).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const std::vector<std::vector<canvas::render::CanonicalStrokePoint>> strokes{
      {{5.0F, 5.0F, 0.5F}}};
  const canvas::render::CanonicalViewportTransform viewport{
      .scale = 2.0F, .translationX = 3.0F, .translationY = 4.0F};
  (void)viewport;
  assert(backend.submit(strokes, viewport).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.rasterizationCount() == 1U);
  const auto pixels = backend.rgba();
  (void)pixels;
  assert(isBlue(pixels, backend.width(), 13U, 14U));
  assert(!isBlue(pixels, backend.width(), 5U, 5U));

  // Repeating the current size is a paint-time no-op. In particular it must
  // not replace/clear the already rendered surface, because the Windows host
  // may receive many WM_PAINT messages while input is moving.
  assert(backend.resize(64U, 64U).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(isBlue(backend.rgba(), backend.width(), 13U, 14U));

  assert(backend.submit(strokes, viewport).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.rasterizationCount() == 1U);

  canvas::render::SkiaInkBackend preview;
  assert(preview.resize(64U, 64U).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const std::vector<std::vector<canvas::render::CanonicalStrokePoint>> previewStroke{
      {{5.0F, 5.0F, 0.5F}, {20.0F, 5.0F, 0.5F}}};
  assert(preview.submitPreview(previewStroke).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(hasPreviewPixels(preview.rgba(), preview.width(), preview.height()));
  return 0;
}

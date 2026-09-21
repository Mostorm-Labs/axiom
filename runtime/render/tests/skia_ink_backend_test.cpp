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
}

int main() {
  canvas::render::SkiaInkBackend backend;
  assert(backend.resize(64U, 64U).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const std::vector<std::vector<canvas::render::CanonicalStrokePoint>> strokes{
      {{5.0F, 5.0F, 0.5F}}};
  const canvas::render::CanonicalViewportTransform viewport{
      .scale = 2.0F, .translationX = 3.0F, .translationY = 4.0F};
  assert(backend.submit(strokes, viewport).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  const auto pixels = backend.rgba();
  assert(isBlue(pixels, backend.width(), 13U, 14U));
  assert(!isBlue(pixels, backend.width(), 5U, 5U));
  return 0;
}

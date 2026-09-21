#include "canvas/render/skia_ink_backend.hpp"

#include <array>
#include <cassert>

int main() {
  canvas::render::SkiaInkBackend backend;
  assert(backend.resize(64, 64).code == canvas::render::BackendSubmissionCode::kAccepted);
  const std::array<std::uint8_t, 16> alpha{0, 0, 0, 0, 0, 255, 255, 0,
                                            0, 255, 255, 0, 0, 0, 0, 0};
  const canvas::render::ProgrammableDab dab{
      .x = 20.0F, .y = 20.0F, .size = 12.0F, .rotationDegrees = 15.0F,
      .opacity = 0.8F, .resourceWidth = 4, .resourceHeight = 4,
      .resourceAlpha = alpha};
  assert(backend.submitProgrammableDabs(std::span(&dab, 1)).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.rasterizationCount() == 1);
  const auto first = backend.rgba();
  const auto before = first.size();
  assert(before != 0);
  const canvas::render::ProgrammableDab rotated = {.x = 20.0F,
      .y = 20.0F, .size = 12.0F, .rotationDegrees = 90.0F, .opacity = 0.8F,
      .resourceWidth = 4, .resourceHeight = 4, .resourceAlpha = alpha};
  assert(backend.submitProgrammableDabs(std::span(&rotated, 1)).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.rasterizationCount() == 2);
  return 0;
}

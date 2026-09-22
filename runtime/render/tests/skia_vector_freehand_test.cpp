#include "canvas/render/skia_ink_backend.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <vector>

int main() {
  const std::array samples{
      canvas::ink::BrushInputSample{8, 32, 0.2F, 0, 0, 1},
      canvas::ink::BrushInputSample{32, 28, 0.7F, 0, 0, 2},
      canvas::ink::BrushInputSample{56, 32, 1.0F, 0, 0, 3}};
  const auto geometry = canvas::ink::generateVectorStroke(
      samples, {.size = 12.0F, .thinning = 0.65F, .smoothing = 0.35F});
  canvas::render::SkiaInkBackend backend;
  assert(backend.resize(64U, 64U).code == canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.submitVectorGeometry(std::span(&geometry, 1)).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  std::size_t colored = 0;
  for (std::size_t i = 0; i + 3U < backend.rgba().size(); i += 4U)
    if (backend.rgba()[i] != 255U || backend.rgba()[i + 1U] != 255U ||
        backend.rgba()[i + 2U] != 255U) ++colored;
  assert(colored > 20U);
  const auto count = backend.rasterizationCount();
  assert(backend.submitVectorGeometry(std::span(&geometry, 1)).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.rasterizationCount() == count);

  const auto normalPixels = std::vector<std::uint8_t>(backend.rgba().begin(),
                                                       backend.rgba().end());
  backend.setDiagnosticColor(true);
  assert(backend.submitVectorGeometry(std::span(&geometry, 1)).code ==
         canvas::render::BackendSubmissionCode::kAccepted);
  assert(backend.rasterizationCount() == count + 1U);
  assert(!std::equal(normalPixels.begin(), normalPixels.end(), backend.rgba().begin()));
}

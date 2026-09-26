#include "canvas/ink/brush_session.hpp"
#include <array>
#include <cassert>
#include <cstdint>

int main() {
  canvas::ink::BrushPackage package;
  package.packageId = "0123456789abcdef0123456789abcdef";
  auto state = canvas::ink::resolveBrushState(package, 7);
  canvas::ink::BrushSession session(9, state);
  assert(session.begin());
  canvas::ink::BrushPreviewDelta delta;
  for (std::uint64_t i = 1; i <= 2048; ++i) {
    const canvas::ink::BrushSample sample{8.0 * static_cast<double>(i), 0.0, 0.5, true, i};
    assert(session.append(std::span<const canvas::ink::BrushSample>(&sample, 1), {}, delta) ==
           canvas::ink::BrushSessionError::kNone);
  }
  canvas::ink::BrushCommitIntent intent;
  assert(session.seal(intent) == canvas::ink::BrushSessionError::kNone);
  const auto metrics = session.metrics();
  assert(metrics.appendEvaluations == 2048);
  assert(metrics.sealEvaluations == 1);
  assert(metrics.maxAppendInputSamples == 2048);
  assert(metrics.maxCopiedHistoricalSamples == 2048);
  return 0;
}

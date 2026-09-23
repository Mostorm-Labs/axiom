#include "../platform/apple/apple_input_adapter.hpp"

#include <cassert>

int main() {
  using namespace canvas::ink_playground::apple_input;
  AppleExtractedPointer samples[2]{};
  samples[0].sample.source = 7;
  samples[0].sample.pointer = 1;
  samples[0].sample.sequence = 1;
  samples[0].sample.timestampNs = 1;
  samples[0].sample.phase = canvas::input::PointerPhase::kDown;
  samples[1] = samples[0];
  samples[1].sample.pointer = 2;
  samples[1].sample.sequence = 2;
  samples[1].predicted = true;
  const auto batch = makeBatch(samples, 2);
  assert(batch.samples.size() == 2);
  assert(batch.samples[1].provenance == canvas::input::SampleProvenance::kPredicted);
  return 0;
}

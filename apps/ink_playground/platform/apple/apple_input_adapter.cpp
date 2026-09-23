#include "apple_input_adapter.hpp"

namespace canvas::ink_playground::apple_input {

canvas::input::PlatformPointerBatch makeBatch(
    const AppleExtractedPointer* samples, std::size_t count) noexcept {
  canvas::input::PlatformPointerBatch batch;
  if (samples == nullptr) return batch;
  batch.samples.reserve(count);
  for (std::size_t i = 0; i < count; ++i) {
    auto sample = samples[i].sample;
    if (samples[i].predicted) {
      sample.provenance = canvas::input::SampleProvenance::kPredicted;
    }
    batch.samples.push_back(sample);
  }
  return batch;
}

}  // namespace canvas::ink_playground::apple_input

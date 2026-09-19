#include "canvas/ink/pointer_trace.hpp"

#include <cstring>

namespace canvas::ink {
namespace {
constexpr std::uint64_t kOffset = 1469598103934665603ULL;
constexpr std::uint64_t kPrime = 1099511628211ULL;
void add(std::uint64_t& digest, std::uint64_t value) noexcept {
  for (unsigned i = 0; i < 8; ++i) {
    digest ^= (value >> (i * 8U)) & 0xffU;
    digest *= kPrime;
  }
}
std::uint32_t bits(float value) noexcept {
  std::uint32_t result = 0;
  std::memcpy(&result, &value, sizeof(result));
  return result;
}
}  // namespace

TraceValidation validate(const PointerTrace& trace) noexcept {
  if (trace.schemaVersion != 1) return TraceValidation::kUnsupportedSchema;
  if (trace.samples.empty()) return TraceValidation::kMissingSamples;
  std::uint64_t previous = 0;
  for (const auto& sample : trace.samples) {
    if (sample.sequence == 0 || sample.timestampNs == 0 || (!sample.predicted && sample.sequence <= previous)) {
      return sample.sequence == 0 || sample.timestampNs == 0 ? TraceValidation::kInvalidSample
                                                              : TraceValidation::kNonMonotonicConfirmed;
    }
    if (!sample.predicted) previous = sample.sequence;
  }
  return TraceValidation::kValid;
}

std::uint64_t traceDigest(const PointerTrace& trace) noexcept {
  std::uint64_t digest = kOffset;
  add(digest, trace.schemaVersion);
  add(digest, trace.samples.size());
  for (const auto& sample : trace.samples) {
    add(digest, sample.sequence);
    add(digest, sample.timestampNs);
    add(digest, bits(sample.x));
    add(digest, bits(sample.y));
    add(digest, bits(sample.pressure));
    add(digest, sample.predicted ? 1 : 0);
  }
  return digest;
}

}  // namespace canvas::ink

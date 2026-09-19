#pragma once

#include "canvas/input/pointer_sample.hpp"

#include <cstdint>
#include <vector>

namespace canvas::ink {

struct PointerTrace final {
  std::uint32_t schemaVersion = 1;
  std::vector<input::PointerSample> samples;
  bool cancelled = false;
  bool confirmedOverrun = false;
  std::uint64_t surfaceMetricsGeneration = 0;
  std::uint64_t expectedSurfaceMetricsGeneration = 0;
};

enum class TraceValidation : std::uint8_t {
  kValid,
  kUnsupportedSchema,
  kMissingSamples,
  kNonMonotonicConfirmed,
  kInvalidSample,
};

TraceValidation validate(const PointerTrace& trace) noexcept;
std::uint64_t traceDigest(const PointerTrace& trace) noexcept;

}  // namespace canvas::ink

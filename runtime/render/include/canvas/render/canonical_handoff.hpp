#pragma once

#include <cstdint>

namespace canvas::render {

struct HandoffEligibility final {
  std::uint64_t tokenHigh = 0;
  std::uint64_t tokenLow = 0;
  bool pending = false;
  std::uint64_t requiredDocumentRevision = 0;
  std::uint64_t presentedDocumentRevision = 0;
  std::uint64_t requiredSurfaceGeneration = 0;
  std::uint64_t presentedSurfaceGeneration = 0;
  std::uint64_t requiredMetricsGeneration = 0;
  std::uint64_t presentedMetricsGeneration = 0;
  bool presented = false;
  bool platformQualified = false;
  bool coverageEligible = false;
};

enum class HandoffDisposition : std::uint8_t {
  kEligible,
  kNoPendingToken,
  kInvalidToken,
  kRevisionMismatch,
  kSurfaceGenerationMismatch,
  kMetricsGenerationMismatch,
  kNotPresented,
  kUnqualifiedEvidence,
  kIneligibleCoverage,
};

class CanonicalHandoffEvaluator final {
 public:
  [[nodiscard]] static HandoffDisposition evaluate(const HandoffEligibility& input) noexcept;
};

}  // namespace canvas::render

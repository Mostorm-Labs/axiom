#include "canvas/render/canonical_handoff.hpp"

namespace canvas::render {

HandoffDisposition CanonicalHandoffEvaluator::evaluate(const HandoffEligibility& input) noexcept {
  if (!input.pending) return HandoffDisposition::kNoPendingToken;
  if (input.tokenHigh == 0 && input.tokenLow == 0) return HandoffDisposition::kInvalidToken;
  if (input.requiredDocumentRevision == 0 ||
      input.requiredDocumentRevision != input.presentedDocumentRevision) {
    return HandoffDisposition::kRevisionMismatch;
  }
  if (input.requiredSurfaceGeneration == 0 ||
      input.requiredSurfaceGeneration != input.presentedSurfaceGeneration) {
    return HandoffDisposition::kSurfaceGenerationMismatch;
  }
  if (input.requiredMetricsGeneration == 0 ||
      input.requiredMetricsGeneration != input.presentedMetricsGeneration) {
    return HandoffDisposition::kMetricsGenerationMismatch;
  }
  if (!input.presented) return HandoffDisposition::kNotPresented;
  if (!input.platformQualified) return HandoffDisposition::kUnqualifiedEvidence;
  if (!input.coverageEligible) return HandoffDisposition::kIneligibleCoverage;
  return HandoffDisposition::kEligible;
}

}  // namespace canvas::render

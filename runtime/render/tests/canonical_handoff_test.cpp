#include "canvas/render/canonical_handoff.hpp"

#include <cassert>

int main() {
  canvas::render::HandoffEligibility input;
  input.tokenHigh = 1;
  input.tokenLow = 2;
  input.pending = true;
  input.requiredDocumentRevision = 8;
  input.presentedDocumentRevision = 8;
  input.requiredSurfaceGeneration = 3;
  input.presentedSurfaceGeneration = 3;
  input.requiredMetricsGeneration = 4;
  input.presentedMetricsGeneration = 4;
  input.presented = true;
  input.platformQualified = true;
  input.coverageEligible = true;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(input) ==
         canvas::render::HandoffDisposition::kEligible);
  auto negative = input;
  negative.platformQualified = false;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) !=
         canvas::render::HandoffDisposition::kEligible);
  negative = input;
  negative.coverageEligible = false;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) !=
         canvas::render::HandoffDisposition::kEligible);
  negative = input;
  negative.presentedSurfaceGeneration = 2;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) !=
         canvas::render::HandoffDisposition::kEligible);
  negative = input;
  negative.pending = false;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kNoPendingToken);
  negative = input;
  negative.tokenHigh = 0;
  negative.tokenLow = 0;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kInvalidToken);
  negative = input;
  negative.presentedDocumentRevision = 7;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kRevisionMismatch);
  negative = input;
  negative.presentedMetricsGeneration = 3;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kMetricsGenerationMismatch);
  negative = input;
  negative.presented = false;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kNotPresented);
  negative = input;
  negative.coverageEligible = false;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kIneligibleCoverage);
  negative = input;
  negative.platformQualified = false;
  assert(canvas::render::CanonicalHandoffEvaluator::evaluate(negative) ==
         canvas::render::HandoffDisposition::kUnqualifiedEvidence);
  return 0;
}

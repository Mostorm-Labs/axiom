import { isDeepStrictEqual } from "node:util";
import type { CExpectedOutcome, ImplementationObservation } from "./types.js";

export function deepEqualJson(left: unknown, right: unknown): boolean {
  return isDeepStrictEqual(left, right);
}

export function compareObservationToExpected(
  expected: CExpectedOutcome,
  observation: ImplementationObservation,
): string[] {
  const diagnostics: string[] = [];
  if (expected.disposition !== observation.observedDisposition) {
    diagnostics.push("DISPOSITION_MISMATCH");
  }
  if (expected.terminalPhase !== observation.observedTerminalPhase) {
    diagnostics.push("TERMINAL_PHASE_MISMATCH");
  }
  if (
    expected.semanticErrorCategory !== undefined &&
    expected.semanticErrorCategory !== observation.observedErrorCategory
  ) {
    diagnostics.push("SEMANTIC_ERROR_CATEGORY_MISMATCH");
  }
  if (
    expected.logicalPlanProjection !== undefined &&
    !deepEqualJson(expected.logicalPlanProjection, observation.observedPlanProjection)
  ) {
    diagnostics.push("LOGICAL_PLAN_PROJECTION_MISMATCH");
  }
  return diagnostics;
}

export function compareProviders(
  expected: CExpectedOutcome,
  reference: ImplementationObservation,
  indexed: ImplementationObservation,
): string[] {
  const differs =
    reference.observedDisposition !== indexed.observedDisposition ||
    reference.observedTerminalPhase !== indexed.observedTerminalPhase ||
    (expected.semanticErrorCategory !== undefined &&
      reference.observedErrorCategory !== indexed.observedErrorCategory) ||
    (expected.logicalPlanProjection !== undefined &&
      !deepEqualJson(reference.observedPlanProjection, indexed.observedPlanProjection));
  return differs ? ["PROVIDER_DIVERGENCE"] : [];
}

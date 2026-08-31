import { compareObservationToExpected, compareProviders, deepEqualJson } from "./compare.js";
import { validateCoordinatorInputs } from "./provenance.js";
import type {
  CaseIntent,
  CExpectedOutcome,
  ConformanceResult,
  ImplementationObservation,
  OpenAuthorityDecision,
} from "./types.js";

export interface CoordinateCaseInput {
  caseIntent: CaseIntent;
  expected: CExpectedOutcome;
  reference: ImplementationObservation;
  indexed: ImplementationObservation;
  referenceRef: string;
  indexedRef: string;
  openAuthorityDecision: OpenAuthorityDecision;
}

function result(input: CoordinateCaseInput, status: ConformanceResult["status"], diagnostics: string[]): ConformanceResult {
  const value: ConformanceResult = {
    format: "axiom-g1-04-c-result-v1",
    formatVersion: 1,
    provenance: "CONFORMANCE_RESULT",
    caseId: input.caseIntent?.id ?? input.expected?.caseId ?? "INVALID_CASE",
    status,
    expectedRef: input.caseIntent?.expectedRef ?? "INVALID_EXPECTED_REF",
    observationRefs: [input.referenceRef, input.indexedRef].filter((ref): ref is string => typeof ref === "string" && ref.length > 0),
  };
  if (diagnostics.length > 0) value.diagnostics = diagnostics;
  return value;
}

export function coordinateCase(input: CoordinateCaseInput): ConformanceResult {
  const inputDiagnostics = validateCoordinatorInputs(input);
  if (inputDiagnostics.length > 0) {
    return result(input, "FAIL", inputDiagnostics);
  }

  if (input.expected.openPolicy === true) {
    if (input.openAuthorityDecision === "CURRENT_CLOSED") {
      return result(input, "FAIL", ["OPEN_POLICY_STALE_CLOSED"]);
    }
    if (input.openAuthorityDecision !== "CURRENT_OPEN") {
      return result(input, "FAIL", ["OPEN_AUTHORITY_UNRESOLVED"]);
    }
  }

  const mutationDiagnostics: string[] = [];
  if (!deepEqualJson(input.reference.beforeProjection, input.reference.afterProjection)) {
    mutationDiagnostics.push("REFERENCE_MUTATION");
  }
  if (!deepEqualJson(input.indexed.beforeProjection, input.indexed.afterProjection)) {
    mutationDiagnostics.push("INDEXED_MUTATION");
  }
  if (mutationDiagnostics.length > 0) {
    return result(input, "FAIL", mutationDiagnostics);
  }

  if (input.expected.openPolicy === true) {
    const diagnostics = ["OPEN_POLICY_OBSERVATION_ONLY", ...compareProviders(input.expected, input.reference, input.indexed)];
    return result(input, "OBSERVATION_ONLY", diagnostics);
  }

  const diagnostics: string[] = [];
  if (compareObservationToExpected(input.expected, input.reference).length > 0) {
    diagnostics.push("REFERENCE_GOLDEN_MISMATCH");
  }
  if (compareObservationToExpected(input.expected, input.indexed).length > 0) {
    diagnostics.push("INDEXED_GOLDEN_MISMATCH");
  }
  diagnostics.push(...compareProviders(input.expected, input.reference, input.indexed));
  return result(input, diagnostics.length === 0 ? "PASS" : "FAIL", diagnostics);
}

import type { CoordinateCaseInput } from "./coordinator.js";

export function validateCoordinatorInputs(input: CoordinateCaseInput): string[] {
  const diagnostics: string[] = [];
  const raw = input as unknown as Record<string, any>;
  const caseIntent = raw.caseIntent;
  const expected = raw.expected;
  const reference = raw.reference;
  const indexed = raw.indexed;

  if (!caseIntent || caseIntent.provenance !== "AUTHORITY_MANUAL") {
    diagnostics.push("CASE_PROVENANCE_INVALID");
  }
  if (!expected || expected.provenance !== "AUTHORITY_MANUAL") {
    diagnostics.push("EXPECTED_PROVENANCE_INVALID");
  }
  if (!reference || reference.provenance !== "IMPLEMENTATION_OBSERVATION") {
    diagnostics.push("REFERENCE_PROVENANCE_INVALID");
  }
  if (!indexed || indexed.provenance !== "IMPLEMENTATION_OBSERVATION") {
    diagnostics.push("INDEXED_PROVENANCE_INVALID");
  }

  const caseId = caseIntent?.id;
  if (
    typeof caseId !== "string" ||
    expected?.caseId !== caseId ||
    reference?.caseId !== caseId ||
    indexed?.caseId !== caseId
  ) {
    diagnostics.push("CASE_ID_MISMATCH");
  }

  if (reference?.provider !== "reference" || indexed?.provider !== "indexed") {
    diagnostics.push("PROVIDER_SET_INVALID");
  }

  const observationRefs = [raw.referenceRef, raw.indexedRef].filter(
    (ref): ref is string => typeof ref === "string" && ref.length > 0,
  );
  if (observationRefs.length !== 2 || new Set(observationRefs).size !== observationRefs.length) {
    diagnostics.push("PROVIDER_SET_INVALID");
  }

  if (
    expected &&
    expected.openPolicy !== true &&
    (expected.disposition === undefined || expected.terminalPhase === undefined)
  ) {
    diagnostics.push("EXPECTED_CONTRACT_INVALID");
  }

  return [...new Set(diagnostics)];
}

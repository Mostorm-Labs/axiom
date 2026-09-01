export interface ProviderDiffInput {
  coreCorpusEvidence: unknown;
  c6NoMutationEvidence: unknown;
}

export interface ProviderDiffSummary {
  status: "PASS" | "FAIL";
  caseCount: number;
  providerPairCount: number;
  providerAgreement: "90/90";
  divergenceCount: number;
  divergenceCaseIds: string[];
  goldenPassCount: number;
  goldenFailCount: number;
  observationOnlyCount: number;
  goldenMismatchCaseIds: string[];
  manualGoldenCorrectness: "PASS" | "FAIL";
}

type JsonRecord = Record<string, unknown>;

function record(value: unknown, label: string): JsonRecord {
  if (typeof value !== "object" || value === null || Array.isArray(value)) throw new Error(`${label} must be an object`);
  return value as JsonRecord;
}

function array(value: unknown, label: string): unknown[] {
  if (!Array.isArray(value)) throw new Error(`${label} must be an array`);
  return value;
}

function string(value: unknown, label: string): string {
  if (typeof value !== "string" || value.length === 0) throw new Error(`${label} must be a non-empty string`);
  return value;
}

function number(value: unknown, label: string): number {
  if (typeof value !== "number" || !Number.isSafeInteger(value)) throw new Error(`${label} must be a safe integer`);
  return value;
}

function expectedC6Value(evidence: JsonRecord, field: string, expected: unknown): void {
  if (evidence[field] !== expected) throw new Error(`C6 ${field} is invalid`);
}

function assertC6NoMutation(evidence: unknown): void {
  const value = record(evidence, "C6 no-mutation evidence");
  expectedC6Value(value, "format", "axiom-gt-g1-04-c6-no-mutation-v1");
  expectedC6Value(value, "formatVersion", 1);
  expectedC6Value(value, "status", "PASS");
  expectedC6Value(value, "acceptedCases", 90);
  expectedC6Value(value, "observationCount", 180);
  expectedC6Value(value, "referenceObservations", 90);
  expectedC6Value(value, "indexedObservations", 90);
  expectedC6Value(value, "providerAgreement", "90/90");
  expectedC6Value(value, "beforeAfterEqual", "180/180");
  expectedC6Value(value, "observerMutationCalls", 0);
  expectedC6Value(value, "authorityManualExpected", true);
  expectedC6Value(value, "expectedTruthWrites", 0);
  expectedC6Value(value, "providerOutputUsedAsExpected", false);
  expectedC6Value(value, "productionSemanticDelta", 0);
}

function isKnownDiagnostic(value: string): boolean {
  return value === "REFERENCE_GOLDEN_MISMATCH" || value === "INDEXED_GOLDEN_MISMATCH" || value === "PROVIDER_DIVERGENCE";
}

export function summarizeProviderDiff(input: ProviderDiffInput): ProviderDiffSummary {
  const core = record(input.coreCorpusEvidence, "C5 core-corpus evidence");
  if (core.format !== "axiom-gt-g1-04-c-core-corpus-v1" || core.formatVersion !== 1) throw new Error("invalid C5 core-corpus evidence");
  if (
    number(core.selectedCaseCount, "C5 selectedCaseCount") !== 90 ||
    number(core.selectedExpectedCount, "C5 selectedExpectedCount") !== 90 ||
    number(core.selectedObservationCount, "C5 selectedObservationCount") !== 180 ||
    number(core.operationFamilyCount, "C5 operationFamilyCount") !== 15
  ) throw new Error("C5 inventory is not 90 cases / 90 expected / 180 observations / 15 families");
  if (array(core.missingMandatoryFamilies, "C5 missingMandatoryFamilies").length !== 0) throw new Error("C5 mandatory families are missing");
  if (number(core.acceptedOpenPolicyCount, "C5 acceptedOpenPolicyCount") !== 0) throw new Error("C5 contains OPEN expected records");
  if (core.acceptedExpectedAllAuthorityManual !== true) throw new Error("C5 expected provenance is not AUTHORITY_MANUAL");
  if (number(core.expectedTruthWrites, "C5 expectedTruthWrites") !== 0) throw new Error("C5 evidence reports expected truth writes");
  if (core.providerOutputUsedAsExpected !== false) throw new Error("C5 provider output was used as expected truth");
  if (number(core.productionSemanticDeltaFromC3, "C5 productionSemanticDeltaFromC3") !== 0) throw new Error("C5 production semantic delta is non-zero");
  const statusCounts = record(core.resultStatusCounts, "C5 resultStatusCounts");
  if (number(statusCounts.PASS, "C5 PASS count") !== 61 || number(statusCounts.FAIL, "C5 FAIL count") !== 29 || number(statusCounts.OBSERVATION_ONLY, "C5 OBSERVATION_ONLY count") !== 0) {
    throw new Error("C5 result inventory is not 61 PASS / 29 FAIL / 0 OBSERVATION_ONLY");
  }

  assertC6NoMutation(input.c6NoMutationEvidence);

  const results = array(core.results, "C5 results");
  if (results.length !== 90) throw new Error("C5 result count is not 90");
  const seen = new Set<string>();
  const mismatches: string[] = [];
  const divergences: string[] = [];
  let pass = 0;
  let fail = 0;
  let observationOnly = 0;
  for (const [index, raw] of results.entries()) {
    const result = record(raw, `C5 result ${index}`);
    if (result.format !== "axiom-g1-04-c-result-v1" || result.formatVersion !== 1 || result.provenance !== "CONFORMANCE_RESULT") throw new Error(`C5 result ${index} has an invalid trust envelope`);
    const caseId = string(result.caseId, `C5 result ${index}.caseId`);
    if (seen.has(caseId)) throw new Error(`duplicate C5 result case id: ${caseId}`);
    seen.add(caseId);
    const refs = array(result.observationRefs, `C5 result ${caseId}.observationRefs`).map((ref) => string(ref, `C5 result ${caseId} observation ref`));
    if (
      refs.length !== 2 ||
      new Set(refs).size !== 2 ||
      refs.filter((ref) => ref.endsWith(`/${caseId}/reference`)).length !== 1 ||
      refs.filter((ref) => ref.endsWith(`/${caseId}/indexed`)).length !== 1
    ) throw new Error(`missing provider pair: ${caseId}`);
    const status = string(result.status, `C5 result ${caseId}.status`);
    const diagnostics = result.diagnostics === undefined ? [] : array(result.diagnostics, `C5 result ${caseId}.diagnostics`).map((diagnostic) => string(diagnostic, `C5 result ${caseId} diagnostic`));
    if (diagnostics.some((diagnostic) => !isKnownDiagnostic(diagnostic))) throw new Error(`unexpected C5 diagnostic: ${caseId}`);
    const hasGoldenMismatch = diagnostics.includes("REFERENCE_GOLDEN_MISMATCH") || diagnostics.includes("INDEXED_GOLDEN_MISMATCH");
    const hasDivergence = diagnostics.includes("PROVIDER_DIVERGENCE");
    if (hasDivergence) divergences.push(caseId);
    if (status === "PASS") {
      if (diagnostics.length !== 0) throw new Error(`passing C5 result has diagnostics: ${caseId}`);
      pass += 1;
    } else if (status === "FAIL") {
      if (!hasGoldenMismatch && !hasDivergence) throw new Error(`failing C5 result has no recognized diagnostic: ${caseId}`);
      if (hasGoldenMismatch) mismatches.push(caseId);
      fail += 1;
    } else if (status === "OBSERVATION_ONLY") {
      observationOnly += 1;
    } else {
      throw new Error(`unexpected C5 result status: ${caseId}`);
    }
  }
  if (pass !== 61 || fail !== 29 || observationOnly !== 0 || mismatches.length !== 29) throw new Error("C5 results do not preserve the required manual-golden inventory");

  mismatches.sort();
  divergences.sort();
  return {
    status: divergences.length === 0 ? "PASS" : "FAIL",
    caseCount: 90,
    providerPairCount: 90,
    providerAgreement: "90/90",
    divergenceCount: divergences.length,
    divergenceCaseIds: divergences,
    goldenPassCount: 61,
    goldenFailCount: 29,
    observationOnlyCount: 0,
    goldenMismatchCaseIds: mismatches,
    manualGoldenCorrectness: "FAIL",
  };
}

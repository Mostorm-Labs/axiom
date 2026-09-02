export interface ProviderDiffInput {
  coreCorpusEvidence: unknown;
  noMutationEvidence: unknown;
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

function expectedNoMutationValue(evidence: JsonRecord, field: string, expected: unknown): void {
  if (evidence[field] !== expected) throw new Error(`P36 no-mutation ${field} is invalid`);
}

function assertP36NoMutation(evidence: unknown): void {
  const value = record(evidence, "P36 no-mutation evidence");
  expectedNoMutationValue(value, "format", "axiom-gt-g1-04-c-p36-no-mutation-v1");
  expectedNoMutationValue(value, "formatVersion", 1);
  expectedNoMutationValue(value, "stage", "P36");
  expectedNoMutationValue(value, "sourceRef", "492d2f914f078a6e4ac8b567e07f7ec813c10107");
  expectedNoMutationValue(value, "acceptedCases", 90);
  expectedNoMutationValue(value, "observationCount", 180);
  expectedNoMutationValue(value, "referenceObservations", 90);
  expectedNoMutationValue(value, "indexedObservations", 90);
  expectedNoMutationValue(value, "beforeAfterEqual", "180/180");
  expectedNoMutationValue(value, "unexpectedHarnessErrors", 0);
  expectedNoMutationValue(value, "observerMutationCalls", 0);
  expectedNoMutationValue(value, "expectedTruthReads", 0);
  expectedNoMutationValue(value, "semanticCodecCalls", 0);
}

function isKnownDiagnostic(value: string): boolean {
  return value === "REFERENCE_GOLDEN_MISMATCH" || value === "INDEXED_GOLDEN_MISMATCH" || value === "PROVIDER_DIVERGENCE";
}

export function summarizeProviderDiff(input: ProviderDiffInput): ProviderDiffSummary {
  const core = record(input.coreCorpusEvidence, "P36 core-corpus evidence");
  if (core.format !== "axiom-gt-g1-04-c-core-corpus-v1" || core.formatVersion !== 1) throw new Error("invalid P36 core-corpus evidence");
  if (
    number(core.selectedCaseCount, "P36 selectedCaseCount") !== 90 ||
    number(core.selectedExpectedCount, "P36 selectedExpectedCount") !== 90 ||
    number(core.selectedObservationCount, "P36 selectedObservationCount") !== 180 ||
    number(core.operationFamilyCount, "P36 operationFamilyCount") !== 15
  ) throw new Error("P36 inventory is not 90 cases / 90 expected / 180 observations / 15 families");
  for (const field of ["missingMandatoryFamilies", "wrongFamilyCaseIds", "unselectedCaseIds", "duplicateSuiteCaseIds"]) {
    if (array(core[field], `P36 ${field}`).length !== 0) throw new Error(`P36 ${field} is not empty`);
  }
  if (number(core.acceptedOpenPolicyCount, "P36 acceptedOpenPolicyCount") !== 0) throw new Error("P36 contains OPEN expected records");
  if (core.acceptedExpectedAllAuthorityManual !== true) throw new Error("P36 expected provenance is not AUTHORITY_MANUAL");
  if (number(core.expectedTruthWrites, "P36 expectedTruthWrites") !== 0) throw new Error("P36 evidence reports expected truth writes");
  if (core.providerOutputUsedAsExpected !== false) throw new Error("P36 provider output was used as expected truth");
  if (number(core.productionSemanticDelta, "P36 productionSemanticDelta") !== 0) throw new Error("P36 production semantic delta is non-zero");
  const statusCounts = record(core.resultStatusCounts, "P36 resultStatusCounts");

  assertP36NoMutation(input.noMutationEvidence);

  const results = array(core.results, "P36 results");
  if (results.length !== 90) throw new Error("P36 result count is not 90");
  const seen = new Set<string>();
  const mismatches: string[] = [];
  const divergences: string[] = [];
  let pass = 0;
  let fail = 0;
  let observationOnly = 0;
  for (const [index, raw] of results.entries()) {
    const result = record(raw, `P36 result ${index}`);
    if (result.format !== "axiom-g1-04-c-result-v1" || result.formatVersion !== 1 || result.provenance !== "CONFORMANCE_RESULT") throw new Error(`P36 result ${index} has an invalid trust envelope`);
    const caseId = string(result.caseId, `P36 result ${index}.caseId`);
    if (seen.has(caseId)) throw new Error(`duplicate P36 result case id: ${caseId}`);
    seen.add(caseId);
    const refs = array(result.observationRefs, `P36 result ${caseId}.observationRefs`).map((ref) => string(ref, `P36 result ${caseId} observation ref`));
    if (
      refs.length !== 2 ||
      new Set(refs).size !== 2 ||
      refs.filter((ref) => ref.endsWith(`/${caseId}/reference`)).length !== 1 ||
      refs.filter((ref) => ref.endsWith(`/${caseId}/indexed`)).length !== 1
    ) throw new Error(`missing provider pair: ${caseId}`);
    const status = string(result.status, `P36 result ${caseId}.status`);
    const diagnostics = result.diagnostics === undefined ? [] : array(result.diagnostics, `P36 result ${caseId}.diagnostics`).map((diagnostic) => string(diagnostic, `P36 result ${caseId} diagnostic`));
    if (diagnostics.some((diagnostic) => !isKnownDiagnostic(diagnostic))) throw new Error(`unexpected P36 diagnostic: ${caseId}`);
    const hasGoldenMismatch = diagnostics.includes("REFERENCE_GOLDEN_MISMATCH") || diagnostics.includes("INDEXED_GOLDEN_MISMATCH");
    const hasDivergence = diagnostics.includes("PROVIDER_DIVERGENCE");
    if (hasDivergence) divergences.push(caseId);
    if (status === "PASS") {
      if (diagnostics.length !== 0) throw new Error(`passing P36 result has diagnostics: ${caseId}`);
      pass += 1;
    } else if (status === "FAIL") {
      if (!hasGoldenMismatch && !hasDivergence) throw new Error(`failing P36 result has no recognized diagnostic: ${caseId}`);
      if (hasGoldenMismatch) mismatches.push(caseId);
      fail += 1;
    } else if (status === "OBSERVATION_ONLY") {
      observationOnly += 1;
    } else {
      throw new Error(`unexpected P36 result status: ${caseId}`);
    }
  }
  if (
    number(statusCounts.PASS, "P36 PASS count") !== pass ||
    number(statusCounts.FAIL, "P36 FAIL count") !== fail ||
    number(statusCounts.OBSERVATION_ONLY, "P36 OBSERVATION_ONLY count") !== observationOnly ||
    pass + fail + observationOnly !== 90
  ) throw new Error("P36 results do not match resultStatusCounts");

  mismatches.sort();
  divergences.sort();
  return {
    status: divergences.length === 0 ? "PASS" : "FAIL",
    caseCount: 90,
    providerPairCount: 90,
    providerAgreement: "90/90",
    divergenceCount: divergences.length,
    divergenceCaseIds: divergences,
    goldenPassCount: pass,
    goldenFailCount: fail,
    observationOnlyCount: observationOnly,
    goldenMismatchCaseIds: mismatches,
    manualGoldenCorrectness: fail === 0 && observationOnly === 0 ? "PASS" : "FAIL",
  };
}

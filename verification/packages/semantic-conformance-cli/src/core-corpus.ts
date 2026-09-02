import { coordinateCase } from "./coordinator.js";
import type {
  CaseIntent,
  CExpectedOutcome,
  ConformanceResult,
  ImplementationObservation,
} from "./types.js";

export interface CoreSuite {
  format: "axiom-g1-04-c-core-suite-v1";
  formatVersion: 1;
  suiteId: "GT-G1-04-C-CORE";
  caseIds: string[];
}

export interface C3ProjectionEvidence {
  format: "axiom-gt-g1-04-c-plan-projection-v2";
  formatVersion: 2;
  factsOnly: true;
  acceptedCases: number;
  observationCount: number;
  noMutationObservations: number;
  observationRecords: ImplementationObservation[];
}

export interface MandatoryFamilySpec {
  id: string;
  operationFamily: string;
  authorityRequirement: string;
  caseIds: readonly string[];
}

export interface CoreCoverageReport {
  operationFamilies: readonly string[];
  mandatoryFamilies: readonly MandatoryFamilySpec[];
  missingMandatoryFamilies: string[];
  wrongFamilyCaseIds: string[];
  unselectedCaseIds: string[];
  duplicateSuiteCaseIds: string[];
}

export interface CoreCorpusRun {
  suiteId: "GT-G1-04-C-CORE";
  selectedCaseCount: number;
  selectedExpectedCount: number;
  selectedObservationCount: number;
  operationFamilyCount: number;
  coverage: CoreCoverageReport;
  results: ConformanceResult[];
}

const OPERATION_FAMILIES = [
  "InsertObjects",
  "DeleteObjects",
  "RestoreObjects",
  "SetPlacements",
  "SetTransforms",
  "PatchProperties",
  "SetObjectSize",
  "SetVectorPathGeometry",
  "SetImageContent",
  "AddStroke",
  "SplitStrokes",
  "AddEraseMasks",
  "RemoveEraseMasks",
  "EditRichText",
  "SetConnectorContent",
] as const;

const FAMILY_ANCHORS: readonly [string, string, readonly string[]][] = [
  ["InsertObjects", "C-V02/C-V03/C-V05/C-V06", [
    "C1-INSERT-VALID", "C1-INSERT-STAGED-PARENT", "C1-INSERT-STAGED-CONNECTOR",
    "C1-INSERT-EXISTING-ID", "C1-INSERT-HIERARCHY-CYCLE", "C1-INSERT-STICKY-CARDINALITY",
  ]],
  ["DeleteObjects", "C-V02/C-V03/C-V06", [
    "C1-DELETE-VALID", "C1-DELETE-SUBTREE", "C1-DELETE-CASCADE",
    "C1-DELETE-MISSING-TARGET", "C1-DELETE-DUPLICATE-TARGET",
  ]],
  ["RestoreObjects", "C-V02/C-V03/C-V04/C-V05/C-V06", [
    "C1-RESTORE-ELIGIBLE", "C1-RESTORE-EXISTING-ID", "C1-RESTORE-EXISTING-ID-DIFFERENT",
    "C1-RESTORE-BATCH-EXISTING-ID", "C1-RESTORE-STAGED-PARENT-CHILD",
    "C1-RESTORE-STAGED-CONNECTOR", "C1-RESTORE-ABSENT-REF",
    "C1-RESTORE-CONNECTOR-TARGET-ABSENT", "C1-RESTORE-OPID-BEFORE-EXISTENCE",
    "C1-RESTORE-LOCAL-REPLAY-REMOTE", "C1-RESTORE-NO-TOMBSTONE",
    "C1-RESTORE-SAME-PAYLOAD-NEW-OPID",
  ]],
  ["SetPlacements", "C-V02/C-V03/C-V05", [
    "C1-PLACEMENT-VALID", "C1-PLACEMENT-CYCLE", "C1-PLACEMENT-INVALID-PARENT",
    "C1-PLACEMENT-GROUP-ANY", "C1-PLACEMENT-STICKY-RICHTEXT", "C1-PLACEMENT-NONPARENT",
    "C1-HIERARCHY-STICKY", "C1-PLACEMENT-ORDERKEY",
  ]],
  ["SetTransforms", "C-V02/C-V03", [
    "C1-TRANSFORM-FINITE", "C1-TRANSFORM-NEGATIVE-ZERO", "C1-TRANSFORM-NAN-INF",
  ]],
  ["PatchProperties", "C-V02/C-V03", [
    "C1-PATCH-VALID", "C1-PATCH-FIELD-ID", "C1-PATCH-BRANCH-TYPE", "C1-PATCH-APPLICABILITY",
    "C1-PATCH-PRESENCE-DEFAULT", "C1-PATCH-DUPLICATE-FIELD",
  ]],
  ["SetObjectSize", "C-V02/C-V03", [
    "C1-SIZE-VALID", "C1-SIZE-WRONG-KIND", "C1-SIZE-NONFINITE", "C1-SIZE-NONPOSITIVE", "C1-SIZE-HARD-LIMIT",
  ]],
  ["SetVectorPathGeometry", "C-V02/C-V03/C-V05", [
    "C1-GEOMETRY-BOUNDARY", "C1-GEOMETRY-WRONG-KIND", "C1-GEOMETRY-STRUCTURAL",
    "C1-GEOMETRY-N-1", "C1-GEOMETRY-N", "C1-GEOMETRY-LIMIT", "C1-GEOMETRY-OVERFLOW",
  ]],
  ["SetImageContent", "C-V02/C-V03", [
    "C1-IMAGE-VALID", "C1-IMAGE-WRONG-KIND", "C1-IMAGE-CONTENT-PRESENCE", "C1-IMAGE-INTRINSIC",
    "C1-IMAGE-SOURCE-RECT", "C1-IMAGE-CONTENTMODE", "C1-IMAGE-LOCAL-SIZE",
    "C1-IMAGE-RUNTIME-RESOURCE-NONSEMANTIC",
  ]],
  ["AddStroke", "C-V02/C-V03/C-V04", [
    "C1-STROKE-VALID", "C1-STROKE-NEW-ID", "C1-STROKE-WRONG-CONTENT", "C1-STROKE-INVALID-RECORD", "C1-STROKE-EXISTING-ID",
  ]],
  ["SplitStrokes", "C-V02/C-V03/C-V06", [
    "C1-SPLIT-PLAN", "C1-SPLIT-SOURCE-MISSING", "C1-SPLIT-REPLACEMENT-STRUCTURAL", "C1-SPLIT-REPLACEMENT-COLLISION",
  ]],
  ["AddEraseMasks", "C-V02/C-V03", [
    "C1-ERASE-ADD-VALID", "C1-ERASE-ADD-UNIQUENESS", "C1-ERASE-ADD-GEOMETRY",
    "C1-ERASE-ADD-CAPABILITY", "C1-ERASE-ADD-EXISTING-MASK",
  ]],
  ["RemoveEraseMasks", "C-V02/C-V03", [
    "C1-ERASE-REMOVE-VALID", "C1-ERASE-REMOVE-MISSING", "C1-ERASE-REMOVE-DUPLICATE", "C1-ERASE-REMOVE-WHOLE-REJECT",
  ]],
  ["EditRichText", "C-V02/C-V03", [
    "C1-RICHTEXT-VALID", "C1-RICHTEXT-STABLE-REFS", "C1-RICHTEXT-UTF8-STYLE", "C1-RICHTEXT-INVALID-STEP",
  ]],
  ["SetConnectorContent", "C-V02/C-V03/C-V05", [
    "C1-CONNECTOR-VALID", "C1-CONNECTOR-ATTACHED-ENDPOINT", "C1-CONNECTOR-TARGET-CAPABILITY",
    "C1-CONNECTOR-ANCHOR", "C1-CONNECTOR-ROUTING", "C1-CONNECTOR-INVALID-END",
  ]],
];

export const MANDATORY_C5_COVERAGE: readonly MandatoryFamilySpec[] = FAMILY_ANCHORS.flatMap(
  ([operationFamily, authorityRequirement, caseIds]) => caseIds.map((id) => ({
    id,
    operationFamily,
    authorityRequirement,
    caseIds: [id],
  })),
);

function duplicateIds<T>(records: readonly T[], getId: (record: T) => string): string[] {
  const seen = new Set<string>();
  const duplicates = new Set<string>();
  for (const record of records) {
    const id = getId(record);
    if (seen.has(id)) duplicates.add(id);
    seen.add(id);
  }
  return [...duplicates].sort();
}

function idsOf<T>(records: readonly T[], getId: (record: T) => string): Set<string> {
  return new Set(records.map(getId));
}

function symmetricDifference(sets: readonly Set<string>[]): string[] {
  const all = new Set<string>();
  for (const set of sets) for (const id of set) all.add(id);
  return [...all].filter((id) => sets.some((set) => set.has(id)) && !sets.every((set) => set.has(id))).sort();
}

function assertSuiteShape(suite: CoreSuite): void {
  if (suite.format !== "axiom-g1-04-c-core-suite-v1" || suite.formatVersion !== 1 || suite.suiteId !== "GT-G1-04-C-CORE") {
    throw new Error("invalid C5 core suite shape");
  }
  if (!Array.isArray(suite.caseIds)) throw new Error("invalid C5 core suite caseIds");
}

export function validateCoreCoverage(input: {
  cases: readonly CaseIntent[];
  expected: readonly CExpectedOutcome[];
  suite: CoreSuite;
  observations: readonly ImplementationObservation[];
}): CoreCoverageReport {
  assertSuiteShape(input.suite);
  const caseIds = idsOf(input.cases, (record) => record.id);
  const expectedIds = idsOf(input.expected, (record) => record.caseId);
  const suiteIds = new Set(input.suite.caseIds);
  const observationIds = idsOf(input.observations, (record) => record.caseId);
  const operationFamilies = [...new Set(input.cases.map((record) => record.operationFamily))];
  const wrongFamilyCaseIds: string[] = [];
  for (const spec of MANDATORY_C5_COVERAGE) {
    const record = input.cases.find((candidate) => candidate.id === spec.id);
    if (record && record.operationFamily !== spec.operationFamily) wrongFamilyCaseIds.push(spec.id);
  }
  const missingMandatoryFamilies = MANDATORY_C5_COVERAGE
    .filter((spec) => !caseIds.has(spec.id) || !suiteIds.has(spec.id))
    .map((spec) => spec.id);
  const duplicateSuiteCaseIds = [
    ...duplicateIds(input.suite.caseIds, (id) => id),
    ...duplicateIds(input.cases, (record) => record.id),
    ...duplicateIds(input.expected, (record) => record.caseId),
    ...duplicateIds(input.observations, (record) => `${record.caseId}:${record.provider}`),
  ].filter((id, index, all) => all.indexOf(id) === index).sort();
  return {
    operationFamilies,
    mandatoryFamilies: MANDATORY_C5_COVERAGE,
    missingMandatoryFamilies,
    wrongFamilyCaseIds: [...new Set(wrongFamilyCaseIds)].sort(),
    unselectedCaseIds: symmetricDifference([caseIds, expectedIds, suiteIds, observationIds]),
    duplicateSuiteCaseIds,
  };
}

function mapUnique<T>(records: readonly T[], getId: (record: T) => string, label: string): Map<string, T> {
  const result = new Map<string, T>();
  for (const record of records) {
    const id = getId(record);
    if (result.has(id)) throw new Error(`duplicate ${label}: ${id}`);
    result.set(id, record);
  }
  return result;
}

function assertCoverageReady(report: CoreCoverageReport): void {
  if (report.operationFamilies.length !== OPERATION_FAMILIES.length || OPERATION_FAMILIES.some((family) => !report.operationFamilies.includes(family))) {
    throw new Error("C5 core corpus must cover exactly 15 operation families");
  }
  if (report.missingMandatoryFamilies.length > 0) throw new Error(`missing mandatory family: ${report.missingMandatoryFamilies.join(", ")}`);
  if (report.wrongFamilyCaseIds.length > 0) throw new Error(`wrong operation family: ${report.wrongFamilyCaseIds.join(", ")}`);
  if (report.unselectedCaseIds.length > 0) throw new Error(`unselected or extra case ids: ${report.unselectedCaseIds.join(", ")}`);
  if (report.duplicateSuiteCaseIds.length > 0) throw new Error(`duplicate C5 corpus ids: ${report.duplicateSuiteCaseIds.join(", ")}`);
}

function assertExpectedTrust(input: { expected: readonly CExpectedOutcome[] }): void {
  for (const expected of input.expected) {
    if (expected.provenance !== "AUTHORITY_MANUAL") throw new Error(`expected provenance is not AUTHORITY_MANUAL: ${expected.caseId}`);
    if (expected.mutationExpected !== false) throw new Error(`expected mutation contract is invalid: ${expected.caseId}`);
    if (expected.openPolicy === true) throw new Error(`accepted OPEN expected record requires Authority review: ${expected.caseId}`);
  }
}

function assertC3Evidence(evidence: C3ProjectionEvidence): void {
  if (evidence.format !== "axiom-gt-g1-04-c-plan-projection-v2" || evidence.formatVersion !== 2 || evidence.factsOnly !== true) {
    throw new Error("invalid C3 projection evidence trust envelope");
  }
  if (evidence.acceptedCases !== 90 || evidence.observationCount !== 180 || evidence.noMutationObservations !== 180 || evidence.observationRecords.length !== 180) {
    throw new Error("C3 projection evidence inventory is not exactly 90 cases / 180 observations / 180 no-mutation observations");
  }
}

function observationsByCaseAndProvider(observations: readonly ImplementationObservation[]): Map<string, Map<string, ImplementationObservation>> {
  const result = new Map<string, Map<string, ImplementationObservation>>();
  for (const observation of observations) {
    let providers = result.get(observation.caseId);
    if (!providers) {
      providers = new Map<string, ImplementationObservation>();
      result.set(observation.caseId, providers);
    }
    if (providers.has(observation.provider)) throw new Error(`duplicate observation provider: ${observation.caseId}/${observation.provider}`);
    providers.set(observation.provider, observation);
  }
  return result;
}

export function runCoreCorpus(input: {
  cases: readonly CaseIntent[];
  expected: readonly CExpectedOutcome[];
  suite: CoreSuite;
  c3Evidence: C3ProjectionEvidence;
  c3SourceRef: string;
}): CoreCorpusRun {
  assertSuiteShape(input.suite);
  assertC3Evidence(input.c3Evidence);
  assertExpectedTrust(input);
  const observations = input.c3Evidence.observationRecords;
  const coverage = validateCoreCoverage({ cases: input.cases, expected: input.expected, suite: input.suite, observations });
  assertCoverageReady(coverage);
  const casesById = mapUnique(input.cases, (record) => record.id, "case");
  const expectedById = mapUnique(input.expected, (record) => record.caseId, "expected");
  const observationsByCase = observationsByCaseAndProvider(observations);
  const results: ConformanceResult[] = [];
  for (const caseId of input.suite.caseIds) {
    const caseIntent = casesById.get(caseId);
    const expected = expectedById.get(caseId);
    const providers = observationsByCase.get(caseId);
    if (!caseIntent || !expected) throw new Error(`missing C5 record: ${caseId}`);
    if (!providers || providers.size !== 2 || !providers.has("reference") || !providers.has("indexed")) {
      throw new Error(`missing Reference/Indexed observation pair: ${caseId}`);
    }
    const reference = providers.get("reference") as ImplementationObservation;
    const indexed = providers.get("indexed") as ImplementationObservation;
    results.push(coordinateCase({
      caseIntent,
      expected,
      reference,
      indexed,
      referenceRef: `g1-04-c://c3/${input.c3SourceRef}/${caseId}/reference`,
      indexedRef: `g1-04-c://c3/${input.c3SourceRef}/${caseId}/indexed`,
      openAuthorityDecision: "UNRESOLVED",
    }));
  }
  return {
    suiteId: input.suite.suiteId,
    selectedCaseCount: input.suite.caseIds.length,
    selectedExpectedCount: results.length,
    selectedObservationCount: observations.length,
    operationFamilyCount: coverage.operationFamilies.length,
    coverage,
    results,
  };
}

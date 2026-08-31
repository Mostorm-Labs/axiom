import { isDeepStrictEqual } from "node:util";
import type { CaseIntent, CExpectedOutcome, ImplementationObservation } from "./types.js";

export const C6_IDEMPOTENCY_CASES = [
  "C1-IDEMPOTENT-EQUIVALENT",
  "C1-RESTORE-OPID-BEFORE-EXISTENCE",
  "C1-ID-COLLISION",
] as const;

export const C6_HIERARCHY_CASES = [
  "C1-PLACEMENT-VALID",
  "C1-PLACEMENT-GROUP-ANY",
  "C1-PLACEMENT-STICKY-RICHTEXT",
  "C1-PLACEMENT-NONPARENT",
  "C1-HIERARCHY-STICKY",
  "C1-INSERT-STICKY-CARDINALITY",
] as const;

export const C6_GEOMETRY_THRESHOLD_CASES = [
  "C1-GEOMETRY-N-1",
  "C1-GEOMETRY-N",
  "C1-GEOMETRY-LIMIT",
] as const;

const C3_SOURCE_REF = "906327beb9a268c339accd6d3ca6a7038e54ad68";
const C3_MATERIALIZED_REF = "2a601ab35a2294cb38d713ab001bbac7deaa9cf7";
const P36_R2_SOURCE_REF = "1763d57e7554ec690634326b998971f0decaae28";
const P36_FINAL_MATERIALIZED_REF = "c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d";
const GEOMETRY_BLOB = "4b79d3eef0b401519431f09f63e26abe3d4f5180";

type JsonRecord = Record<string, unknown>;

export interface CrossCuttingInput {
  cases: readonly CaseIntent[];
  expected: readonly CExpectedOutcome[];
  coreCorpusEvidence: unknown;
  noMutationEvidence: unknown;
  planProjectionEvidence: unknown;
  p36RepairEvidence: unknown;
  p36OverflowLineageEvidence: unknown;
}

export interface CrossCuttingRun {
  idempotency: {
    status: "PASS";
    cases: Array<{
      caseId: string;
      disposition: string;
      terminalPhase: string;
      reference: "PASS";
      indexed: "PASS";
    }>;
    orderingProof: string;
  };
  noMutation: {
    status: "PASS";
    acceptedCases: number;
    observationCount: number;
    beforeAfterEqual: number;
    observerMutationCalls: number;
    byDisposition: Record<string, { cases: number; observations: number; unchanged: number }>;
  };
  planProjection: {
    status: "PASS";
    factsOnly: true;
    cases: Array<{
      caseId: string;
      expected: unknown;
      reference: "PASS";
      indexed: "PASS";
    }>;
  };
  openReconciliation: {
    status: "PASS";
    closedGroups: string[];
    geometry: {
      thresholds: Record<string, { units: number; observed: string }>;
      checkedAddition: "INTEGER_OVERFLOW";
      checkedDabMultiplication: "INTEGER_OVERFLOW";
      checkedEraseMultiplication: "INTEGER_OVERFLOW";
      historicalOverflowCaseUsedAsArithmeticOracle: false;
    };
    restoreNoTombstone: "PASS";
    hierarchy: { status: "PASS"; rules: string[] };
  };
}

function asRecord(value: unknown, label: string): JsonRecord {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    throw new Error(`${label} must be an object`);
  }
  return value as JsonRecord;
}

function asArray(value: unknown, label: string): unknown[] {
  if (!Array.isArray(value)) throw new Error(`${label} must be an array`);
  return value;
}

function valueString(record: JsonRecord, key: string, label: string): string {
  const value = record[key];
  if (typeof value !== "string") throw new Error(`${label}.${key} must be a string`);
  return value;
}

function valueNumber(record: JsonRecord, key: string, label: string): number {
  const value = record[key];
  if (typeof value !== "number") throw new Error(`${label}.${key} must be a number`);
  return value;
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

function assertTrustedExpected(cases: readonly CaseIntent[], expected: readonly CExpectedOutcome[]): Map<string, CExpectedOutcome> {
  if (cases.length !== 90) throw new Error("C6 requires exactly 90 accepted cases");
  if (expected.length !== 90) throw new Error("C6 requires exactly 90 expected records");
  const caseMap = mapUnique(cases, (record) => record.id, "case");
  for (const record of cases) {
    if (record.provenance !== "AUTHORITY_MANUAL") throw new Error(`case provenance is not AUTHORITY_MANUAL: ${record.id}`);
    if (!caseMap.has(record.id)) throw new Error(`missing accepted case: ${record.id}`);
  }
  const expectedMap = mapUnique(expected, (record) => record.caseId, "expected");
  for (const record of expected) {
    if (record.provenance !== "AUTHORITY_MANUAL") throw new Error(`expected provenance is not AUTHORITY_MANUAL: ${record.caseId}`);
    if (record.mutationExpected !== false) throw new Error(`expected mutationExpected must be false: ${record.caseId}`);
    if (record.openPolicy === true) throw new Error(`accepted expected openPolicy must be false: ${record.caseId}`);
    if (!caseMap.has(record.caseId)) throw new Error(`expected has no accepted case: ${record.caseId}`);
    if (record.disposition === undefined || record.terminalPhase === undefined) {
      throw new Error(`expected contract is incomplete: ${record.caseId}`);
    }
  }
  for (const id of caseMap.keys()) if (!expectedMap.has(id)) throw new Error(`missing expected record: ${id}`);
  return expectedMap;
}

function assertCoreEvidence(evidence: unknown): void {
  const record = asRecord(evidence, "core corpus evidence");
  if (record.format !== "axiom-gt-g1-04-c-core-corpus-v1" || record.formatVersion !== 1) throw new Error("invalid C5 core corpus evidence");
  const basis = asRecord(record.acceptedBasis, "C5 accepted basis");
  if (basis.c3SourceRef !== C3_SOURCE_REF || (basis.c3MaterializedRef !== C3_SOURCE_REF && basis.c3MaterializedRef !== C3_MATERIALIZED_REF)) throw new Error("C5 C3 lineage identity is invalid");
  if (record.selectedCaseCount !== 90 || record.selectedExpectedCount !== 90 || record.selectedObservationCount !== 180) throw new Error("C5 inventory is not 90 cases / 90 expected / 180 observations");
  if (record.acceptedOpenPolicyCount !== 0 || record.acceptedExpectedAllAuthorityManual !== true) throw new Error("C5 accepted truth basis is invalid");
  if (record.expectedTruthWrites !== 0) throw new Error("C5 evidence reports expected truth writes");
  if (record.providerOutputUsedAsExpected !== false) throw new Error("provider output used as expected truth");
  if (record.productionSemanticDeltaFromC3 !== 0) throw new Error("C5 production semantic delta is non-zero");
  const counts = asRecord(record.resultStatusCounts, "C5 result status counts");
  if (counts.PASS !== 61 || counts.FAIL !== 29 || counts.OBSERVATION_ONLY !== 0) throw new Error("C5 result inventory is not 61 PASS / 29 FAIL / 0 OBSERVATION_ONLY");
  if (asArray(record.results, "C5 results").length !== 90) throw new Error("C5 result count is not 90");
}

function assertObservation(observation: unknown, label: string): ImplementationObservation {
  const record = asRecord(observation, label) as unknown as ImplementationObservation;
  if (record.format !== "axiom-g1-04-c-observation-v1" || record.formatVersion !== 1 || record.provenance !== "IMPLEMENTATION_OBSERVATION") throw new Error(`${label} trust envelope is invalid`);
  if (record.provider !== "reference" && record.provider !== "indexed") throw new Error(`${label} has unexpected provider`);
  if (typeof record.caseId !== "string") throw new Error(`${label} has invalid caseId`);
  return record;
}

function observationsByCase(observations: readonly unknown[], label: string): Map<string, Map<string, ImplementationObservation>> {
  const result = new Map<string, Map<string, ImplementationObservation>>();
  for (const [index, raw] of observations.entries()) {
    const observation = assertObservation(raw, `${label}[${index}]`);
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

function assertProviderPairs(observations: Map<string, Map<string, ImplementationObservation>>, expectedMap: Map<string, CExpectedOutcome>): void {
  if (observations.size !== 90) throw new Error("C6 requires observations for all 90 accepted cases");
  for (const caseId of expectedMap.keys()) {
    const providers = observations.get(caseId);
  if (!providers || providers.size !== 2 || !providers.has("reference") || !providers.has("indexed")) throw new Error(`missing Reference/Indexed provider pair: ${caseId}`);
  }
  for (const caseId of observations.keys()) if (!expectedMap.has(caseId)) throw new Error(`unexpected provider observation: ${caseId}`);
}

function assertProviderAgreement(observations: Map<string, Map<string, ImplementationObservation>>): void {
  for (const [caseId, providers] of observations) {
    const reference = providers.get("reference") as ImplementationObservation;
    const indexed = providers.get("indexed") as ImplementationObservation;
    if (
      reference.observedDisposition !== indexed.observedDisposition ||
      reference.observedTerminalPhase !== indexed.observedTerminalPhase ||
      reference.observedErrorCategory !== indexed.observedErrorCategory ||
      !isDeepStrictEqual(reference.observedPlanProjection, indexed.observedPlanProjection)
    ) {
      throw new Error(`provider agreement mismatch: ${caseId}`);
    }
  }
}

function assertNoMutationEvidence(evidence: unknown, expectedMap: Map<string, CExpectedOutcome>): { records: Map<string, Map<string, JsonRecord>>; observerMutationCalls: number } {
  const record = asRecord(evidence, "no-mutation evidence");
  if (record.format !== "axiom-gt-g1-04-c-no-mutation-v2" || record.formatVersion !== 2) throw new Error("invalid no-mutation evidence");
  if (record.acceptedCaseCount !== 90 || record.observationCount !== 180 || record.providerCount !== 2 || record.allBeforeAfterEqual !== true || record.unchangedObservationCount !== 180) throw new Error("no-mutation inventory is not 90 cases / 180 observations / unchanged");
  const mutationCalls = valueNumber(record, "observerMutationCalls", "no-mutation evidence");
  if (mutationCalls !== 0) throw new Error("observer mutation calls must be zero");
  const rows = asArray(record.records, "no-mutation records");
  if (rows.length !== 180) throw new Error("no-mutation records are not 180 observations");
  const byCase = new Map<string, Map<string, JsonRecord>>();
  for (const [index, raw] of rows.entries()) {
    const row = asRecord(raw, `no-mutation record ${index}`);
    const caseId = valueString(row, "caseId", `no-mutation record ${index}`);
    const provider = valueString(row, "provider", `no-mutation record ${index}`);
    if (provider !== "reference" && provider !== "indexed") throw new Error(`unexpected no-mutation provider: ${provider}`);
    const before = row.beforeProjection;
    const after = row.afterProjection;
    if (!isDeepStrictEqual(before, after) || row.beforeAfterEqual !== true) throw new Error(`before/after mutation detected: ${caseId}/${provider}`);
    let providers = byCase.get(caseId);
    if (!providers) { providers = new Map<string, JsonRecord>(); byCase.set(caseId, providers); }
    if (providers.has(provider)) throw new Error(`duplicate no-mutation provider: ${caseId}/${provider}`);
    providers.set(provider, row);
  }
  if (byCase.size !== 90) throw new Error("no-mutation evidence does not cover 90 cases");
  assertProviderPairs(observationsByCase(rows.map((row) => ({ format: "axiom-g1-04-c-observation-v1", formatVersion: 1, provenance: "IMPLEMENTATION_OBSERVATION", caseId: valueString(asRecord(row, "row"), "caseId", "row"), provider: valueString(asRecord(row, "row"), "provider", "row"), observedDisposition: "PLAN_READY", observedTerminalPhase: "PREPARE", beforeProjection: asRecord(row, "row").beforeProjection, afterProjection: asRecord(row, "row").afterProjection })), "no-mutation observations"), expectedMap);
  return { records: byCase, observerMutationCalls: mutationCalls };
}

function compareExpectedObservation(expected: CExpectedOutcome, observation: ImplementationObservation): boolean {
  if (expected.disposition !== observation.observedDisposition || expected.terminalPhase !== observation.observedTerminalPhase) return false;
  if (expected.semanticErrorCategory !== undefined && expected.semanticErrorCategory !== observation.observedErrorCategory) return false;
  if (expected.logicalPlanProjection !== undefined && !isDeepStrictEqual(expected.logicalPlanProjection, observation.observedPlanProjection)) return false;
  return true;
}

function assertLineageAndRepair(p36RepairEvidence: unknown, overflowEvidence: unknown): { geometry: JsonRecord; sticky: JsonRecord; connector: JsonRecord } {
  const repair = asRecord(p36RepairEvidence, "P36 upstream repair evidence");
  if (repair.format !== "axiom-gt-g1-04-c-p36-upstream-repair-v1" || repair.formatVersion !== 1) throw new Error("invalid P36 upstream repair evidence");
  if (repair.productionSemanticDelta !== 0) throw new Error("P36 production semantic delta is non-zero");
  const c3 = asRecord(repair.C3, "P36 C3 facts");
  if (c3.observationCount !== 180 || c3.noMutationCount !== 180 || c3.unexpectedHarnessErrors !== 0) throw new Error("P36 C3 facts are incomplete");
  const connector = asRecord(repair.connector, "P36 connector facts");
  const sticky = asRecord(repair.sticky, "P36 Sticky facts");
  const geometry = asRecord(repair.geometry, "P36 geometry facts");
  const overlay = asRecord(overflowEvidence, "P36 R2 overflow lineage");
  if (overlay.format !== "axiom-gt-g1-04-c-p36-r2-overflow-lineage-v1" || overlay.formatVersion !== 1) throw new Error("invalid P36 R2 overflow lineage evidence");
  if (overlay.sourceRef !== P36_R2_SOURCE_REF || overlay.previousMaterializedRef !== "2a601ab35a2294cb38d713ab001bbac7deaa9cf7") throw new Error("P36 R2 lineage identity is invalid");
  const lineage = asRecord(overlay.C3Lineage, "corrected C3 lineage");
  if (lineage.sourceRef !== C3_SOURCE_REF || lineage.materializedRef !== C3_MATERIALIZED_REF || lineage.metadataOnlyLineageCorrection !== true) throw new Error("corrected C3 lineage is missing or invalid");
  const binding = asRecord(overlay.productionSourceBinding, "production source binding");
  if (binding.path !== "runtime/semantic/src/validator.cpp" || binding.blobSha !== GEOMETRY_BLOB || binding.currentBlobSha !== GEOMETRY_BLOB || binding.allEqual !== true) throw new Error("geometry production binding is invalid");
  const arithmetic = asRecord(overlay.arithmeticOverflow, "arithmetic overflow");
  const addition = asRecord(arithmetic.checkedAddition, "checked addition");
  const multiplication = asRecord(arithmetic.checkedMultiplication, "checked multiplication");
  if (addition.expected !== "INTEGER_OVERFLOW" || addition.observed !== "INTEGER_OVERFLOW") throw new Error("checked addition overflow proof is invalid");
  if (multiplication.expected !== "INTEGER_OVERFLOW") throw new Error("checked multiplication overflow proof is invalid");
  const cases = asArray(multiplication.cases, "checked multiplication cases");
  if (cases.length !== 2 || cases.some((item) => asRecord(item, "multiplication case").weightValue !== 3 && asRecord(item, "multiplication case").weightValue !== 6)) throw new Error("checked Dab/Erase multiplication proof is incomplete");
  const historical = asRecord(overlay.corpusOverflowCase, "historical overflow case");
  if (historical.caseId !== "C1-GEOMETRY-OVERFLOW" || historical.usedAsArithmeticOverflowOracle !== false) throw new Error("historical C1-GEOMETRY-OVERFLOW cannot be the arithmetic oracle");
  return { geometry, sticky, connector };
}

export function runCrossCutting(input: CrossCuttingInput): CrossCuttingRun {
  const expectedMap = assertTrustedExpected(input.cases, input.expected);
  assertCoreEvidence(input.coreCorpusEvidence);
  const repair = assertLineageAndRepair(input.p36RepairEvidence, input.p36OverflowLineageEvidence);
  const planEvidence = asRecord(input.planProjectionEvidence, "plan projection evidence");
  if (planEvidence.format !== "axiom-gt-g1-04-c-plan-projection-v2" || planEvidence.formatVersion !== 2 || planEvidence.factsOnly !== true || planEvidence.acceptedCases !== 90 || planEvidence.observationCount !== 180 || planEvidence.noMutationObservations !== 180 || planEvidence.unexpectedHarnessErrors !== 0) throw new Error("invalid C3 plan projection evidence");
  const planObservations = observationsByCase(asArray(planEvidence.observationRecords, "plan projection observations"), "plan projection observations");
  assertProviderPairs(planObservations, expectedMap);
  assertProviderAgreement(planObservations);
  const noMutation = assertNoMutationEvidence(input.noMutationEvidence, expectedMap);

  const idempotencyCases = C6_IDEMPOTENCY_CASES.map((caseId) => {
    const expected = expectedMap.get(caseId);
    const providers = planObservations.get(caseId);
    if (!expected || !providers) throw new Error(`missing idempotency anchor: ${caseId}`);
    if (!compareExpectedObservation(expected, providers.get("reference") as ImplementationObservation) || !compareExpectedObservation(expected, providers.get("indexed") as ImplementationObservation)) throw new Error(`idempotency anchor must terminate at IDEMPOTENCY: ${caseId}`);
    if (expected.terminalPhase !== "IDEMPOTENCY") throw new Error(`idempotency ordering is not IDEMPOTENCY: ${caseId}`);
    return { caseId, disposition: expected.disposition as string, terminalPhase: expected.terminalPhase, reference: "PASS" as const, indexed: "PASS" as const };
  });

  const byDisposition: Record<string, { cases: number; observations: number; unchanged: number }> = {};
  for (const expected of expectedMap.values()) {
    const disposition = expected.disposition as string;
    const group = byDisposition[disposition] ?? { cases: 0, observations: 0, unchanged: 0 };
    group.cases += 1;
    group.observations += 2;
    const pair = noMutation.records.get(expected.caseId);
    if (!pair || pair.size !== 2) throw new Error(`missing no-mutation provider pair: ${expected.caseId}`);
    group.unchanged += 2;
    byDisposition[disposition] = group;
  }
  if (Object.keys(byDisposition).some((key) => byDisposition[key].observations === 0)) throw new Error("case assigned to no expected disposition");

  const projectionCases: Array<{ caseId: string; expected: unknown; reference: "PASS"; indexed: "PASS" }> = [];
  for (const expected of expectedMap.values()) {
    if (expected.logicalPlanProjection === undefined) continue;
    const providers = planObservations.get(expected.caseId);
    if (!providers || !compareExpectedObservation(expected, providers.get("reference") as ImplementationObservation) || !compareExpectedObservation(expected, providers.get("indexed") as ImplementationObservation)) throw new Error(`logical plan projection mismatch: ${expected.caseId}`);
    projectionCases.push({ caseId: expected.caseId, expected: expected.logicalPlanProjection, reference: "PASS", indexed: "PASS" });
  }
  const connector = repair.connector;
  if (connector.caseId !== "C1-DELETE-CASCADE" || !isDeepStrictEqual(connector.expectedDeleteClosure, ["14ad662e612a53fa46af20b1aa5fd0f5", "bed0e2f48ca23c0992a9a9ae8f8bf109"]) || !isDeepStrictEqual(connector.observedDeleteClosure, connector.expectedDeleteClosure) || connector.providersAgree !== true) throw new Error("Connector cascade closure proof is invalid");

  const geometry = repair.geometry;
  if (geometry.NMinus1Units !== 1999999 || geometry.NUnits !== 2000000 || geometry.NPlus1Units !== 2000001 || geometry.NMinus1Observed !== "PLAN_READY" || geometry.NObserved !== "PLAN_READY" || geometry.NPlus1Observed !== "GEOMETRY_LIMIT_EXCEEDED") throw new Error("geometry threshold proof is invalid");
  const restore = expectedMap.get("C1-RESTORE-NO-TOMBSTONE");
  if (!restore || restore.disposition !== "PLAN_READY" || restore.terminalPhase !== "PREPARE" || !compareExpectedObservation(restore, planObservations.get(restore.caseId)?.get("reference") as ImplementationObservation) || !compareExpectedObservation(restore, planObservations.get(restore.caseId)?.get("indexed") as ImplementationObservation)) throw new Error("Restore no-tombstone proof is invalid");
  for (const caseId of C6_HIERARCHY_CASES) {
    const expected = expectedMap.get(caseId);
    const providers = planObservations.get(caseId);
    if (!expected || !providers || !compareExpectedObservation(expected, providers.get("reference") as ImplementationObservation) || !compareExpectedObservation(expected, providers.get("indexed") as ImplementationObservation)) throw new Error(`hierarchy anchor mismatch: ${caseId}`);
  }
  if (valueString(repair.sticky, "placement", "P36 Sticky facts").includes("C1-PLACEMENT-STICKY-RICHTEXT") === false || valueString(repair.sticky, "cardinality", "P36 Sticky facts").includes("C1-INSERT-STICKY-CARDINALITY") === false) throw new Error("Sticky hierarchy/cardinality proof is invalid");

  return {
    idempotency: { status: "PASS", cases: idempotencyCases, orderingProof: "terminalPhase=IDEMPOTENCY proves termination before stateful validation" },
    noMutation: { status: "PASS", acceptedCases: 90, observationCount: 180, beforeAfterEqual: 180, observerMutationCalls: noMutation.observerMutationCalls, byDisposition },
    planProjection: { status: "PASS", factsOnly: true, cases: projectionCases },
    openReconciliation: {
      status: "PASS",
      closedGroups: ["connector-target-delete", "geometry-point-like-elements-per-operation-aggregate", "restore-no-tombstone", "hierarchy-parent-capability", "sticky-direct-richtext-cardinality"],
      geometry: {
        thresholds: {
          "C1-GEOMETRY-N-1": { units: 1999999, observed: "PLAN_READY" },
          "C1-GEOMETRY-N": { units: 2000000, observed: "PLAN_READY" },
          "C1-GEOMETRY-LIMIT": { units: 2000001, observed: "GEOMETRY_LIMIT_EXCEEDED" },
        },
        checkedAddition: "INTEGER_OVERFLOW",
        checkedDabMultiplication: "INTEGER_OVERFLOW",
        checkedEraseMultiplication: "INTEGER_OVERFLOW",
        historicalOverflowCaseUsedAsArithmeticOracle: false,
      },
      restoreNoTombstone: "PASS",
      hierarchy: { status: "PASS", rules: ["Root -> any", "Group -> any", "Sticky -> RichText only", "Sticky direct RichText cardinality = 0..1", "all other kinds = non-parent"] },
    },
  };
}

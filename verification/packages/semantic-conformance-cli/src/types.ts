export type Provider = "reference" | "indexed";
export type ConformanceStatus = "PASS" | "FAIL" | "OBSERVATION_ONLY";
export type OpenAuthorityDecision = "CURRENT_OPEN" | "CURRENT_CLOSED" | "UNRESOLVED";

export type DiagnosticCode =
  | "CASE_PROVENANCE_INVALID"
  | "EXPECTED_PROVENANCE_INVALID"
  | "REFERENCE_PROVENANCE_INVALID"
  | "INDEXED_PROVENANCE_INVALID"
  | "CASE_ID_MISMATCH"
  | "PROVIDER_SET_INVALID"
  | "EXPECTED_CONTRACT_INVALID"
  | "OPEN_AUTHORITY_UNRESOLVED"
  | "OPEN_POLICY_STALE_CLOSED"
  | "REFERENCE_MUTATION"
  | "INDEXED_MUTATION"
  | "REFERENCE_GOLDEN_MISMATCH"
  | "INDEXED_GOLDEN_MISMATCH"
  | "PROVIDER_DIVERGENCE"
  | "OPEN_POLICY_OBSERVATION_ONLY";

export interface CaseIntent {
  format: "axiom-g1-04-c-case-v1";
  formatVersion: 1;
  provenance: "AUTHORITY_MANUAL";
  id: string;
  operationFamily: string;
  authorityRuleRefs: string[];
  inputRef: string;
  expectedRef: string;
  blocking: boolean;
}

export interface CExpectedOutcome {
  format: "axiom-g1-04-c-expected-v1";
  formatVersion: 1;
  provenance: "AUTHORITY_MANUAL";
  caseId: string;
  authorityRuleRefs: string[];
  mutationExpected: false;
  disposition?: "PLAN_READY" | "ALREADY_APPLIED" | "REJECTED";
  terminalPhase?: "NORMALIZE" | "STATELESS_VALIDATE" | "IDEMPOTENCY" | "STATEFUL_VALIDATE" | "PREPARE";
  semanticErrorCategory?: string;
  logicalPlanProjection?: unknown;
  openPolicy?: boolean;
}

export interface ImplementationObservation {
  format: "axiom-g1-04-c-observation-v1";
  formatVersion: 1;
  provenance: "IMPLEMENTATION_OBSERVATION";
  caseId: string;
  provider: Provider;
  observedDisposition: "PLAN_READY" | "ALREADY_APPLIED" | "REJECTED";
  observedTerminalPhase: "NORMALIZE" | "STATELESS_VALIDATE" | "IDEMPOTENCY" | "STATEFUL_VALIDATE" | "PREPARE";
  observedErrorCategory?: string;
  observedPlanProjection?: unknown;
  beforeProjection: unknown;
  afterProjection: unknown;
}

export interface ConformanceResult {
  format: "axiom-g1-04-c-result-v1";
  formatVersion: 1;
  provenance: "CONFORMANCE_RESULT";
  caseId: string;
  status: ConformanceStatus;
  expectedRef: string;
  observationRefs: string[];
  diagnostics?: string[];
}

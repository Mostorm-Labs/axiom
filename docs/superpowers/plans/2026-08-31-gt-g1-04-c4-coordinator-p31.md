# GT-G1-04-C C4 Coordinator / Golden Comparison Contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the verification-only C4 coordinator that is the sole owner of PASS/FAIL comparison between human-reviewed `AUTHORITY_MANUAL` expected outcomes and facts-only Reference/Indexed `IMPLEMENTATION_OBSERVATION` records, without changing expected truth, production semantics, C3 observation behavior, or starting C5-C8 / GT-G1-05.

**Architecture:** C4 is a pure verification coordinator. It consumes accepted C0/C1 schemas and manual expected data plus C3-shaped observations, validates provenance/identity/no-mutation first, then compares only authority-asserted semantic fields. Provider agreement is secondary parity; it can never override a manual golden mismatch. Genuine Current-Authority OPEN cases are observation-only; unresolved/stale OPEN is fail-closed. C4 emits C0-compatible per-case `CONFORMANCE_RESULT` records and contract evidence, but does not own the later C7 final Gate aggregation.

**Tech Stack:** Node.js ESM, TypeScript 7, npm workspaces, `node:test`, existing verification JSON schemas/contracts.

**Spec:** GT-G1-04-C P20 Verification Design Reconciliation v0.1 (`notion:3cc4c57a-590c-81ae-ab73-d75501c47169`) + GT-G1-04-C P30 Implementation Plan v0.1 (`notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`).

## Global Constraints

- Repository: `Mostorm-Labs/axiom`.
- Branch hint: `codex/gt-g1-04-operation-apply`.
- C3 accepted materialized baseline: `855c114f36e4d4d4b9db9faaa28b96ae6d5249c6`.
- Task anchor:
  ```yaml
  revision: 855c114f36e4d4d4b9db9faaa28b96ae6d5249c6
  relation: ancestor
  ```
- `Task Anchor != Execution Cursor`: a later P32 may start from a legal descendant after reconciling the delta.
- C0-C3 are closed inputs; do not reopen or rewrite them in C4.
- Expected truth comes only from accepted `AUTHORITY_MANUAL` records. Production/Reference/Indexed output must never write, mutate, bless, or derive expected truth.
- C4 ends at comparison/conformance-result contract. Do not implement C5 corpus expansion, C6 cross-cutting suite expansion, C7 provider-diff/final Gate aggregation, C8 CI/evidence orchestration, or GT-G1-05 Atomic Apply.
- Every case must still prove zero canonical-store mutation independently of semantic match.
- `semanticErrorCategory` and `logicalPlanProjection` are compared only when the manual expected record asserts them. Their absence must not promote implementation-only detail into a contract.
- Current accepted C1 expected data contains no `openPolicy=true`. Runtime code must not invent Current OPEN cases from case names or implementation behavior.
- C4 may support a synthetic/injected `CURRENT_OPEN` decision for contract tests, but any real accepted `openPolicy=true` with no explicit Current-Authority decision must fail closed.
- A local-only P32 result is insufficient. Future P32 must return new immutable `source_ref` and later `materialized_ref` with reviewer-resolvable C4 evidence.

---

## 0. P31 Package Header

**Stage / task:** `GT-G1-04-C / C4 -> P31 Task Package Planning`

**Purpose:** Freeze the exact verification-only coordinator semantics, file ownership, unit-test matrix, durable evidence, and stop conditions before any C4 code execution.

**Package identity:** the immutable Git commit containing this plan is the C4 P31 `package_ref`. This file does not self-invent that SHA.

**Lifecycle boundary:** this package is P31 only. It does not authorize P32.

### Closed dependencies

```text
C0 = CLOSED / ACCEPTED
C1 = CLOSED / ACCEPTED_FOR_DOWNSTREAM
C2 = CLOSED / ACCEPTED_FOR_DOWNSTREAM
C3 = CLOSED / ACCEPTED_FOR_DOWNSTREAM / P34 PASS
```

Accepted C3 refs:

```yaml
package_ref: a46692ac00090d0fa06397c8aa1511704d742734
source_ref: c26c38feb192a7e584fa60a5ffbedf44f4b6e97a
materialized_ref: 855c114f36e4d4d4b9db9faaa28b96ae6d5249c6
```

C3 durable evidence consumed read-only:

```text
verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-PLAN-PROJECTION.json
verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-NO-MUTATION.json
```

---

## 1. Current Authority / Accepted Inputs

C4 consumes and must not redesign:

1. `notion:3cc4c57a-590c-81ae-ab73-d75501c47169` — Current GT-G1-04-C Verification Authority.
2. `notion:3cd4c57a-590c-8165-973f-ee31d93f1116` — Current terminal-phase classification sub-authority.
3. `notion:3cd4c57a-590c-8163-b606-e5f4e1eb4c92` — Current non-finite fixture-carrier sub-authority; C4 only consumes resulting observations.
4. `notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b` — Current P30 implementation slice DAG.
5. Accepted C0 schemas, read-only:
   - `verification/schemas/semantic/g1-04-c-case.schema.json`
   - `verification/schemas/semantic/g1-04-c-expected.schema.json`
   - `verification/schemas/semantic/g1-04-c-observation.schema.json`
   - `verification/schemas/semantic/g1-04-c-result.schema.json`
6. Accepted C1 authoring, read-only:
   - `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json`
   - `verification/corpus/semantic/v1/g1-04-c/authoring/expected.json`
   - `verification/corpus/semantic/v1/g1-04-c/suites/core.json`
7. Existing verification workspace/package conventions:
   - `verification/package.json`
   - `verification/packages/platform-conformance-cli/**`
8. Existing current-policy helper is read-only reference only:
   - `verification/tools/g1_04_c_contract.mjs`

If C4 discovers that the accepted C0 result schema cannot represent the required C4 result without schema changes, return `BLOCKED_UPSTREAM / C0_SCHEMA_CONTRACT` rather than editing C0 inside C4.

---

## 2. Frozen C4 Comparison Semantics

### 2.1 Result status vocabulary

C4 uses only this verification-only result status vocabulary inside the already-open C0 `status` string field:

```ts
export type ConformanceStatus = "PASS" | "FAIL" | "OBSERVATION_ONLY";
```

This is not Product ABI and not a production error enum.

Diagnostics are machine-stable verification strings. At minimum:

```ts
export type DiagnosticCode =
  | "EXPECTED_PROVENANCE_INVALID"
  | "REFERENCE_PROVENANCE_INVALID"
  | "INDEXED_PROVENANCE_INVALID"
  | "CASE_ID_MISMATCH"
  | "PROVIDER_SET_INVALID"
  | "OPEN_AUTHORITY_UNRESOLVED"
  | "OPEN_POLICY_STALE_CLOSED"
  | "REFERENCE_MUTATION"
  | "INDEXED_MUTATION"
  | "REFERENCE_GOLDEN_MISMATCH"
  | "INDEXED_GOLDEN_MISMATCH"
  | "PROVIDER_DIVERGENCE"
  | "OPEN_POLICY_OBSERVATION_ONLY";
```

### 2.2 Required precedence

Coordinator decision order is frozen:

```text
1. validate identity + provenance
2. validate explicit OPEN authority decision when openPolicy=true
3. validate no-mutation independently for Reference and Indexed
4. if genuine Current OPEN:
      emit OBSERVATION_ONLY
      do not choose a golden winner
      provider divergence may be diagnostic only
5. otherwise compare Reference to manual expected
6. compare Indexed to manual expected
7. compare providers on authority-asserted semantic fields
8. PASS only when all blocking checks above are clean
```

No-mutation failure is `FAIL` even for a genuine OPEN case because OPEN does not waive harness/evidence validity.

### 2.3 Golden comparison fields

For a closed case, always compare exactly:

```text
expected.disposition       <-> observation.observedDisposition
expected.terminalPhase     <-> observation.observedTerminalPhase
```

Conditional fields:

```text
if expected.semanticErrorCategory exists:
  require exact observedErrorCategory match
else:
  do not use observedErrorCategory for golden correctness or provider-divergence failure

if expected.logicalPlanProjection exists:
  require deep structural equality with observedPlanProjection
else:
  do not use observedPlanProjection to create a stronger golden contract
```

Provider parity uses the same authority-asserted field set. Both providers agreeing on the same wrong value remains `FAIL` with golden mismatch diagnostics and no false PASS.

### 2.4 No-mutation

For each provider:

```ts
beforeProjection deepEqual afterProjection
```

must hold. This check is independent from semantic outcome and expected golden comparison.

### 2.5 OPEN behavior

Represent the caller-resolved Authority state explicitly:

```ts
export type OpenAuthorityDecision = "CURRENT_OPEN" | "CURRENT_CLOSED" | "UNRESOLVED";
```

Rules:

```text
expected.openPolicy !== true
  -> normal closed comparison; open decision ignored

expected.openPolicy === true + CURRENT_OPEN
  -> OBSERVATION_ONLY after provenance/no-mutation checks

expected.openPolicy === true + CURRENT_CLOSED
  -> FAIL / OPEN_POLICY_STALE_CLOSED

expected.openPolicy === true + UNRESOLVED
  -> FAIL / OPEN_AUTHORITY_UNRESOLVED
```

The production C4 coordinator must never infer `CURRENT_OPEN` from a case ID, provider output, or old document wording.

---

## 3. Exact Authorized Future P32 Source Scope

A later P32 may create only:

```text
verification/packages/semantic-conformance-cli/package.json
verification/packages/semantic-conformance-cli/tsconfig.json
verification/packages/semantic-conformance-cli/src/types.ts
verification/packages/semantic-conformance-cli/src/corpus.ts
verification/packages/semantic-conformance-cli/src/provenance.ts
verification/packages/semantic-conformance-cli/src/compare.ts
verification/packages/semantic-conformance-cli/src/coordinator.ts
verification/packages/semantic-conformance-cli/src/main.ts
verification/packages/semantic-conformance-cli/test/compare.test.mjs
verification/packages/semantic-conformance-cli/test/coordinator.test.mjs
verification/packages/semantic-conformance-cli/test/provenance.test.mjs
verification/packages/semantic-conformance-cli/test/cli.test.mjs
verification/tools/generate_g1_04_c4_evidence.mjs
```

A later P32 may modify only:

```text
verification/package.json
verification/package-lock.json
```

The package should use no new third-party runtime dependency. TypeScript/Node built-ins and the existing workspace are sufficient.

Read-only in C4:

```text
verification/schemas/semantic/g1-04-c-*.schema.json
verification/corpus/semantic/v1/g1-04-c/authoring/**
verification/corpus/semantic/v1/g1-04-c/suites/**
verification/corpus/semantic/v1/g1-04-c/generated/**
verification/tools/g1_04_c_contract.mjs
verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/**
runtime/semantic/**
schema/axiom/v1/proto/**
```

Explicitly forbidden:

```text
expected.json mutation
cases.json mutation
generated fixture mutation
C3 observer/decoder/projection changes
production semantic changes
Product/public ABI changes
schema/proto changes
--bless / --update-golden / accept-current-output
provider output -> expected generation
C5 corpus implementation
C6 cross-cutting implementation
C7 final gate/provider-diff aggregation
C8 CI workflow implementation
GT-G1-05 Atomic Apply
```

If any required behavior needs a file outside this scope, stop and return for P31 scope review.

---

## 4. Required Interfaces

### `src/types.ts`

Define schema-shaped verification types only; do not redefine Product semantic models.

```ts
export type Provider = "reference" | "indexed";
export type ConformanceStatus = "PASS" | "FAIL" | "OBSERVATION_ONLY";
export type OpenAuthorityDecision = "CURRENT_OPEN" | "CURRENT_CLOSED" | "UNRESOLVED";

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
```

### `src/corpus.ts`

Provide deterministic lookup only; no coverage expansion:

```ts
export function indexByCaseId<T>(records: readonly T[], getId: (record: T) => string): Map<string, T>;
export function requireExpectedForCase(caseIntent: CaseIntent, expectedById: ReadonlyMap<string, CExpectedOutcome>): CExpectedOutcome;
```

Duplicate IDs must throw before comparison.

### `src/provenance.ts`

```ts
export function validateCoordinatorInputs(input: CoordinateCaseInput): string[];
```

It must reject wrong provenance, mismatched case IDs, duplicate/missing providers, invalid observation provider identity, and accepted closed-case expected records missing `disposition` or `terminalPhase`.

### `src/compare.ts`

```ts
export function deepEqualJson(left: unknown, right: unknown): boolean;
export function compareObservationToExpected(expected: CExpectedOutcome, observation: ImplementationObservation): string[];
export function compareProviders(expected: CExpectedOutcome, reference: ImplementationObservation, indexed: ImplementationObservation): string[];
```

Comparison is structural/deterministic; object key ordering must not affect equality and array ordering remains semantically significant.

### `src/coordinator.ts`

```ts
export interface CoordinateCaseInput {
  caseIntent: CaseIntent;
  expected: CExpectedOutcome;
  reference: ImplementationObservation;
  indexed: ImplementationObservation;
  referenceRef: string;
  indexedRef: string;
  openAuthorityDecision: OpenAuthorityDecision;
}

export function coordinateCase(input: CoordinateCaseInput): ConformanceResult;
```

`coordinateCase` is the sole C4 PASS/FAIL owner. It must not write files or read production code.

### `src/main.ts`

Verification-only CLI. Minimum command:

```text
axiom-semantic-conformance compare-case \
  --case <case-intent.json> \
  --expected <expected-record.json> \
  --reference <reference-observation.json> \
  --indexed <indexed-observation.json> \
  --reference-ref <stable-ref> \
  --indexed-ref <stable-ref> \
  --open-authority CURRENT_OPEN|CURRENT_CLOSED|UNRESOLVED
```

It prints exactly one `ConformanceResult` JSON object to stdout. No command may mutate authoring expected data.

---

## 5. Task 1 — Package scaffold + types

**Files:**
- Create package/tsconfig/types listed in Section 3.
- Modify `verification/package.json` only to add verification CLI/bin/script entrypoints while preserving existing workspace scripts.
- Modify `verification/package-lock.json` only for deterministic workspace registration.

- [ ] **Step 1: Write the package test that imports the future compiled `types`/coordinator entrypoint.**

Create `test/cli.test.mjs` with an initial smoke test that fails because `dist/main.js` does not exist.

- [ ] **Step 2: Run the package test and confirm RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

Expected: FAIL before implementation/build output exists.

- [ ] **Step 3: Add package scaffold and exact types from Section 4.**

Use `type: module`, `tsc -p tsconfig.json`, and a package-local `test` script following the existing workspace convention.

- [ ] **Step 4: Build/typecheck.**

```bash
cd verification
npm run build --workspace @axiom/semantic-conformance-cli
npm run typecheck --workspace @axiom/semantic-conformance-cli
```

Expected: PASS.

- [ ] **Step 5: Commit the scaffold separately.**

```bash
git add verification/packages/semantic-conformance-cli verification/package.json verification/package-lock.json
git commit -m "test(g1): scaffold C4 semantic coordinator"
```

---

## 6. Task 2 — Golden comparator semantics

**Files:**
- Create/modify `src/compare.ts`.
- Create `test/compare.test.mjs`.

- [ ] **Step 1: Add RED tests for authority-field comparison.**

Required cases:

```text
A. exact expected match -> no diagnostics
B. both providers agree on wrong disposition -> each receives golden mismatch; provider agreement does not erase mismatch
C. expected semanticErrorCategory absent + observations expose extra categories -> no golden/category assertion
D. expected semanticErrorCategory present -> exact match required
E. expected logicalPlanProjection present -> deep structural equality required
F. expected logicalPlanProjection absent -> observation plan detail is not promoted into golden truth
G. object key order differs but content equal -> equal
H. array order differs -> not equal
```

- [ ] **Step 2: Run comparator tests and confirm RED.**

```bash
cd verification
node --test packages/semantic-conformance-cli/test/compare.test.mjs
```

- [ ] **Step 3: Implement minimal pure comparison functions from Section 4.**

Do not import production semantic code and do not read `authoring/expected.json` inside comparator functions.

- [ ] **Step 4: Re-run comparator tests and confirm PASS.**

- [ ] **Step 5: Commit.**

```bash
git add verification/packages/semantic-conformance-cli/src/compare.ts verification/packages/semantic-conformance-cli/test/compare.test.mjs
git commit -m "feat(g1): add C4 golden comparator"
```

---

## 7. Task 3 — Provenance / identity fail-closed boundary

**Files:**
- Create/modify `src/corpus.ts`, `src/provenance.ts`.
- Create `test/provenance.test.mjs`.

- [ ] **Step 1: Add RED tests for:** wrong expected provenance, wrong observation provenance, mismatched case IDs, duplicate/missing provider identity, duplicate corpus IDs, and closed expected missing disposition/terminal phase.

- [ ] **Step 2: Run tests and confirm RED.**

```bash
cd verification
node --test packages/semantic-conformance-cli/test/provenance.test.mjs
```

- [ ] **Step 3: Implement deterministic indexing and provenance checks.**

The coordinator must not silently coerce malformed records into a semantic FAIL result if the input contract itself is invalid; it records explicit fail diagnostics.

- [ ] **Step 4: Re-run and confirm PASS.**

- [ ] **Step 5: Commit.**

```bash
git add verification/packages/semantic-conformance-cli/src/corpus.ts verification/packages/semantic-conformance-cli/src/provenance.ts verification/packages/semantic-conformance-cli/test/provenance.test.mjs
git commit -m "feat(g1): enforce C4 coordinator provenance"
```

---

## 8. Task 4 — Coordinator precedence + OPEN contract

**Files:**
- Create/modify `src/coordinator.ts`.
- Create `test/coordinator.test.mjs`.

- [ ] **Step 1: Add RED tests for the complete C4 matrix.**

At minimum:

```text
C4-T01 closed case, both providers match manual expected, no mutation
  -> PASS

C4-T02 both providers agree with each other but both mismatch manual expected
  -> FAIL
  -> REFERENCE_GOLDEN_MISMATCH + INDEXED_GOLDEN_MISMATCH
  -> must not become PASS

C4-T03 one provider matches, one differs
  -> FAIL
  -> one golden mismatch + PROVIDER_DIVERGENCE

C4-T04 semantic values match but Reference before != after
  -> FAIL / REFERENCE_MUTATION

C4-T05 semantic values match but Indexed before != after
  -> FAIL / INDEXED_MUTATION

C4-T06 invalid provenance
  -> FAIL before semantic golden comparison

C4-T07 synthetic expected.openPolicy=true + CURRENT_OPEN + no mutation
  -> OBSERVATION_ONLY / OPEN_POLICY_OBSERVATION_ONLY
  -> no golden winner selected

C4-T08 synthetic expected.openPolicy=true + CURRENT_CLOSED
  -> FAIL / OPEN_POLICY_STALE_CLOSED

C4-T09 synthetic expected.openPolicy=true + UNRESOLVED
  -> FAIL / OPEN_AUTHORITY_UNRESOLVED

C4-T10 CURRENT_OPEN but before != after
  -> FAIL, because OPEN does not waive no-mutation
```

- [ ] **Step 2: Run coordinator tests and confirm RED.**

```bash
cd verification
node --test packages/semantic-conformance-cli/test/coordinator.test.mjs
```

- [ ] **Step 3: Implement `coordinateCase` exactly in Section 2 precedence order.**

- [ ] **Step 4: Re-run and confirm PASS.**

- [ ] **Step 5: Commit.**

```bash
git add verification/packages/semantic-conformance-cli/src/coordinator.ts verification/packages/semantic-conformance-cli/test/coordinator.test.mjs
git commit -m "feat(g1): add C4 conformance coordinator"
```

---

## 9. Task 5 — Verification-only CLI contract

**Files:**
- Create/modify `src/main.ts`.
- Complete `test/cli.test.mjs`.

- [ ] **Step 1: Add RED CLI tests using temporary JSON inputs.**

Prove:

```text
valid closed inputs -> stdout is one result JSON, exit 0
semantic mismatch -> stdout result status FAIL, exit nonzero only if CLI contract chooses failure signaling; JSON must still be emitted
no command writes authoring/expected.json
unknown/missing args -> deterministic usage failure
```

The CLI must never expose `bless`, `update-golden`, `accept-current-output`, or equivalent options.

- [ ] **Step 2: Run CLI tests and confirm RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

- [ ] **Step 3: Implement minimal argument parsing with Node built-ins only.**

- [ ] **Step 4: Confirm CLI/package tests PASS.**

- [ ] **Step 5: Commit.**

```bash
git add verification/packages/semantic-conformance-cli/src/main.ts verification/packages/semantic-conformance-cli/test/cli.test.mjs
git commit -m "feat(g1): expose C4 verification coordinator CLI"
```

---

## 10. Task 6 — C4 durable contract evidence generator

**Files:**
- Create `verification/tools/generate_g1_04_c4_evidence.mjs`.

The generator must run the fixed C4-T01..T10 synthetic contract matrix through the built coordinator and emit deterministic facts only. It must additionally read the accepted C1 expected file only to prove:

```text
accepted expected record count = 90
accepted openPolicy=true count = 0
all accepted expected provenance = AUTHORITY_MANUAL
```

It must not run production code or generate/modify expected truth.

Expected future P32 evidence path:

```text
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-COORDINATOR-CONTRACT.json
```

Minimum evidence fields:

```json
{
  "format": "axiom-gt-g1-04-c-coordinator-contract-v1",
  "packageRef": "<C4 P31 package_ref>",
  "sourceRef": "<future C4 source_ref>",
  "taskAnchor": {
    "revision": "855c114f36e4d4d4b9db9faaa28b96ae6d5249c6",
    "relation": "ancestor"
  },
  "syntheticScenarioCount": 10,
  "syntheticScenarioPassCount": 10,
  "acceptedExpectedCount": 90,
  "acceptedOpenPolicyCount": 0,
  "expectedTruthWrites": 0,
  "productionSemanticDependencies": 0,
  "providerOutputUsedAsExpected": false,
  "verification": []
}
```

- [ ] **Step 1: Add evidence-generator invocation to package/contract test as RED.**
- [ ] **Step 2: Implement deterministic generator.**
- [ ] **Step 3: Run it twice to two temp files and require byte-for-byte equality after substituting the same source ref.**
- [ ] **Step 4: Commit.**

```bash
git add verification/tools/generate_g1_04_c4_evidence.mjs
git commit -m "test(g1): materialize C4 coordinator contract evidence"
```

---

## 11. Required P32 Verification Commands

Before freezing a future C4 `source_ref`, run:

```bash
cd verification
npm run typecheck --workspace @axiom/semantic-conformance-cli
npm run test --workspace @axiom/semantic-conformance-cli
npm run build --workspace @axiom/semantic-conformance-cli
npm run validate
cd ..
git diff --check
```

Required results:

```text
C4 synthetic contract matrix = 10/10 PASS
accepted expected inventory = 90
accepted openPolicy=true = 0
expected truth writes = 0
provider-output-to-expected paths = 0
new production semantic dependencies = 0
workspace validate = PASS
git diff --check = PASS
```

Because C4 is verification-only TypeScript and P32 is forbidden to modify C++/production semantic code, a full semantic CTest rerun is not a C4 package requirement unless P32 unexpectedly touches C++ — which would itself be outside scope and must stop for review.

---

## 12. Future Source / Evidence Boundary

P31 does not invent future refs.

```text
source_ref
= future immutable C4 implementation commit whose net tree delta from this package contains only the authorized C4 source/test/workspace files and no durable evidence directory

materialized_ref
= later immutable descendant commit adding only C4 durable evidence for that exact source_ref
```

Future materialization must add:

```text
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-COORDINATOR-CONTRACT.json
```

No C3 evidence may be copied/rebound under the C4 source ref. C4 may cite C3 evidence as read-only upstream evidence.

---

## 13. C4 Exit Criteria

C4 may return to CONTROL_REVIEW only when all are true:

1. C4 implementation is inside the exact authorized file scope.
2. Manual `AUTHORITY_MANUAL` expected remains immutable.
3. No production/reference/indexed output can create or mutate expected truth.
4. `coordinateCase` implements the frozen precedence exactly.
5. Both providers agreeing on a wrong result yields `FAIL`, never `PASS`.
6. Reference/Indexed divergence is surfaced independently from golden mismatch.
7. No-mutation failure independently yields `FAIL`.
8. Invalid provenance/identity fails closed.
9. Genuine injected `CURRENT_OPEN` yields `OBSERVATION_ONLY`, never a chosen winner.
10. Stale/unresolved OPEN yields `FAIL`.
11. Optional error/plan fields are compared only when manual expected asserts them.
12. Produced `ConformanceResult` objects remain compatible with accepted C0 result schema shape.
13. Synthetic C4 matrix = 10/10 PASS.
14. Existing accepted C1 expected inventory is read-only and remains 90 records with zero Current OPEN records.
15. Workspace typecheck/build/test/validate and `git diff --check` pass.
16. A new pure `source_ref` and later evidence-only `materialized_ref` are remotely reviewer-resolvable.

---

## 14. Blocked Return Behavior

Return immediately without widening scope on any of:

```text
BLOCKED_UPSTREAM / C0_SCHEMA_CONTRACT
  accepted result schema cannot represent required C4 output without modification

BLOCKED_AUTHORITY / OPEN_POLICY
  an accepted real case has openPolicy=true but Current Authority cannot resolve CURRENT_OPEN vs CURRENT_CLOSED

BLOCKED_UPSTREAM / C3_OBSERVATION_CONTRACT
  accepted C3 observation shape cannot be consumed without changing C3 or production semantics

BLOCKED_SCOPE
  required implementation needs files outside Section 3

BLOCKED_EVIDENCE
  exact source result cannot be materialized at reviewer-accessible durable ref

BLOCKED_EXECUTION_DIVERGENCE
  task anchor ancestry cannot be established or descendant delta conflicts with C4 scope
```

Do not repair C0-C3 or redesign Current Authority inside C4.

---

## 15. Required Future P32 Return

```yaml
stage: P32
task: GT-G1-04-C/C4
package_ref: <immutable commit containing this plan>
task_anchor: 855c114f36e4d4d4b9db9faaa28b96ae6d5249c6
execution_start_ref: <actual reconciled P32 starting revision>
source_ref: <new C4 implementation ref>
materialized_ref: <later C4 evidence ref>
evidence:
  - verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-COORDINATOR-CONTRACT.json
verification:
  coordinator_matrix: PASS_10_OF_10
  accepted_expected: 90
  accepted_open_policy: 0
  npm_typecheck: PASS
  npm_package_test: PASS
  npm_build: PASS
  npm_validate: PASS
  git_diff_check: PASS
scope:
  c0_c3_changed: NO
  expected_truth_changed: NO
  production_semantic_changed: NO
  c5_started: NO
  c6_c8_started: NO
  gt_g1_05_started: NO
status: READY_FOR_CONTROL_REVIEW
```

After P32 returns, stop at CONTROL_REVIEW. Do not automatically begin C5.

# GT-G1-04-C C7 Provider Differential + Gate Aggregation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans for P32 coding mechanics. This document is the Aegis P31 package; it does not itself authorize P32 until its immutable commit is returned as `package_ref`.

**Goal:** Materialize the GT-G1-04-C C7 provider-differential and Gate-aggregation slice. C7 must combine manual-golden correctness with Reference/Indexed parity without conflating them, consume accepted C0-C6 evidence, and emit source-bound `C-PROVIDER-DIFF.json` plus `C-GATE.json` without changing production semantics, expected truth, fixtures, C0-C6 evidence, or entering C8 / GT-G1-05.

**Critical current-state expectation:** the accepted repaired C5 corpus contains `61 PASS / 29 FAIL / 0 OBSERVATION_ONLY` while provider agreement is `90/90`. Therefore C7 must currently report **provider parity PASS** and **overall C Gate FAIL** because 29 blocking cases do not match `AUTHORITY_MANUAL` expected truth. That is the honest expected Gate result for this source state. C7 must not reinterpret provider agreement as semantic correctness and must not repair, bless, rewrite, suppress, or downgrade the 29 mismatches.

**Spec:**
- GT-G1-04-C P20 Verification Design Reconciliation v0.1 — `notion:3cc4c57a-590c-81ae-ab73-d75501c47169`
- GT-G1-04-C P30 Implementation Plan v0.1 — `notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`

---

## 0. P31 Package Identity / Control Boundary

Repository:

```text
Mostorm-Labs/axiom
```

Control branch for this package:

```text
codex/gt-g1-04-c7-provider-gate
```

Stage / slice:

```text
GT-G1-04-C / C7 -> P31 Task Package Planning
```

Fresh repository-state check immediately before C7 packaging:

```text
accepted downstream materialized baseline = 0213e1d910f43fde14a23577f0d9265b7521869d
repair branch HEAD                       = 0213e1d910f43fde14a23577f0d9265b7521869d
relation                                 = exact
new descendant delta                     = none
```

This package is created on a dedicated C7 branch directly from the accepted C6 P36/P34-re-review baseline. The immutable commit containing this file becomes the C7 P31 `package_ref`; this file does not self-invent that SHA.

Task anchor for later C7 P32/P33 execution:

```yaml
task_anchor:
  revision: 0213e1d910f43fde14a23577f0d9265b7521869d
  relation: ancestor
```

`Task Anchor != Execution Cursor`.

At P31 completion:

```text
resume_cursor = null
P32           = NOT_STARTED
```

Closed state inherited without re-review:

```text
C0 = CLOSED / PASS
C1 = CLOSED / PASS
C2 = CLOSED / PASS
C3 = CLOSED / PASS
C4 = CLOSED / PASS
C5 = CLOSED / PASS_WITH_FINDINGS / ACCEPTED_FOR_DOWNSTREAM
C6 = CLOSED / PASS_WITH_FINDINGS / ACCEPTED_FOR_DOWNSTREAM
C6 accepted source_ref       = 2bd2a2fa6502163d471995147daa683cefd7cf8f
C6 accepted materialized_ref = 0213e1d910f43fde14a23577f0d9265b7521869d
C7 = P31 PACKAGE ONLY
C8 = NOT_STARTED
GT-G1-05 = NOT_STARTED
```

The superseded blocked C6 lineage ending at `ebb798c9788359867edea191b57bc18cb71ff7ba` is historical failed-occurrence evidence only and is not a C7 baseline.

---

## 1. Current Authority / C7 Gate Contract

C7 inherits the P20/P30 C7 purpose:

```text
manual golden correctness = primary correctness oracle
Reference/Indexed parity  = secondary differential confidence
```

P30 freezes seven blocking C7 Gate conditions:

```text
1. authority/provenance validation PASS
2. mandatory corpus selected and valid
3. every blocking observation matches manual expected disposition/category/logical facts
4. zero canonical mutation for all cases
5. zero unexplained Reference/Indexed divergence
6. fixture regeneration reproducible
7. zero stale OPEN mapping
```

C7 must evaluate each condition independently. A provider-differential PASS cannot compensate for a manual-golden failure.

Current expected condition state from accepted inputs:

```text
1 authority/provenance       PASS
2 mandatory corpus           PASS
3 manual-golden correctness  FAIL  (29 blocking semantic golden mismatches)
4 no mutation                PASS  (180/180 unchanged, 0 mutation calls)
5 provider differential      PASS  (90/90 agreement, 0 unexplained divergence)
6 fixture reproducibility    expected PASS; C7 must freshly re-run the existing independent compiler twice
7 stale OPEN reconciliation  PASS  (OPEN = 0)

Overall C Gate               FAIL
```

This expected `FAIL` is not permission to infer the owning defect or repair it inside C7. P34/P35 own later classification. C7 only materializes truthful evidence.

---

## 2. Trusted Immutable Inputs

### 2.1 Repaired C5 corpus result

Read the current C5 repaired corpus from the accepted repaired evidence lineage:

```text
source key:
906327beb9a268c339accd6d3ca6a7038e54ad68

reviewer-accessible repaired materialization:
2a601ab35a2294cb38d713ab001bbac7deaa9cf7

artifact:
verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/C-CORE-CORPUS.json
```

Required accepted facts:

```text
selectedCaseCount             = 90
selectedExpectedCount         = 90
selectedObservationCount      = 180
operationFamilyCount          = 15
missingMandatoryFamilies      = []
acceptedOpenPolicyCount       = 0
acceptedExpectedAllAuthorityManual = true
expectedTruthWrites           = 0
providerOutputUsedAsExpected  = false
productionSemanticDeltaFromC3 = 0
PASS                          = 61
FAIL                          = 29
OBSERVATION_ONLY              = 0
```

The 29 failures are current semantic golden mismatches. C7 must preserve the exact failed-case inventory and diagnostics; it must not require C5 `PASS=90` as an implementation-test precondition, because the purpose of C7 is to surface the truthful Gate failure.

### 2.2 Accepted clean C6 evidence

Reviewer-accessible C6 materialized ref:

```text
0213e1d910f43fde14a23577f0d9265b7521869d
```

Source-bound evidence root:

```text
verification/evidence/gates/G1/2bd2a2fa6502163d471995147daa683cefd7cf8f/GT-G1-04-C/
```

Consume exactly:

```text
C-IDEMPOTENCY.json
C-NO-MUTATION.json
C-PLAN-PROJECTION.json
C-OPEN-RECONCILIATION.json
```

Required accepted C6 facts include:

```text
providerAgreement      = 90/90
beforeAfterEqual       = 180/180
observerMutationCalls  = 0
authorityManualExpected = true
expectedTruthWrites     = 0
providerOutputUsedAsExpected = false
productionSemanticDelta = 0
OPEN                     = 0
```

C7 does not reinterpret C6 geometry / Restore / hierarchy semantics; it only consumes their accepted aggregate status as Gate prerequisites.

### 2.3 Corrected C3 lineage

Where lineage identity is emitted, preserve:

```yaml
C3:
  source_ref: 906327beb9a268c339accd6d3ca6a7038e54ad68
  materialized_ref: 2a601ab35a2294cb38d713ab001bbac7deaa9cf7
```

Do not resurrect stale embedded C3 materialized-ref metadata from older C5 JSON.

### 2.4 Human-reviewed truth / fixture compiler

Read-only accepted roots:

```text
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/fixture-author/compile_g1_04_c.py
verification/schemas/semantic/g1-04-c-gate.schema.json
```

The fixture compiler is existing C2 implementation and remains read-only. C7 may execute it into temporary directories only.

---

## 3. Exact C7 P32 Source Scope

A future C7 P32 may create **only** these six files:

```text
verification/packages/semantic-conformance-cli/src/provider-diff.ts
verification/packages/semantic-conformance-cli/src/gate.ts
verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs
verification/packages/semantic-conformance-cli/test/gate.test.mjs
verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs
verification/tools/generate_g1_04_c7_evidence.mjs
```

All six paths are create-only. No existing file modification is authorized.

Existing package scripts already compile `src/**` and discover `test/*.test.mjs`; therefore `package.json`, `tsconfig.json`, workspace package files and lockfiles must remain unchanged.

If implementation genuinely requires a seventh source file or modification to any existing file, stop and return:

```text
BLOCKED_SCOPE / C7_P31_REFRESH
```

Do not widen scope opportunistically.

---

## 4. Explicit Non-Goals / Forbidden Changes

Do NOT:

- modify `runtime/semantic/**` production source or headers;
- modify ObjectStore APIs or add production instrumentation;
- modify schema/proto/public ABI;
- modify authoring `cases.json`, `expected.json`, suite membership or generated accepted fixtures;
- write provider output into expected truth;
- add bless/update-golden/accept-current-output paths;
- repair, suppress, reclassify or rewrite the 29 C5 golden mismatches;
- modify C0-C6 source, tests or accepted evidence;
- use the superseded `ebb798...` C6 occurrence as current evidence;
- start C8 workflow/CI implementation;
- start GT-G1-05;
- implement Atomic Apply, SemanticGeneration, ChangeSet, CanonicalCommitStamp or post-apply behavior;
- claim a final Aegis P34 verdict from inside C7 code.

---

## 5. Required Pure Provider-Differential Interface

Create `src/provider-diff.ts` as a pure parsed-value verifier. It must not access fs, Git, subprocesses, network, production runtime, or expected authoring files directly.

Conceptual surface:

```ts
export interface ProviderDiffInput {
  coreCorpusEvidence: unknown;
  c6NoMutationEvidence: unknown;
}

export interface ProviderDiffSummary {
  status: "PASS" | "FAIL";
  caseCount: number;
  providerPairCount: number;
  providerAgreement: string;
  divergenceCount: number;
  divergenceCaseIds: string[];
  goldenPassCount: number;
  goldenFailCount: number;
  observationOnlyCount: number;
  goldenMismatchCaseIds: string[];
  manualGoldenCorrectness: "PASS" | "FAIL";
}

export function summarizeProviderDiff(input: ProviderDiffInput): ProviderDiffSummary;
```

Required behavior:

1. Locally validate the evidence shapes before trusting fields.
2. Require C5 inventory `90 / 90 / 180`, 15 operation families, zero accepted OPEN, Authority-manual provenance and zero expected-truth writes.
3. Require C6 `providerAgreement=90/90`, `acceptedCases=90`, `observationCount=180`.
4. Count `PROVIDER_DIVERGENCE` independently from golden mismatch diagnostics.
5. Current accepted evidence must yield `divergenceCount=0` and provider-diff `status=PASS`.
6. Preserve current C5 `61 PASS / 29 FAIL / 0 OBSERVATION_ONLY` as manual-golden truth; `manualGoldenCorrectness=FAIL`.
7. Provider agreement must never turn a golden mismatch into PASS.
8. Preserve exact 29 failed case IDs in deterministic sorted order.
9. Unexpected diagnostics, missing provider-pair evidence, changed inventory or nonzero divergence fail the provider-diff verifier rather than being silently ignored.

`ProviderDiffSummary.status` reports parity only. It is not the C Gate status.

---

## 6. Required Pure Gate Interface

Create `src/gate.ts` as a pure aggregation function over parsed prerequisite facts.

Conceptual surface:

```ts
export type C7ConditionId =
  | "authority-provenance"
  | "mandatory-corpus"
  | "manual-golden-correctness"
  | "no-mutation"
  | "provider-differential"
  | "fixture-reproducibility"
  | "open-reconciliation";

export interface C7ConditionResult {
  id: C7ConditionId;
  status: "PASS" | "FAIL";
  evidenceRefs: string[];
}

export interface C7GateSummary {
  status: "PASS" | "FAIL";
  conditions: C7ConditionResult[];
  failedConditions: C7ConditionId[];
}

export function evaluateC7Gate(input: unknown): C7GateSummary;
```

Required behavior:

- exactly seven conditions must be present once each;
- overall PASS only if all seven are PASS;
- current accepted state must evaluate only `manual-golden-correctness` as FAIL;
- current overall C Gate must therefore be FAIL;
- no condition may be waived because another condition passes;
- `provider-differential=PASS` + `manual-golden-correctness=FAIL` is an explicitly tested valid state;
- gate logic must not classify defect ownership or propose repairs.

---

## 7. Fixture Reproducibility Requirement

C7 must freshly prove P30 condition 6 without modifying accepted fixture roots.

`generate_g1_04_c7_evidence.mjs` must execute the existing independent compiler twice into two temporary output directories using current accepted authoring inputs:

```text
python3 verification/fixture-author/compile_g1_04_c.py --output <tmp-a>
python3 verification/fixture-author/compile_g1_04_c.py --output <tmp-b>
```

Then compare the complete generated directory trees byte-for-byte, including manifest, inputs and provenance.

Required result:

```text
caseCount = 90
byte-identical = true
accepted authoring root writes = 0
accepted generated root writes = 0
```

Any difference blocks C7 evidence generation.

---

## 8. C7 Evidence Generator

Create:

```text
verification/tools/generate_g1_04_c7_evidence.mjs
```

CLI exactly:

```text
node verification/tools/generate_g1_04_c7_evidence.mjs --source-ref <sha> --output-dir <dir>
```

No other CLI options.

The generator must:

1. hard-bind the immutable C7 P31 `package_ref` once known at P32;
2. hard-bind task anchor `0213e1d910f43fde14a23577f0d9265b7521869d`;
3. verify package/task-anchor/source refs are resolvable and satisfy ancestry;
4. verify `package_ref -> source_ref` changed paths are exactly the six authorized create-only source files;
5. verify production semantic delta is zero;
6. verify accepted authoring, suite, fixture compiler, schema and C0-C6 evidence roots are unchanged from the task anchor;
7. read repaired C5 evidence from immutable materialized ref `2a601ab35a2294cb38d713ab001bbac7deaa9cf7`;
8. read clean C6 evidence from immutable materialized ref `0213e1d910f43fde14a23577f0d9265b7521869d`;
9. run fixture compiler twice into temp dirs and prove byte identity;
10. invoke pure provider-diff and gate aggregation code;
11. produce byte-deterministic JSON;
12. reject output paths inside accepted authoring/generated/schema/runtime/upstream evidence roots.

The generator must not execute Reference/Indexed production providers anew and must not rewrite expected truth. C7 aggregates accepted C5/C6 observations and evidence.

---

## 9. Durable C7 Evidence Contract

After freezing a distinct immutable C7 `source_ref`, materialize exactly **two** source-bound evidence files in a later evidence-only descendant:

```text
verification/evidence/gates/G1/<C7-source_ref>/GT-G1-04-C/C-PROVIDER-DIFF.json
verification/evidence/gates/G1/<C7-source_ref>/GT-G1-04-C/C-GATE.json
```

No third C7 aggregate artifact is authorized. C8 later owns exact-source CI orchestration, not a second semantic Gate definition.

### 9.1 `C-PROVIDER-DIFF.json`

Must include an identity envelope containing at least:

```text
gate = GT-G1-04-C
slice = C7
packageRef = <immutable C7 P31 package_ref>
sourceRef = <C7 source_ref>
taskAnchor.revision = 0213e1d910f43fde14a23577f0d9265b7521869d
P20 Authority ref
P30 plan ref
C5 repaired materialized ref = 2a601ab35a2294cb38d713ab001bbac7deaa9cf7
C6 materialized ref = 0213e1d910f43fde14a23577f0d9265b7521869d
C6 source ref = 2bd2a2fa6502163d471995147daa683cefd7cf8f
corrected C3 source/materialized refs
authorityManualExpected = true
expectedTruthWrites = 0
providerOutputUsedAsExpected = false
productionSemanticDelta = 0
```

Current expected payload facts:

```text
status                  = PASS       # provider differential only
caseCount               = 90
providerPairCount       = 90
providerAgreement       = 90/90
divergenceCount         = 0
divergenceCaseIds       = []
goldenPassCount         = 61
goldenFailCount         = 29
observationOnlyCount    = 0
manualGoldenCorrectness = FAIL
goldenMismatchCaseIds   = exact sorted 29-case inventory
```

The artifact must make the distinction between parity PASS and golden correctness FAIL unmistakable.

### 9.2 `C-GATE.json`

Must conform to existing `g1-04-c-gate.schema.json` and remain a verification-only Gate artifact:

```json
{
  "format": "axiom-g1-04-c-gate-v1",
  "formatVersion": 1,
  "provenance": "GATE_EVIDENCE",
  "gateId": "GT-G1-04-C",
  "status": "FAIL",
  "resultRefs": ["..."],
  "authorityRefs": ["..."],
  "sourceRef": "<C7-source-ref>"
}
```

For the current accepted source state, `status` must be `FAIL` because blocking condition 3 fails for 29 cases. The evidence generator/test must prove that all other six conditions pass.

`resultRefs` must reference the C7 provider-diff artifact plus the immutable upstream evidence used for the seven-condition decision; it must not point to local temp files.

A truthful Gate FAIL is a successful C7 implementation output. It is not grounds for the C7 implementation to rewrite inputs. P34 will decide downstream classification.

---

## 10. Required Tests / Oracles

### Provider-diff tests

Cover at minimum:

- current `61/29/0 + 90/90` => provider diff PASS, manual golden FAIL;
- provider divergence => provider diff FAIL even if golden fields otherwise match;
- provider agreement cannot convert a golden mismatch into PASS;
- changed/missing C5 inventory => fail closed;
- nonzero OPEN/invalid provenance/expected truth writes => fail closed;
- deterministic sorted mismatch inventory.

### Gate tests

Cover at minimum:

- seven PASS conditions => Gate PASS;
- current state: only manual-golden FAIL => Gate FAIL;
- provider parity FAIL => Gate FAIL;
- no-mutation FAIL => Gate FAIL;
- fixture reproducibility FAIL => Gate FAIL;
- stale OPEN => Gate FAIL;
- missing/duplicate condition => fail closed.

### Evidence tests

Cover at minimum:

- exact identity refs;
- exact source-bound paths;
- `C-PROVIDER-DIFF` current counts and statuses;
- `C-GATE` schema validity and current `status=FAIL`;
- double generation byte identity;
- exact-six source scope;
- exact-two evidence scope;
- production semantic delta = 0;
- no historical C6 failed-occurrence ref used as current evidence.

---

## 11. P32 Verification Requirements

Required focused verification before source freeze:

```text
@axiom/semantic-conformance-cli typecheck PASS
@axiom/semantic-conformance-cli test PASS
@axiom/semantic-conformance-cli build PASS
C7 focused tests PASS
fixture compiler double-run byte-identical PASS
git diff --check PASS
exact source scope PASS
production semantic delta = 0
```

After source freeze:

```text
C7 evidence generation twice PASS
byte-for-byte evidence comparison PASS
C-GATE schema validation PASS
source_ref -> materialized_ref exact two-file evidence delta PASS
```

Known pre-existing finding outside C7 repair scope:

```text
workspace npm run validate = 64/65
only accepted historical C5 source-scope guard mismatch
```

If full workspace validation is run and reproduces only that exact known failure, report it as pre-existing. Any new or different failure blocks C7.

---

## 12. P32 Materialization Sequence

1. Freshly inspect repository state and record `execution_start_ref`.
2. Confirm task anchor ancestry; if starting from a legal descendant, reconcile only descendant delta and do not reset valid history.
3. Implement only the six authorized create-only files.
4. Run focused verification and exact source-scope checks.
5. Freeze a distinct immutable `source_ref`.
6. Generate C7 evidence twice into temporary directories and prove byte identity.
7. Commit exactly two source-bound C7 evidence JSON files in one evidence-only descendant.
8. Push the execution branch/ref so both source and materialized refs are reviewer-accessible.
9. Return to CONTROL_REVIEW; do not self-execute P34.

Required identity separation:

```text
package_ref != source_ref
source_ref  != materialized_ref
```

If durable materialization is unavailable, return `BLOCKED_EVIDENCE`.

---

## 13. P32 Return Contract

Successful implementation return must include:

```yaml
stage: P32
task: GT-G1-04-C/C7
status: READY_FOR_CONTROL_REVIEW

package_ref: <C7 P31 package_ref>
task_anchor:
  revision: 0213e1d910f43fde14a23577f0d9265b7521869d
  relation: ancestor
execution_start_ref: <actual start>
source_ref: <immutable C7 source ref>
materialized_ref: <reviewer-accessible evidence descendant>

source_scope:
  changed_files: 6
  exact_authorized_six: PASS

evidence_scope:
  changed_files: 2
  exact_authorized_two: PASS

provider_diff:
  status: PASS
  provider_agreement: 90/90
  divergence_count: 0
  golden_pass: 61
  golden_fail: 29
  observation_only: 0
  manual_golden_correctness: FAIL

c_gate:
  status: FAIL
  failed_conditions:
    - manual-golden-correctness

fixture_reproducibility: PASS
production_semantic_delta: 0
authority_manual_expected: true
expected_truth_writes: 0
provider_output_used_as_expected: false

known_preexisting_findings:
  c5_semantic_golden_mismatches: 29
  historical_c5_scope_guard: preserved

c8: NOT_STARTED
gt_g1_05: NOT_STARTED
next:
  surface: CONTROL_REVIEW
  stage: P34
```

Also report exact pushed commits/refs, changed-file lists, and exact test commands/results.

A return with `c_gate.status=PASS` against the unchanged accepted `61/29/0` input is itself a C7 implementation defect and must not be claimed ready for review.

---

## 14. Blocked Return Behavior

Fail closed and preserve valid work if any of these occur:

- Authority contradiction or missing Current Authority -> `BLOCKED_AUTHORITY` / ownership handoff to Aegis;
- source scope requires any file outside the six authorized paths -> `BLOCKED_SCOPE / C7_P31_REFRESH`;
- task-anchor / execution lineage divergence -> `BLOCKED_EXECUTION_DIVERGENCE`;
- accepted upstream evidence missing/unresolvable -> `BLOCKED_MISSING_INPUT`;
- evidence cannot be durably materialized -> `BLOCKED_EVIDENCE`;
- any unexpected new test/build/determinism/schema failure -> precise `BLOCKED_IMPLEMENTATION` or `BLOCKED_ENVIRONMENT` as applicable.

Do not repair upstream semantics, Authority, golden data, C5 mismatches, C6 evidence, C8 CI, or GT-G1-05 inside a blocked C7 occurrence.

---

## 15. P31 Exit

P31 is complete only when this plan is committed as a reviewer-resolvable immutable `package_ref` on `codex/gt-g1-04-c7-provider-gate`.

At P31 completion:

```text
C7 = READY_FOR_P32
P32 = NOT_STARTED
C8 = NOT_STARTED
GT-G1-05 = NOT_STARTED
```

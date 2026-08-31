# GT-G1-04-C C6 Cross-Cutting / Adversarial Verification Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Materialize the GT-G1-04-C C6 cross-cutting verification slice that independently proves OperationId ordering, the full pre-apply no-mutation invariant, Authority-defined logical `PreparedApplyPlan` facts, and Current-Authority closed-policy reconciliation by consuming the already-repaired C1/C3/C5/P36 evidence basis without modifying production semantics, reopening C0-C5, repairing the 29 C5 semantic golden mismatches, or entering Atomic Apply.

**Architecture:** C6 is verification-only aggregation over immutable upstream facts. It reads the accepted human-authored `AUTHORITY_MANUAL` expected outcomes, the corrected C3 Reference/Indexed observation basis, the repaired C3 no-mutation/logical-plan artifacts, the current repaired C5 core-corpus result, and the P36 R1/R2 repair/lineage overlays. C6 adds a pure TypeScript cross-cutting verifier plus a source-bound evidence generator. It does not run a new fixture repair, does not bless provider behavior, does not mutate the accepted corpus, and does not use the historical `C1-GEOMETRY-OVERFLOW` C5 case as the arithmetic-overflow oracle.

**Tech Stack:** TypeScript 7 / Node.js ESM in the existing `@axiom/semantic-conformance-cli` workspace, Node built-ins only, immutable JSON verification artifacts, existing accepted C3/C4/C5 contracts.

**Spec:** GT-G1-04-C P20 Verification Design Reconciliation v0.1 (`notion:3cc4c57a-590c-81ae-ab73-d75501c47169`) + GT-G1-04-C P30 Implementation Plan v0.1 (`notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`).

---

## 0. P31 Package Identity / Control Boundary

Repository:

```text
Mostorm-Labs/axiom
```

Branch:

```text
codex/gt-g1-04-operation-apply
```

Stage / slice:

```text
GT-G1-04-C / C6 -> P31 Task Package Planning
```

Fresh repository-state check before this package was materialized:

```text
branch HEAD = c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
expected    = c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
relation    = exact
new delta   = none
```

The immutable Git commit containing this file is the C6 P31 `package_ref`. This file does not self-invent that SHA.

Task anchor for every later C6 P32 handoff:

```yaml
task_anchor:
  revision: c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
  relation: ancestor
```

`Task Anchor != Execution Cursor`.

At P31 completion:

```text
resume_cursor = null
P32           = NOT_STARTED
```

A later P32 may execute only after this package has a non-null immutable `package_ref` and a fresh P33-style reconciliation confirms the execution cursor is the package lineage or a compatible legal descendant. It must not reset history to the task anchor.

Closed lifecycle state consumed without re-review:

```text
C0 = CLOSED / PASS
C1 = CLOSED / PASS
C2 = CLOSED / PASS
C3 = CLOSED / PASS
C4 = CLOSED / PASS
C5 = CLOSED / PASS_WITH_FINDINGS / ACCEPTED_FOR_DOWNSTREAM
P35 = CLOSED
P36 = PASS_WITH_FINDINGS / CLOSED / ACCEPTED_FOR_DOWNSTREAM
C6 = P31 PACKAGE ONLY
C7 = NOT_STARTED
C8 = NOT_STARTED
GT-G1-05 = NOT_STARTED
```

---

## 1. Trusted Dependency Basis

### 1.1 Current Authority

C6 consumes, but does not modify or reinterpret:

```text
GT-G1-04-C P20 Verification Design Reconciliation v0.1
notion:3cc4c57a-590c-81ae-ab73-d75501c47169
```

Frozen C semantic lane:

```text
Operation
  -> Normalize / Canonicalize
  -> Stateless Validation
  -> OperationId Idempotency
  -> Stateful Validation
  -> PreparedApplyPlan
```

Explicitly outside C6:

```text
Atomic Apply
SemanticGeneration
ChangeSet
CanonicalCommitStamp
post-apply publication / no-echo
post-state mutation oracle
```

Current expected truth remains:

```text
provenance = AUTHORITY_MANUAL
mutationExpected = false
```

Allowed dispositions:

```text
PLAN_READY
ALREADY_APPLIED
REJECTED
```

Allowed terminal phases:

```text
NORMALIZE
STATELESS_VALIDATE
IDEMPOTENCY
STATEFUL_VALIDATE
PREPARE
```

Implementation-only fields are never expected truth.

### 1.2 P30 slice DAG

C6 consumes:

```text
GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31
notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b
```

C6 ownership inherited from P30:

```text
C5/C6 -> negative / stateful / idempotency cross-cutting proof
C3/C6 -> plan projection / no-mutation cross-cutting proof
C0/C6 -> Current CLOSED-policy reconciliation
```

C7 retains final provider-differential / Gate aggregation. C8 retains exact-source CI orchestration.

### 1.3 P36 repaired downstream basis

Final accepted materialized ref and C6 task anchor:

```text
c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
```

P36 Round 1:

```yaml
source_ref: 906327beb9a268c339accd6d3ca6a7038e54ad68
materialized_ref: 2a601ab35a2294cb38d713ab001bbac7deaa9cf7
```

P36 Round 2:

```yaml
source_ref: 1763d57e7554ec690634326b998971f0decaae28
materialized_ref: c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
```

Read-only repaired evidence:

```text
verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/C-CORE-CORPUS.json
verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/C-NO-MUTATION.json
verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/C-PLAN-PROJECTION.json
verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/P36-C6-UPSTREAM-REPAIR.json
verification/evidence/gates/G1/1763d57e7554ec690634326b998971f0decaae28/GT-G1-04-C/P36-R2-OVERFLOW-LINEAGE.json
```

P36 froze:

```text
production semantic delta = 0
C3 observations           = 180 / 180 no-mutation
C5 current repaired run   = 61 PASS / 29 FAIL / 0 OBSERVATION_ONLY
OPEN                      = 0
expected provenance       = AUTHORITY_MANUAL
expectedTruthWrites       = 0
provider agreement        = 90 / 90
```

The 29 C5 FAIL results are current semantic golden mismatches. They are not C6 P32 repair scope and do not, by themselves, block C6 package completeness.

### 1.4 Corrected C3 lineage is mandatory

The old embedded `C-CORE-CORPUS.json` metadata contains a stale materialized-ref relationship. C6 must ignore that stale embedded value and consume the append-only P36 R2 correction:

```yaml
C3:
  source_ref: 906327beb9a268c339accd6d3ca6a7038e54ad68
  materialized_ref: 2a601ab35a2294cb38d713ab001bbac7deaa9cf7
```

The authoritative correction source is:

```text
P36-R2-OVERFLOW-LINEAGE.json
```

Every C6 durable evidence file must record the corrected C3 relationship above and the P36 final materialized ref. No C6 code may treat `c3MaterializedRef=906327...` as current truth.

---

## 2. Why the Previous C6 Blocker Is Closed

The earlier C6 P31 occurrence returned `BLOCKED_MISSING_INPUT` because cross-cutting proof depended on upstream materialization that did not yet prove the real Connector cascade, Sticky hierarchy/cardinality, real geometry N-1/N/N+1 boundaries, arithmetic overflow, and corrected C3 lineage.

P35 classified that blocker as:

```text
primary   = TEST_DEFECT
secondary = EVIDENCE_GAP + DEPENDENCY_BLOCKER
Authority defect = NO
Spec defect      = NO
production implementation defect = NOT_ESTABLISHED
```

P36 R1/R2 then materially supplied the missing verification inputs while preserving production semantics.

Therefore C6 P31 must **not** inherit the old blocker mechanically. The current dependency basis is sufficient for packaging. If a later P32 finds a genuinely new Authority contradiction, it must fail closed and route back to Aegis; ordinary implementation choices inside the verified boundary are not Authority blockers.

---

## 3. Exact C6 Blocking Proof Groups

C6 does not expand the ordinary 15-Operation corpus. It reuses accepted case/expected records and repaired upstream evidence to prove cross-cutting invariants that two providers could otherwise get wrong in the same way.

### 3.1 Group A — OperationId equivalent replay / ordering

Primary anchors:

```text
C1-IDEMPOTENT-EQUIVALENT
C1-RESTORE-OPID-BEFORE-EXISTENCE
```

Blocking expected behavior:

```text
same OperationId + semantically equivalent Operation
=> disposition = ALREADY_APPLIED
=> terminalPhase = IDEMPOTENCY
```

The ordering proof is the Authority-visible terminal phase: the result terminates at `IDEMPOTENCY`, before stateful validation can reinterpret the now-current document state.

C6 must compare manual expected truth against both repaired providers. It must not infer equivalence from provider agreement alone.

### 3.2 Group B — OperationId collision

Primary anchor:

```text
C1-ID-COLLISION
```

Blocking expected behavior:

```text
same OperationId + different semantic payload
=> disposition = REJECTED
=> terminalPhase = IDEMPOTENCY
```

A collision observed as a later `STATEFUL_VALIDATE` failure does not satisfy C6 even if both providers agree.

### 3.3 Group C — Full no-mutation invariant

C6 owns the aggregate no-mutation proof across the accepted C lane, not only a hand-picked subset.

Inputs:

```text
90 AUTHORITY_MANUAL expected records
180 repaired C3 observations
C-NO-MUTATION.json from corrected C3 materialization
```

C6 must mechanically prove:

```text
for every accepted case:
  expected.mutationExpected == false

for every Reference/Indexed observation:
  canonical store projection before == after
```

The proof must cover all three dispositions that occur in the accepted corpus:

```text
PLAN_READY
REJECTED
ALREADY_APPLIED
```

Required aggregate condition:

```text
acceptedCases = 90
observations = 180
beforeAfterEqual = 180 / 180
observerMutationCalls = 0
```

No hidden mutation may be tolerated because a later operation would overwrite or compensate for it.

### 3.4 Group D — Connector cascade exact logical plan projection

Primary anchor:

```text
C1-DELETE-CASCADE
```

The manual expected `logicalPlanProjection` is exact Authority truth:

```json
{
  "creates": {"objects": []},
  "replacements": {"objects": []},
  "deletes": [
    "14ad662e612a53fa46af20b1aa5fd0f5",
    "bed0e2f48ca23c0992a9a9ae8f8bf109"
  ],
  "deleteClosure": [
    "14ad662e612a53fa46af20b1aa5fd0f5",
    "bed0e2f48ca23c0992a9a9ae8f8bf109"
  ]
}
```

C6 must compare exact Authority-defined logical facts, including the attached Connector discovered from target-only delete. `PLAN_READY` alone is insufficient.

More generally, C6 must scan every accepted expected record that actually declares `logicalPlanProjection` and require both providers to equal that manual projection. It may not create stronger expected plan fields for cases where Current Authority did not author them.

### 3.5 Group E — Geometry accounting / repaired upstream consumption

C6 must consume, not recreate, these closed facts:

```text
1,999,999 -> PLAN_READY
2,000,000 -> PLAN_READY
2,000,001 -> GEOMETRY_LIMIT_EXCEEDED
checked addition overflow       -> INTEGER_OVERFLOW
checked Dab multiplication      -> INTEGER_OVERFLOW
checked Erase multiplication    -> INTEGER_OVERFLOW
```

Threshold anchors:

```text
C1-GEOMETRY-N-1
C1-GEOMETRY-N
C1-GEOMETRY-LIMIT
```

Arithmetic-overflow oracle:

```text
P36-R2-OVERFLOW-LINEAGE.json
```

The arithmetic proof compiled exact production:

```text
runtime/semantic/src/validator.cpp
production blob = 4b79d3eef0b401519431f09f63e26abe3d4f5180
```

**Critical anti-confusion rule:**

```text
C1-GEOMETRY-OVERFLOW
!= arithmetic-overflow oracle
```

That historical C5 case observed `GEOMETRY_LIMIT_EXCEEDED` and remains one of the semantic golden mismatches. C6 must never reinterpret, rewrite, or bless it as the P36 R2 checked-arithmetic proof.

No new surrogate geometry fixture is authorized in C6.

### 3.6 Group F — Restore no-tombstone

Primary anchor:

```text
C1-RESTORE-NO-TOMBSTONE
```

Blocking expected behavior:

```text
disposition = PLAN_READY
terminalPhase = PREPARE
mutationExpected = false
```

C6 must prove the result from Current-Authority-visible input/current state. It must not add a production tombstone/history query, hidden-history fixture seam, or ObjectStore instrumentation.

The repaired C5 result for this case is consumed as a conformance fact; it is not the source of expected truth.

### 3.7 Group G — Hierarchy / Sticky capability cross-cutting state

Frozen Authority rules:

```text
Root   -> any
Group  -> any
Sticky -> RichText only
Sticky direct RichText cardinality = 0..1
all other kinds = non-parent
```

Blocking anchors:

```text
C1-PLACEMENT-VALID
C1-PLACEMENT-GROUP-ANY
C1-PLACEMENT-STICKY-RICHTEXT
C1-PLACEMENT-NONPARENT
C1-HIERARCHY-STICKY
C1-INSERT-STICKY-CARDINALITY
```

The value of using both `SetPlacements` and `InsertObjects` anchors is cross-operation consistency: parent capability/cardinality must be the same semantic rule regardless of the operation family that reaches stateful planning.

C6 consumes the P36-fixed fixture semantics. It does not modify Sticky/RichText fixtures or production hierarchy code.

---

## 4. C6 Closed-Policy / OPEN Reconciliation Contract

C6 must materialize a fresh `C-OPEN-RECONCILIATION.json` for the current source. It must not rewrite historical C0 evidence.

The current accepted expected inventory has:

```text
openPolicy=true = 0
```

C6 verification-only reconciliation labels are:

```text
connector-target-delete
geometry-point-like-elements-per-operation-aggregate
restore-no-tombstone
hierarchy-parent-capability
sticky-direct-richtext-cardinality
```

These labels are evidence keys, not new Product semantic identifiers.

Blocking Current-Authority status:

```text
connector-target-delete                         = CLOSED
geometry-point-like-elements-per-operation-aggregate = CLOSED
restore-no-tombstone                            = CLOSED
hierarchy-parent-capability                     = CLOSED
sticky-direct-richtext-cardinality              = CLOSED
```

C6 must reject stale `openPolicy=true` for any accepted record participating in these closed groups. Unknown future policy keys must not be silently promoted to CLOSED.

C6 must not modify:

```text
verification/tools/g1_04_c_contract.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
```

Those are accepted C0 artifacts. C6 owns its own cross-cutting contract/test surface.

---

## 5. Oracle Ownership / Provider Independence

C6 has one primary expected-truth owner:

```text
AUTHORITY_MANUAL authoring/expected.json
```

Trusted roles remain separate:

```text
Current Authority / AUTHORITY_MANUAL expected
  = expected truth

Reference provider observation
  = implementation fact

Indexed provider observation
  = implementation fact

C3 no-mutation / plan projection
  = facts-only observation evidence

C5 result
  = conformance result

P36 R1/R2
  = repaired evidence / lineage / exact-production proof
```

Forbidden truth flows:

```text
Reference -> expected
Indexed -> expected
OperationEngine output -> expected
observer output -> expected
C5 PASS/FAIL -> expected
P36 observed behavior -> rewritten expected
current implementation behavior -> bless/update expected
```

For P36 checked arithmetic, `INTEGER_OVERFLOW` remains the expected semantic category because Current Authority requires checked arithmetic; P36 R2 is evidence that exact production observes it, not a generator of the expectation.

Every C6 evidence file must record:

```text
expectedTruthWrites = 0
providerOutputUsedAsExpected = false
authorityManualExpected = true
```

---

## 6. Exact Authorized Future P32 File Scope

P31 itself changes only this plan document.

A later C6 P32 may create **only**:

```text
verification/packages/semantic-conformance-cli/src/cross-cutting.ts
verification/packages/semantic-conformance-cli/test/cross-cutting.test.mjs
verification/packages/semantic-conformance-cli/test/cross-cutting-evidence.test.mjs
verification/tools/generate_g1_04_c6_evidence.mjs
```

No existing source file needs modification. Existing workspace scripts already compile `src/**` and discover `test/*.test.mjs`.

### 6.1 Read-only inputs

```text
verification/corpus/semantic/v1/g1-04-c/README.md
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/corpus/semantic/v1/g1-04-c/generated/**
verification/schemas/semantic/g1-04-c-*.schema.json
verification/fixture-author/**

verification/packages/semantic-conformance-cli/src/types.ts
verification/packages/semantic-conformance-cli/src/corpus.ts
verification/packages/semantic-conformance-cli/src/provenance.ts
verification/packages/semantic-conformance-cli/src/compare.ts
verification/packages/semantic-conformance-cli/src/coordinator.ts
verification/packages/semantic-conformance-cli/src/core-corpus.ts
verification/packages/semantic-conformance-cli/src/main.ts
verification/packages/semantic-conformance-cli/package.json
verification/package.json
verification/package-lock.json

verification/tools/g1_04_c_contract.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
verification/tools/generate_g1_04_c5_evidence.mjs

runtime/semantic/tools/g1_04_c_*
runtime/semantic/tests/g1_04_c_*
runtime/semantic/include/canvas/semantic/**
runtime/semantic/src/**
schema/axiom/v1/proto/**

verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C/**
verification/evidence/gates/G1/1763d57e7554ec690634326b998971f0decaae28/GT-G1-04-C/**
```

### 6.2 Explicitly forbidden

```text
any authoring cases/expected mutation
any suite/core.json mutation
any generated fixture/provenance mutation
any C0-C5 source/test/evidence rewrite
any P35/P36 evidence rewrite
any production semantic source/header mutation
any ObjectStore API change
any production instrumentation
any schema/proto/public ABI change
any new tombstone/history API
any provider output -> expected generation
any bless/update-golden/accept-current-output path
any repair of the current 29 C5 semantic golden mismatches
any C7 provider-diff/final Gate aggregation
any C8 CI workflow work
any Atomic Apply / SemanticGeneration / ChangeSet / CanonicalCommitStamp
any GT-G1-05 work
```

If C6 implementation requires a file outside the four create-only paths, stop and return `BLOCKED_SCOPE / C6_P31_REFRESH` rather than widening scope opportunistically.

---

## 7. Required C6 Pure Interface

Create `verification/packages/semantic-conformance-cli/src/cross-cutting.ts` as a pure parsed-value verifier. It must not perform file-system access, Git queries, subprocess execution, or production imports.

Required conceptual surface:

```ts
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
  idempotency: unknown;
  noMutation: unknown;
  planProjection: unknown;
  openReconciliation: unknown;
}

export function runCrossCutting(input: CrossCuttingInput): CrossCuttingRun;
```

`unknown` above deliberately means C6-owned verification artifact shapes must be locally validated before use; it does **not** authorize untyped implementation logic. The implementation should define precise C6-local interfaces for the accepted evidence shapes it actually consumes rather than importing Product models or copying production logic.

Frozen behavior:

1. Require exactly 90 accepted case intents and exactly 90 expected records with one-to-one case IDs.
2. Require every expected record to remain `AUTHORITY_MANUAL` and `mutationExpected=false`.
3. Require current accepted `openPolicy=true` count to remain zero.
4. Require corrected C3 lineage from P36 R2 exactly (`906327...` -> `2a601...`).
5. Reject the stale embedded C3 materialized-ref value if code attempts to use it as the current dependency identity.
6. Require current repaired C5 inventory = 90 expected / 180 observations / 61 PASS / 29 FAIL / 0 OBSERVATION_ONLY / provider agreement 90/90.
7. Do not require global C5 semantic PASS=90. The 29 mismatches remain findings outside C6 repair scope.
8. Require each blocking C6 anchor case to exist with its accepted manual expected record.
9. Require equivalent replay and collision expected terminal phase = `IDEMPOTENCY`, with the exact dispositions in Section 3.
10. Require both providers to conform to those idempotency expected fields; provider agreement alone is insufficient.
11. Aggregate all 90 cases / 180 observations for no-mutation and group the proof by expected disposition.
12. Require all 180 before/after canonical store projections equal and zero observer mutation calls.
13. For every expected record declaring `logicalPlanProjection`, compare both providers to the exact manual projection; never infer a projection when manual expected omits it.
14. Require `C1-DELETE-CASCADE` exact projection from Section 3.4.
15. Require P36 repaired Connector cascade facts and Sticky hierarchy/cardinality facts to match the accepted selected cases.
16. Require geometry N-1/N/N+1 facts from P36 R1 and checked addition/Dab/Erase overflow facts from P36 R2.
17. Explicitly reject using `C1-GEOMETRY-OVERFLOW` as arithmetic-overflow evidence.
18. Require Restore no-tombstone and hierarchy/Sticky closed-policy groups to be represented by accepted manual cases and repaired conformance facts.
19. Unknown future policy keys remain unknown; C6 does not promote them to CLOSED.
20. Return only verification summaries/facts. Do not produce a final C Gate verdict; C7/P34 owns downstream classification.

---

## 8. Durable C6 Evidence Contract

A future C6 P32 source commit contains the four implementation/test/generator files only. After freezing that `source_ref`, materialize exactly four source-bound evidence files in a later evidence-only descendant commit:

```text
verification/evidence/gates/G1/<C6-source_ref>/GT-G1-04-C/C-IDEMPOTENCY.json
verification/evidence/gates/G1/<C6-source_ref>/GT-G1-04-C/C-NO-MUTATION.json
verification/evidence/gates/G1/<C6-source_ref>/GT-G1-04-C/C-PLAN-PROJECTION.json
verification/evidence/gates/G1/<C6-source_ref>/GT-G1-04-C/C-OPEN-RECONCILIATION.json
```

No fifth C6 aggregate/Gate artifact is authorized by this P31 package. C7 owns final Gate aggregation.

Every evidence file must carry the same identity envelope:

```json
{
  "gate": "GT-G1-04-C",
  "slice": "C6",
  "packageRef": "<immutable C6 P31 package_ref>",
  "sourceRef": "<immutable C6 source_ref>",
  "taskAnchor": {
    "revision": "c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d",
    "relation": "ancestor"
  },
  "authority": {
    "verification": "notion:3cc4c57a-590c-81ae-ab73-d75501c47169",
    "implementationPlan": "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b"
  },
  "trustedDependencies": {
    "p36FinalMaterializedRef": "c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d",
    "p36R1SourceRef": "906327beb9a268c339accd6d3ca6a7038e54ad68",
    "p36R1MaterializedRef": "2a601ab35a2294cb38d713ab001bbac7deaa9cf7",
    "p36R2SourceRef": "1763d57e7554ec690634326b998971f0decaae28",
    "correctedC3SourceRef": "906327beb9a268c339accd6d3ca6a7038e54ad68",
    "correctedC3MaterializedRef": "2a601ab35a2294cb38d713ab001bbac7deaa9cf7"
  },
  "authorityManualExpected": true,
  "expectedTruthWrites": 0,
  "providerOutputUsedAsExpected": false,
  "productionSemanticDelta": 0
}
```

### 8.1 `C-IDEMPOTENCY.json`

Must contain, at minimum:

```text
C1-IDEMPOTENT-EQUIVALENT
  expected ALREADY_APPLIED / IDEMPOTENCY
  Reference exact match
  Indexed exact match

C1-RESTORE-OPID-BEFORE-EXISTENCE
  expected ALREADY_APPLIED / IDEMPOTENCY
  Reference exact match
  Indexed exact match

C1-ID-COLLISION
  expected REJECTED / IDEMPOTENCY
  Reference exact match
  Indexed exact match
```

Evidence must explicitly state that `terminalPhase=IDEMPOTENCY` is the blocking ordering proof and therefore no later stateful rejection can substitute for idempotency termination.

### 8.2 `C-NO-MUTATION.json`

Must aggregate the complete accepted C lane:

```text
acceptedCases = 90
observationCount = 180
Reference observations = 90
Indexed observations = 90
beforeAfterEqual = 180 / 180
observerMutationCalls = 0
```

It must report per-disposition no-mutation counts for:

```text
PLAN_READY
REJECTED
ALREADY_APPLIED
```

Every accepted case must appear in exactly one expected-disposition group; every case must contribute exactly two provider observations.

### 8.3 `C-PLAN-PROJECTION.json`

Must remain `factsOnly=true` and include all accepted cases whose manual expected record declares `logicalPlanProjection`.

At minimum `C1-DELETE-CASCADE` must show exact expected/Reference/Indexed equality for:

```text
creates.objects
replacements.objects
deletes
deleteClosure
```

The exact two IDs from Section 3.4 are blocking. Internal planner-only fields must be absent from expected-truth comparison.

### 8.4 `C-OPEN-RECONCILIATION.json`

Must record all Current closed groups from Section 4, with authority/evidence basis and no stale `openPolicy=true` acceptance.

Geometry must record two distinct proof families:

```text
aggregate threshold:
  1,999,999
  2,000,000
  2,000,001

checked arithmetic:
  addition overflow
  Dab multiplication overflow
  Erase multiplication overflow
```

It must also record:

```text
historical C1-GEOMETRY-OVERFLOW used as arithmetic oracle = false
```

Restore must record:

```text
hidden tombstone/history dependency required = false
production tombstone API added = false
```

Hierarchy must record the Root/Group/Sticky/non-parent rules and Sticky `0..1` direct RichText cardinality.

---

## 9. C6 Evidence Generator Contract

Create:

```text
verification/tools/generate_g1_04_c6_evidence.mjs
```

The generator is responsible for Git/source identity and deterministic serialization; `cross-cutting.ts` stays pure.

CLI contract:

```bash
node verification/tools/generate_g1_04_c6_evidence.mjs \
  --source-ref <C6-source_ref> \
  --output-dir <directory>
```

Only those two options are permitted.

The generator must:

1. Hardcode the immutable C6 P31 `package_ref` after P31 materialization; do not infer it from mutable branch HEAD.
2. Hardcode the task anchor `c8fe64...d80d` and P36 immutable dependency refs from Section 1.
3. Verify `task_anchor` and `package_ref` are ancestors of `source_ref`.
4. Verify the exact source delta from package ref to source ref consists of the four authorized C6 files only.
5. Verify accepted authoring `cases.json`, `expected.json`, `suites/core.json`, generated inputs, existing C0-C5 code/evidence roots, P35/P36 evidence, production semantic source/header, and schema/proto are unchanged.
6. Verify no production semantic delta from the C6 task anchor to the C6 source ref.
7. Read repaired C3 evidence from the immutable corrected materialized ref `2a601ab...`, not a mutable working-tree copy.
8. Read the R2 correction overlay from immutable final materialized ref `c8fe64...`.
9. Treat the R2 overlay as authoritative if old embedded C3 lineage metadata conflicts.
10. Read the current repaired C5 core-corpus evidence as a conformance-result dependency while preserving its 61/29 result honestly.
11. Call `runCrossCutting(...)` once and emit the exact four evidence documents from Section 8.
12. Reject output paths under authoring/generated/schema/runtime/P35/P36 or any prior evidence root.
13. Generate byte-for-byte identical files for the same source ref.
14. Never write expected truth, fixtures, upstream evidence, or production files.

Use `execFileSync("git", [...])` only for read-only identity/diff/show queries. Do not concatenate untrusted shell strings.

---

## 10. Task 1 — Freeze Cross-Cutting Dependency and Case Contracts

**Files:**
- Create: `verification/packages/semantic-conformance-cli/src/cross-cutting.ts`
- Create/Test: `verification/packages/semantic-conformance-cli/test/cross-cutting.test.mjs`

- [ ] **Step 1: Write RED tests for dependency identity and trust.**

Synthetic corruptions must fail for:

```text
wrong C3 source_ref
wrong corrected C3 materialized_ref
attempt to use stale embedded c3MaterializedRef=906327...
wrong P36 final materialized ref
missing P36 R2 overlay
non-AUTHORITY_MANUAL expected
mutationExpected=true
real accepted openPolicy=true
missing C6 anchor case
missing provider observation
unexpected provider
provider output used as expected
changed accepted case/expected identity
```

- [ ] **Step 2: Prove RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

Expected: fail because `cross-cutting.ts` does not exist yet.

- [ ] **Step 3: Implement the minimal C6-local evidence shape validation and frozen anchor sets.**

No file-system/Git code in `cross-cutting.ts`.

- [ ] **Step 4: Re-run package tests and confirm PASS.**

- [ ] **Step 5: Commit only the two C6 files.**

Suggested commit:

```text
test(g1): freeze C6 cross-cutting contracts
```

---

## 11. Task 2 — Prove Idempotency Ordering and Full No-Mutation

**Files:**
- Modify only the new C6-owned `cross-cutting.ts`
- Extend only the new C6-owned `cross-cutting.test.mjs`

- [ ] **Step 1: Add RED idempotency tests.**

Prove failures for:

```text
equivalent replay -> anything except ALREADY_APPLIED
collision -> anything except REJECTED
either idempotency anchor terminalPhase != IDEMPOTENCY
Reference matches but Indexed does not
Indexed matches but Reference does not
both providers agree with each other but disagree with manual expected
```

- [ ] **Step 2: Add RED full no-mutation aggregation tests.**

Prove failures for:

```text
89 or 91 accepted cases
179 or 181 observations
missing Reference/Indexed pair
before/after mismatch in PLAN_READY
before/after mismatch in REJECTED
before/after mismatch in ALREADY_APPLIED
observerMutationCalls != 0
case assigned to no expected disposition
```

- [ ] **Step 3: Implement minimal deterministic aggregation.**

No production call is authorized. The implementation consumes immutable observations only.

- [ ] **Step 4: Re-run tests and confirm PASS.**

- [ ] **Step 5: Commit.**

Suggested commit:

```text
test(g1): prove C6 idempotency and no-mutation
```

---

## 12. Task 3 — Prove Exact Logical Plan and Closed Policies

**Files:**
- Modify only the new C6-owned `cross-cutting.ts`
- Extend only the new C6-owned `cross-cutting.test.mjs`

- [ ] **Step 1: Add RED Connector plan tests.**

Fail if:

```text
C1-DELETE-CASCADE missing
manual expected projection missing
delete target absent from closure
attached Connector absent from closure
extra delete appears
creates/replacements differ
Reference != exact manual projection
Indexed != exact manual projection
implementation-only planner field is treated as expected truth
```

- [ ] **Step 2: Add RED geometry tests.**

Fail if:

```text
N-1 != 1,999,999 PLAN_READY
N != 2,000,000 PLAN_READY
N+1 != 2,000,001 GEOMETRY_LIMIT_EXCEEDED
checked addition != INTEGER_OVERFLOW
checked Dab multiply != INTEGER_OVERFLOW
checked Erase multiply != INTEGER_OVERFLOW
production blob != 4b79d3eef0b401519431f09f63e26abe3d4f5180
historical C1-GEOMETRY-OVERFLOW is selected as arithmetic oracle
```

- [ ] **Step 3: Add RED Restore/hierarchy closed-policy tests.**

Fail if:

```text
C1-RESTORE-NO-TOMBSTONE is absent or not manual expected
Restore proof claims hidden tombstone/history is required
Root/Group/Sticky/non-parent coverage anchor missing
Sticky direct RichText second-child cardinality anchor missing
any selected closed group is tagged openPolicy=true
unknown future policy is silently promoted to CLOSED
```

- [ ] **Step 4: Implement the minimal cross-cutting proof summaries.**

Do not modify the accepted C0 OPEN helper or P36 fixtures.

- [ ] **Step 5: Re-run tests and confirm PASS, then commit.**

Suggested commit:

```text
test(g1): prove C6 plans and closed policies
```

---

## 13. Task 4 — Add Deterministic Source-Bound C6 Evidence Generation

**Files:**
- Create: `verification/tools/generate_g1_04_c6_evidence.mjs`
- Create/Test: `verification/packages/semantic-conformance-cli/test/cross-cutting-evidence.test.mjs`

- [ ] **Step 1: Add RED generator tests.**

Tests must prove:

```text
same source_ref twice -> four byte-identical outputs
missing --source-ref -> failure
missing --output-dir -> failure
unknown option -> failure
package_ref not ancestor -> failure
task_anchor not ancestor -> failure
source scope includes any fifth file -> failure
authoring blob changed -> failure
C0-C5 accepted code/evidence changed -> failure
P36 evidence changed -> failure
production semantic delta -> failure
corrected C3 lineage absent/wrong -> failure
output under forbidden truth/runtime/upstream-evidence root -> failure
```

- [ ] **Step 2: Run tests and verify RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

- [ ] **Step 3: Implement generator with Node built-ins only.**

It must serialize exactly the four Section 8 evidence files.

- [ ] **Step 4: Run focused package regression.**

```bash
cd verification
npm run typecheck --workspace @axiom/semantic-conformance-cli
npm run test --workspace @axiom/semantic-conformance-cli
npm run build --workspace @axiom/semantic-conformance-cli
```

Expected: PASS.

- [ ] **Step 5: Commit.**

Suggested commit:

```text
test(g1): add C6 cross-cutting evidence generator
```

---

## 14. Task 5 — Freeze Pure C6 Source Ref

- [ ] **Step 1: Re-run focused C6/package tests.**

```bash
cd verification
npm run typecheck --workspace @axiom/semantic-conformance-cli
npm run test --workspace @axiom/semantic-conformance-cli
npm run build --workspace @axiom/semantic-conformance-cli
cd ..
git diff --check
```

- [ ] **Step 2: Prove exact C6 source scope.**

```bash
git diff --name-only <C6-package_ref> HEAD
```

Must equal exactly:

```text
verification/packages/semantic-conformance-cli/src/cross-cutting.ts
verification/packages/semantic-conformance-cli/test/cross-cutting.test.mjs
verification/packages/semantic-conformance-cli/test/cross-cutting-evidence.test.mjs
verification/tools/generate_g1_04_c6_evidence.mjs
```

- [ ] **Step 3: Prove production semantics unchanged from task anchor.**

```bash
git diff --quiet \
  c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d \
  HEAD -- \
  runtime/semantic/include/canvas/semantic \
  runtime/semantic/src
```

Expected: clean.

- [ ] **Step 4: Prove accepted truth / upstream verification roots unchanged.**

```bash
git diff --exit-code \
  c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d \
  HEAD -- \
  verification/corpus/semantic/v1/g1-04-c \
  verification/schemas/semantic \
  verification/fixture-author \
  verification/tools/g1_04_c_contract.mjs \
  verification/tests/g1_04_c_open_reconciliation.test.mjs \
  verification/tools/generate_g1_04_c5_evidence.mjs \
  verification/evidence/gates/G1/906327beb9a268c339accd6d3ca6a7038e54ad68/GT-G1-04-C \
  verification/evidence/gates/G1/1763d57e7554ec690634326b998971f0decaae28/GT-G1-04-C \
  schema/axiom/v1/proto
```

Expected: clean.

- [ ] **Step 5: Record/push the final implementation commit as immutable `source_ref`.**

The `source_ref` is not the P31 package ref and not the later evidence commit.

### Accepted workspace-validation finding

At the P36 R2 accepted basis, full `cd verification && npm run validate` has a known historical C5 source-scope guard mismatch while the focused P36 tests, full semantic CTest, and source-scope checks passed. C6 is forbidden to repair that historical C5 guard.

Therefore C6 package completeness is gated by the focused semantic-conformance workspace package typecheck/test/build above plus exact source-scope/no-production-delta checks. If P32 also runs full `npm run validate`, it must report the accepted historical C5 guard finding separately. A **new or different** validation failure introduced by C6 is blocking; the pre-existing accepted C5 guard finding is not authorization to edit C5.

---

## 15. Task 6 — Materialize Four C6 Evidence Files as Evidence-Only Descendant

Only after immutable `source_ref` exists:

- [ ] **Step 1: Generate twice into two temporary directories.**

```bash
node verification/tools/generate_g1_04_c6_evidence.mjs \
  --source-ref <C6-source_ref> \
  --output-dir /tmp/c6-a
node verification/tools/generate_g1_04_c6_evidence.mjs \
  --source-ref <C6-source_ref> \
  --output-dir /tmp/c6-b
diff -ru /tmp/c6-a /tmp/c6-b
```

Expected: no diff.

- [ ] **Step 2: Copy exactly four files to the source-bound evidence root.**

```text
verification/evidence/gates/G1/<C6-source_ref>/GT-G1-04-C/
```

- [ ] **Step 3: Verify evidence-only working-tree delta.**

Only these may be new:

```text
C-IDEMPOTENCY.json
C-NO-MUTATION.json
C-PLAN-PROJECTION.json
C-OPEN-RECONCILIATION.json
```

- [ ] **Step 4: Commit evidence only.**

Suggested commit:

```text
evidence(g1): materialize C6 cross-cutting proof
```

- [ ] **Step 5: Record/push that commit as immutable `materialized_ref`.**

Any implementation change after `source_ref` invalidates this materialization and requires a new source ref/evidence generation. Historical P36 evidence is never rewritten.

---

## 16. Required Test / Oracle Matrix

| Dimension | Blocking proof | Oracle owner | C6 completion condition |
|---|---|---|---|
| Equivalent replay | ALREADY_APPLIED at IDEMPOTENCY | manual expected / C-V04 | both providers exact match |
| OperationId collision | REJECTED at IDEMPOTENCY | manual expected / C-V04 | both providers exact match |
| Ordering | idempotency terminates before stateful | terminalPhase | IDEMPOTENCY, not later phase |
| No mutation | before canonical store == after | C-V07 + repaired C3 facts | 90 cases / 180 obs / 180 equal / 0 mutation calls |
| No mutation by disposition | PLAN_READY / REJECTED / ALREADY_APPLIED | manual expected + C3 facts | zero violations in each present group |
| Connector cascade | exact delete/deleteClosure sets | manual logicalPlanProjection / C-V06 | exact two-ID equality for both providers |
| Plan projection | every authored logical projection exact | manual expected | no inferred implementation-only field |
| Geometry threshold | N-1/N/N+1 | Current geometry Authority + P36 R1 | exact boundary facts |
| Checked overflow | add/Dab/Erase -> INTEGER_OVERFLOW | Current checked-arithmetic Authority + P36 R2 proof | all three exact-production seams PASS |
| Overflow lineage | corrected C3 source/materialized relationship | P36 R2 overlay | 906327... -> 2a601... only |
| Restore | no hidden tombstone/history dependency | RST-B12 + manual expected | visible-input proof; no production tombstone API |
| Hierarchy | Root/Group/Sticky/non-parent | Current hierarchy Authority | selected cross-operation anchors conform |
| Sticky cardinality | direct RichText 0..1 | Current hierarchy Authority | placement + insert anchors conform |
| OPEN reconciliation | all current closed groups not stale OPEN | C-V09 | openPolicy=true accepted count 0 |
| Expected truth | AUTHORITY_MANUAL only | C-V01/C-V10 | zero writes / no provider blessing |
| Production scope | semantic implementation unchanged | Git identity | zero delta |
| Determinism | same source -> same four evidence files | C6 generator | byte-for-byte identical |

---

## 17. C6 Exit Criteria

C6 may return from future P32 to CONTROL_REVIEW only when all package-completeness conditions hold:

1. P32 source delta from the P31 package contains exactly the four authorized C6 files and nothing else.
2. Task anchor and package ref are ancestors of the source ref.
3. Accepted C0-C5 truth/code/evidence and P35/P36 evidence remain unchanged.
4. Production semantic source/header, ObjectStore API, schema/proto, and public ABI remain unchanged.
5. Corrected C3 lineage is exactly `906327... -> 2a601...` and is taken from the P36 R2 overlay.
6. C6 never uses stale embedded `c3MaterializedRef=906327...` as current dependency truth.
7. Accepted inventory remains 90 cases / 90 expected / 180 repaired provider observations.
8. Expected provenance remains 90/90 `AUTHORITY_MANUAL`; `expectedTruthWrites=0`; provider output is never expected truth.
9. Accepted `openPolicy=true` remains zero.
10. Equivalent replay anchors conform to `ALREADY_APPLIED / IDEMPOTENCY` for Reference and Indexed.
11. Collision anchor conforms to `REJECTED / IDEMPOTENCY` for Reference and Indexed.
12. All 90 cases / 180 observations prove canonical store before == after; zero observer mutation calls.
13. No-mutation proof is partitioned across every accepted disposition with zero uncovered cases.
14. Every manual expected `logicalPlanProjection` is compared exactly against both providers.
15. `C1-DELETE-CASCADE` exact creates/replacements/deletes/deleteClosure matches the Authority-defined two-object closure.
16. Geometry 1,999,999 / 2,000,000 / 2,000,001 facts match the repaired upstream basis.
17. Checked add/Dab/Erase arithmetic all prove `INTEGER_OVERFLOW` from P36 R2 exact-production seams.
18. `C1-GEOMETRY-OVERFLOW` is explicitly not used as arithmetic-overflow oracle and is not repaired.
19. Restore no-tombstone proof requires no hidden history/tombstone state and no production API/instrumentation change.
20. Root/Group/Sticky/non-parent and Sticky `0..1` direct RichText cardinality cross-operation anchors conform.
21. Fresh C6 OPEN reconciliation records all current closed groups without rewriting C0 evidence.
22. Current repaired C5 `61 PASS / 29 FAIL` is preserved honestly; C6 does not repair or suppress the 29 mismatches.
23. C6 produces exactly the four authorized source-bound evidence files, deterministically.
24. Focused package typecheck/test/build and `git diff --check` pass.
25. Source ref and later evidence-only materialized ref are pushed/reviewer-resolvable.
26. No C7/C8/GT-G1-05 work is produced.

A new C6 blocking semantic mismatch in one of the cross-cutting obligations is evidence for CONTROL_REVIEW; it is not authorization to patch production in C6.

---

## 18. Fail-Closed / Blocked Return Behavior

### Repository divergence

```text
status = BLOCKED_EXECUTION_DIVERGENCE
```

Use when task anchor/package ancestry is invalid, history was rewritten, or an unexpected semantic scope delta appears. Do not reset history automatically.

### Missing/contradictory Authority

```text
status = BLOCKED_AUTHORITY
route = aegis
```

Use only for a real missing/contradictory Current Authority fact. Do not classify an implementation choice as Authority missing.

### Corrected C3/P36 dependency missing or contradictory

```text
status = BLOCKED_UPSTREAM / C6_DEPENDENCY_BASIS
```

Include the missing immutable ref/evidence path. Do not rerun P36 inside C6.

### Stale production observation basis

```text
status = BLOCKED_UPSTREAM / C3_OBSERVATION_BASIS_STALE
```

Use if production semantic source differs from the accepted repaired observation basis. Do not modify the C3 observer inside C6.

### C6 requires a fifth implementation file

```text
status = BLOCKED_SCOPE / C6_P31_REFRESH
```

Do not widen scope opportunistically.

### Cross-cutting semantic mismatch

Materialize truthful evidence and return:

```text
status = READY_FOR_CONTROL_REVIEW_WITH_FINDINGS
```

Include exact case/policy/evidence records. P34 classifies the defect and owns any P35/P36 route. C6 P32 must not fix production to force green.

### Existing 29 C5 mismatches

```text
status impact on C6 package completeness = none by themselves
```

They remain historical/current conformance findings and must be preserved. If one directly violates a C6 blocking invariant, report the exact C6 finding to CONTROL_REVIEW rather than repairing it.

---

## 19. Future P32 Return Contract

A future accepted C6 P32 execution must return at least:

```yaml
stage: P32
task: GT-G1-04-C/C6
package_ref: <immutable C6 P31 package ref>
task_anchor:
  revision: c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
  relation: ancestor
execution_start_ref: <actual reconciled cursor>
source_ref: <immutable four-file C6 implementation ref>
materialized_ref: <later evidence-only ref>
production_semantic_delta: 0
corrected_c3_lineage:
  source_ref: 906327beb9a268c339accd6d3ca6a7038e54ad68
  materialized_ref: 2a601ab35a2294cb38d713ab001bbac7deaa9cf7
evidence:
  - C-IDEMPOTENCY.json
  - C-NO-MUTATION.json
  - C-PLAN-PROJECTION.json
  - C-OPEN-RECONCILIATION.json
focused_tests: <results>
known_preexisting_findings:
  c5_semantic_golden_mismatches: 29
  p36_r2_workspace_validate_c5_scope_guard: preserved / not repaired
C7: NOT_STARTED
C8: NOT_STARTED
GT-G1-05: NOT_STARTED
next_control_step: P34 CONTROL_REVIEW
```

`source_ref` and `materialized_ref` must remain distinct roles even if both are descendants of the package.

---

## 20. P31 Self-Review

### Authority / scope coverage

- C6 objective recovered from Current P20/P30: covered.
- C0-C5/P35/P36 are treated as closed inputs: covered.
- Task anchor = final P36 materialized ref: frozen.
- Corrected C3 lineage overlay precedence: explicit.
- Equivalent replay: exact disposition/phase frozen.
- OperationId collision: exact disposition/phase frozen.
- No-mutation: full 90-case/180-observation aggregation frozen.
- Connector exact logical plan: manual two-object closure frozen.
- Geometry N-1/N/N+1: repaired facts frozen.
- Checked addition/Dab/Erase overflow: P36 R2 exact-production evidence frozen.
- Historical `C1-GEOMETRY-OVERFLOW` anti-confusion rule: explicit.
- Restore no-tombstone: explicit; hidden-state API forbidden.
- Root/Group/Sticky/non-parent + Sticky cardinality: cross-operation anchors explicit.
- OPEN reconciliation: current closed groups explicit; unknown future policy not auto-closed.
- Oracle ownership: `AUTHORITY_MANUAL` only.
- Provider independence: explicit.
- Exact P32 file scope: four create-only files.
- Exact evidence outputs: four files; no C7 Gate artifact.
- Existing 29 C5 mismatches: preserved, not repaired.
- Production semantic/ObjectStore/instrumentation changes: forbidden.
- Atomic Apply and GT-G1-05: forbidden.

### Placeholder scan

No Product-semantic `TBD`, guessed expected value, or unresolved Authority decision is authorized by this package. Future immutable `package_ref`, `source_ref`, `materialized_ref`, and execution cursor are intentionally role placeholders produced by lifecycle materialization; they are not fabricated SHAs.

---

## 21. P31 Stop / Handoff Boundary

Once this docs-only package commit is reviewer-resolvable, C6 P31 is complete.

Expected control return:

```yaml
stage: P31
task: GT-G1-04-C/C6
status: READY_FOR_P32
package_ref: <immutable commit containing this plan>
task_anchor:
  revision: c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
  relation: ancestor
authority:
  verification: notion:3cc4c57a-590c-81ae-ab73-d75501c47169
  implementation_plan: notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b
trusted_dependencies:
  p36_final_materialized: c8fe64b4b2927fb369b6735c6b6a1b45edd5d80d
  corrected_c3_source: 906327beb9a268c339accd6d3ca6a7038e54ad68
  corrected_c3_materialized: 2a601ab35a2294cb38d713ab001bbac7deaa9cf7
scope:
  C0-C5: CLOSED / preserved
  P35: CLOSED
  P36: CLOSED / ACCEPTED_FOR_DOWNSTREAM
  C6: PACKAGED
  C7: NOT_STARTED
  C8: NOT_STARTED
  GT-G1-05: NOT_STARTED
P32: NOT_STARTED
resume_cursor: null
next_control_step: C6 P32 only after accepted non-null package_ref
```

Stop at CONTROL_REASONING. Do not execute P32, do not call Codex, do not run C6 implementation, do not call `aegis-gate-review`, do not start C7/C8, and do not start GT-G1-05 from this P31 occurrence.

# GT-G1-04-C C5 15-Operation Core Corpus Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Materialize the accepted GT-G1-04-C 90-case human-authored corpus as a mechanically complete 15-Operation core conformance slice by proving Current-Authority mandatory-family coverage and running every selected case through the accepted C3 provider observations plus the accepted C4 single-case coordinator, without creating new expected truth, changing production semantics, or absorbing C6-C8.

**Architecture:** C5 is verification-only corpus realization. It treats the accepted C1 `AUTHORITY_MANUAL` authoring root, C2 generated inputs, C3 facts-only observation evidence, and C4 `coordinateCase` contract as read-only trusted dependencies. C5 adds a deterministic coverage contract and batch core-corpus coordinator that binds the existing single `GT-G1-04-C-CORE` suite to the P20 mandatory 15-Operation intent families, then feeds the already-accepted Reference/Indexed observations into C4 one case at a time. C5 may report semantic mismatches for later Gate classification, but it must never repair production code or turn provider behavior into expected truth.

**Tech Stack:** TypeScript 7 / Node.js ESM in the existing `@axiom/semantic-conformance-cli` workspace, Node built-ins only, accepted JSON verification artifacts, existing C4 coordinator.

**Spec:** GT-G1-04-C P20 Verification Design Reconciliation v0.1 (`notion:3cc4c57a-590c-81ae-ab73-d75501c47169`) + GT-G1-04-C P30 Implementation Plan v0.1 (`notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`).

## Global Constraints

- Repository: `Mostorm-Labs/axiom`.
- Branch hint: `codex/gt-g1-04-operation-apply`.
- Stage / task: `GT-G1-04-C / C5 -> P31 Task Package Planning`.
- This P31 package authorizes **no P32 execution by itself**.
- Task anchor:
  ```yaml
  revision: 34584c185d8db84034faeb9c3607b92e495ca8f2
  relation: ancestor
  ```
- `Task Anchor != Execution Cursor`: a later P32 may start from a legal descendant after P33-style reconciliation; it must not mechanically reset to the anchor.
- C0-C4 are closed and accepted inputs. Do not reopen, repair, or reinterpret them in C5.
- Expected truth comes only from accepted `AUTHORITY_MANUAL` records. Reference output, Indexed output, production behavior, generated fixtures, and C3 observations are never expected-truth sources.
- The accepted C1 inventory at the task anchor is exactly 90 cases, exactly 90 expected records, zero `openPolicy=true`, and all expected provenance is `AUTHORITY_MANUAL`.
- C3 already materialized exactly 180 facts-only observations for the 90 accepted cases: Reference + Indexed for every case, with 180/180 no-mutation observations. C5 consumes this accepted evidence; it does not modify or replace the C3 observer/projection.
- C4 `coordinateCase` remains the sole single-case PASS/FAIL owner. C5 may batch-call it but must not change its comparison semantics.
- Provider agreement remains secondary parity and never overrides manual golden truth.
- Every C5-selected case remains subject to the C4 no-mutation check; C5 does not create the later C6 aggregate `C-NO-MUTATION` evidence family.
- C5 does not own C6 cross-cutting idempotency/no-mutation/closed-policy aggregation, C7 final provider-diff/Gate aggregation, C8 CI/evidence orchestration, or GT-G1-05 Atomic Apply.
- No `--bless`, `--update-golden`, `--accept-current-output`, captured-output-to-expected, or equivalent path may exist.
- No new third-party runtime dependency is authorized.
- No production-performance threshold is introduced by C5. The product hot path is read-only and must have zero C5 source delta.
- PR #57 remains OPEN / DRAFT / UNMERGED and is not designated as the C5 P32 transport or C5 Gate object. The branch may remain visible through that historical PR because of Git ref mechanics, but P32/P34 must not cite PR #57 as C5 hosted corroboration by default.

---

## 0. P31 Package Identity / Closed Dependencies

The immutable Git commit containing this file is the C5 P31 `package_ref`. This file does not self-invent that SHA.

Closed control state consumed without re-review:

```text
C0 = CLOSED / ACCEPTED
C1 = CLOSED / ACCEPTED_FOR_DOWNSTREAM
C2 = CLOSED / ACCEPTED_FOR_DOWNSTREAM
C3 = CLOSED / ACCEPTED_FOR_DOWNSTREAM
C4 = CLOSED / ACCEPTED_FOR_DOWNSTREAM
C5 = P31 PACKAGE ONLY
C6 = NOT STARTED
C7 = NOT STARTED
C8 = NOT STARTED
GT-G1-05 = NOT STARTED
```

Accepted C3 refs:

```yaml
package_ref: a46692ac00090d0fa06397c8aa1511704d742734
source_ref: c26c38feb192a7e584fa60a5ffbedf44f4b6e97a
materialized_ref: 855c114f36e4d4d4b9db9faaa28b96ae6d5249c6
```

Accepted C3 evidence consumed read-only:

```text
verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-PLAN-PROJECTION.json
verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-NO-MUTATION.json
```

Accepted C4 refs / basis:

```yaml
original_package_ref: 6f19852e6589af60a37a1ea36b18b321e4a00543
accepted_source_ref: 06b50a781a7b36d6c19f920711ccd416e455aa0b
accepted_materialized_ref: 34584c185d8db84034faeb9c3607b92e495ca8f2
```

Accepted C4 evidence consumed read-only:

```text
verification/evidence/gates/G1/06b50a781a7b36d6c19f920711ccd416e455aa0b/GT-G1-04-C/C-COORDINATOR-CONTRACT.json
```

C4 accepted contract facts that C5 inherits:

```text
C4 synthetic contract = 10/10 PASS
observation-ref boundary = 5/5 PASS
accepted expected records = 90
accepted OPEN records = 0
expected provenance = AUTHORITY_MANUAL
expectedTruthWrites = 0
productionSemanticDependencies = 0
providerOutputUsedAsExpected = false
```

---

## 1. Current Authority / Exact C5 Objective

C5 consumes these Current / accepted Authorities:

1. `notion:3cc4c57a-590c-81ae-ab73-d75501c47169` — Current GT-G1-04-C Verification Authority.
2. `notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b` — Current GT-G1-04-C P30 implementation slice DAG.
3. The Current semantic authorities referenced by each accepted C1 `authorityRuleRefs[]`; C5 does not replace those references with implementation-derived assumptions.
4. Accepted C0 schemas, C1 authoring, C2 generated fixtures, C3 observations/projections, and C4 coordinator contract as repository materialized dependencies.

P30 freezes C5 as:

```text
Slice C5 — 15-Operation Core Corpus
Purpose:
  Materialize mandatory positive and operation-specific negative intent
  families across all 15 V1 Operations.

Implementation strategy:
  Grow/realize the corpus by semantic family while keeping one C suite
  manifest. Expected-authority creation must never be separated from the
  corresponding fixture/probe coverage.

Exit:
  Every Current C mandatory family is represented; all selected cases run
  on both Reference and Indexed provider observations where the current
  production seam supports them.
```

At the C5 task anchor, the authoring/generation work has already been materialized upstream: C1 contains 90 human-reviewed cases/expected records, C2 contains 90 generated inputs, and C3 contains 180 provider observations. Therefore C5 does **not** manufacture additional truth merely to increase case count. Its exact work is to prove that the accepted 90-case core suite mechanically covers the P20 mandatory families and to bind each case to a C4 conformance result.

If that proof exposes a missing Current-Authority mandatory family, C5 must return `BLOCKED_UPSTREAM / C1_CORPUS_AUTHORITY_GAP`; it must not add an ad-hoc expected result inside C5.

---

## 2. Exact Corpus Expansion / Coverage Dimensions

C5 uses the one accepted suite:

```text
suiteId = GT-G1-04-C-CORE
accepted case count = 90
```

Coverage completeness is obligation-driven, not case-count-driven. The 90-case count is an accepted inventory invariant, not a substitute for the mapping below.

### 2.1 InsertObjects

Required Current-Authority families and accepted anchors:

```text
valid staged insert                         -> C1-INSERT-VALID
staged parent                               -> C1-INSERT-STAGED-PARENT
staged Connector target                     -> C1-INSERT-STAGED-CONNECTOR
duplicate/existing ID                       -> C1-INSERT-EXISTING-ID
invalid staged hierarchy/cycle              -> C1-INSERT-HIERARCHY-CYCLE
parent-child capability / Sticky cardinality-> C1-INSERT-STICKY-CARDINALITY
```

The accepted suite also contains `C1-ID-COLLISION`. C5 executes it because it is in the single core suite, but C6 retains ownership of the later cross-cutting idempotency aggregation.

### 2.2 DeleteObjects

```text
valid delete                                -> C1-DELETE-VALID
subtree closure                             -> C1-DELETE-SUBTREE
Connector cascade fixed point               -> C1-DELETE-CASCADE
missing/invalid target                      -> C1-DELETE-MISSING-TARGET
duplicate target structural rejection      -> C1-DELETE-DUPLICATE-TARGET
```

### 2.3 RestoreObjects

```text
current-state eligibility                   -> C1-RESTORE-ELIGIBLE
existing-ID rejection                       -> C1-RESTORE-EXISTING-ID
                                               C1-RESTORE-EXISTING-ID-DIFFERENT
                                               C1-RESTORE-BATCH-EXISTING-ID
staged parent + child                       -> C1-RESTORE-STAGED-PARENT-CHILD
staged target + Connector                   -> C1-RESTORE-STAGED-CONNECTOR
absent required reference                   -> C1-RESTORE-ABSENT-REF
                                               C1-RESTORE-CONNECTOR-TARGET-ABSENT
OperationId-before-existence ordering       -> C1-RESTORE-OPID-BEFORE-EXISTENCE
Local/Replay/Remote semantic parity         -> C1-RESTORE-LOCAL-REPLAY-REMOTE
no tombstone dependency                     -> C1-RESTORE-NO-TOMBSTONE
same payload with new OperationId           -> C1-RESTORE-SAME-PAYLOAD-NEW-OPID
```

The accepted suite also contains `C1-IDEMPOTENT-EQUIVALENT`. C5 executes it; C6 owns the later cross-cutting idempotency evidence family.

### 2.4 SetPlacements

```text
valid move                                  -> C1-PLACEMENT-VALID
full resulting-cycle rejection              -> C1-PLACEMENT-CYCLE
invalid parent                              -> C1-PLACEMENT-INVALID-PARENT
Group parent capability                     -> C1-PLACEMENT-GROUP-ANY
Sticky -> RichText capability               -> C1-PLACEMENT-STICKY-RICHTEXT
other-kind non-parent                       -> C1-PLACEMENT-NONPARENT
Sticky second-RichText cardinality          -> C1-HIERARCHY-STICKY
OrderKey validity                           -> C1-PLACEMENT-ORDERKEY
```

### 2.5 SetTransforms

```text
valid finite canonical transform            -> C1-TRANSFORM-FINITE
-0 normalization                            -> C1-TRANSFORM-NEGATIVE-ZERO
NaN / Infinity rejection                    -> C1-TRANSFORM-NAN-INF
```

### 2.6 PatchProperties

```text
valid Set/Clear                             -> C1-PATCH-VALID
FieldId existence                           -> C1-PATCH-FIELD-ID
value branch/type                           -> C1-PATCH-BRANCH-TYPE
kind applicability                          -> C1-PATCH-APPLICABILITY
presence/default semantics                  -> C1-PATCH-PRESENCE-DEFAULT
duplicate (ObjectId, FieldId) rejection     -> C1-PATCH-DUPLICATE-FIELD
```

### 2.7 SetObjectSize

```text
valid Shape/Image/Sticky size               -> C1-SIZE-VALID
wrong kind                                  -> C1-SIZE-WRONG-KIND
non-finite                                  -> C1-SIZE-NONFINITE
non-positive                                -> C1-SIZE-NONPOSITIVE
hard limit                                  -> C1-SIZE-HARD-LIMIT
```

### 2.8 SetVectorPathGeometry

```text
valid replacement                           -> C1-GEOMETRY-BOUNDARY
wrong kind                                  -> C1-GEOMETRY-WRONG-KIND
structural invariant                        -> C1-GEOMETRY-STRUCTURAL
aggregate N-1                               -> C1-GEOMETRY-N-1
aggregate N                                 -> C1-GEOMETRY-N
aggregate N+1                               -> C1-GEOMETRY-LIMIT
checked arithmetic overflow                 -> C1-GEOMETRY-OVERFLOW
```

### 2.9 SetImageContent

```text
valid Image content                         -> C1-IMAGE-VALID
wrong kind                                  -> C1-IMAGE-WRONG-KIND
ResourceId/content presence                 -> C1-IMAGE-CONTENT-PRESENCE
intrinsic-size validity                     -> C1-IMAGE-INTRINSIC
sourceRect validity                         -> C1-IMAGE-SOURCE-RECT
contentMode validity                        -> C1-IMAGE-CONTENTMODE
local-size/whole-content validity           -> C1-IMAGE-LOCAL-SIZE
runtime resource unavailable is nonsemantic -> C1-IMAGE-RUNTIME-RESOURCE-NONSEMANTIC
```

### 2.10 AddStroke

```text
valid Vector/Dab / new-ID behavior          -> C1-STROKE-VALID
                                               C1-STROKE-NEW-ID
wrong kind/content                          -> C1-STROKE-WRONG-CONTENT
invalid StrokeRecord                        -> C1-STROKE-INVALID-RECORD
existing-ID behavior                        -> C1-STROKE-EXISTING-ID
```

### 2.11 SplitStrokes

```text
valid source -> replacement plan            -> C1-SPLIT-PLAN
source missing                              -> C1-SPLIT-SOURCE-MISSING
replacement structural invalidity           -> C1-SPLIT-REPLACEMENT-STRUCTURAL
replacement ID collision                    -> C1-SPLIT-REPLACEMENT-COLLISION
```

The valid plan case is also the core proof anchor for whole-split pre-apply closure; C5 does not perform Atomic Apply.

### 2.12 AddEraseMasks

```text
valid object-local mask                     -> C1-ERASE-ADD-VALID
mask ID uniqueness                          -> C1-ERASE-ADD-UNIQUENESS
geometry validity                           -> C1-ERASE-ADD-GEOMETRY
target capability                           -> C1-ERASE-ADD-CAPABILITY
duplicate/existing mask rejection           -> C1-ERASE-ADD-EXISTING-MASK
```

### 2.13 RemoveEraseMasks

```text
valid removal                               -> C1-ERASE-REMOVE-VALID
object/mask missing                         -> C1-ERASE-REMOVE-MISSING
duplicate mask IDs                          -> C1-ERASE-REMOVE-DUPLICATE
whole-operation rejection                   -> C1-ERASE-REMOVE-WHOLE-REJECT
```

### 2.14 EditRichText

```text
valid ordered steps                         -> C1-RICHTEXT-VALID
stable refs/positions                       -> C1-RICHTEXT-STABLE-REFS
UTF-8/scalar/style invariants               -> C1-RICHTEXT-UTF8-STYLE
invalid step -> whole-delta rejection       -> C1-RICHTEXT-INVALID-STEP
```

### 2.15 SetConnectorContent

```text
valid Free/Attached endpoints               -> C1-CONNECTOR-VALID
                                               C1-CONNECTOR-ATTACHED-ENDPOINT
target existence / V1 Connectable capability-> C1-CONNECTOR-TARGET-CAPABILITY
anchor presence/applicability               -> C1-CONNECTOR-ANCHOR
routing validity                            -> C1-CONNECTOR-ROUTING
invalid one-end -> whole-content rejection  -> C1-CONNECTOR-INVALID-END
```

### 2.16 Single-suite inventory invariant

C5 must execute **all 90 IDs in `suites/core.json`**, not only the anchors named above. The mandatory mapping proves requirement coverage; suite membership defines the complete C5 execution inventory. No second C5 suite manifest may be created.

---

## 3. Expected-Truth / Oracle Strategy

C5 correctness has four separate inputs with different authority levels:

```text
1. AUTHORITY_MANUAL expected.json
   = primary correctness oracle

2. CaseIntent.authorityRuleRefs[]
   = provenance / semantic obligation link

3. C3 ImplementationObservation
   = implementation fact only

4. C4 coordinateCase(...)
   = single-case comparison owner
```

C5 itself never infers an expected `disposition`, `terminalPhase`, `semanticErrorCategory`, or `logicalPlanProjection` from observations.

Always compared by C4:

```text
disposition
terminalPhase
```

Conditionally compared only when manual expected declares the field:

```text
semanticErrorCategory
logicalPlanProjection
```

Current accepted expected inventory has zero `openPolicy=true`. If a later execution cursor shows any accepted real `openPolicy=true` record or a conflicting Current Authority, C5 must return `BLOCKED_AUTHORITY`; it may not infer `CURRENT_OPEN` or choose a winner.

---

## 4. Provider / C3 Observation Consumption Contract

C5 must not modify or reimplement the C3 observer. It consumes the accepted C3 durable evidence from immutable C3 materialization basis:

```text
C3 source_ref       = c26c38feb192a7e584fa60a5ffbedf44f4b6e97a
C3 materialized_ref = 855c114f36e4d4d4b9db9faaa28b96ae6d5249c6
C3 projection path  = verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/C-PLAN-PROJECTION.json
```

The accepted artifact contains:

```text
factsOnly = true
acceptedCases = 90
observationCount = 180
noMutationObservations = 180
observationRecords = 180
```

C5 must require exactly two observations per suite case:

```text
provider = reference
provider = indexed
```

No duplicate provider per case, no missing provider, no extra accepted case.

Because C5 reuses source-bound C3 observations rather than silently claiming they came from a later source, a future C5 source must prove the production semantic lane has not changed since the accepted C3 source:

```bash
git diff --quiet \
  c26c38feb192a7e584fa60a5ffbedf44f4b6e97a \
  <C5-source_ref> -- \
  runtime/semantic/include/canvas/semantic \
  runtime/semantic/src
```

If the command is not clean, return:

```text
BLOCKED_UPSTREAM / C3_OBSERVATION_BASIS_STALE
```

Do not modify C3 in C5. A fresh observation basis requires the appropriate C3/Gate route.

Stable C4 observation references emitted by C5 use this verification-only form:

```text
g1-04-c://c3/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/<caseId>/reference
g1-04-c://c3/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/<caseId>/indexed
```

They are evidence identifiers only, not Product URIs.

---

## 5. Exact Authorized Future P32 File Scope

P31 itself changes only this plan document. A later C5 P32 authorization may create only:

```text
verification/packages/semantic-conformance-cli/src/core-corpus.ts
verification/packages/semantic-conformance-cli/test/core-corpus.test.mjs
verification/packages/semantic-conformance-cli/test/core-corpus-evidence.test.mjs
verification/tools/generate_g1_04_c5_evidence.mjs
```

No existing source file needs modification. Existing workspace scripts already compile `src/**` and discover `test/*.test.mjs`.

### Read-only in C5

```text
verification/corpus/semantic/v1/g1-04-c/README.md
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/corpus/semantic/v1/g1-04-c/generated/**
verification/schemas/semantic/g1-04-c-*.schema.json
verification/fixture-author/**

runtime/semantic/tools/g1_04_c_*
runtime/semantic/tests/g1_04_c_*
runtime/semantic/include/canvas/semantic/**
runtime/semantic/src/**
schema/axiom/v1/proto/**

verification/packages/semantic-conformance-cli/src/types.ts
verification/packages/semantic-conformance-cli/src/corpus.ts
verification/packages/semantic-conformance-cli/src/provenance.ts
verification/packages/semantic-conformance-cli/src/compare.ts
verification/packages/semantic-conformance-cli/src/coordinator.ts
verification/packages/semantic-conformance-cli/src/main.ts
verification/packages/semantic-conformance-cli/package.json
verification/packages/semantic-conformance-cli/tsconfig.json
verification/package.json
verification/package-lock.json

verification/evidence/gates/G1/c26c38feb192a7e584fa60a5ffbedf44f4b6e97a/GT-G1-04-C/**
verification/evidence/gates/G1/06b50a781a7b36d6c19f920711ccd416e455aa0b/GT-G1-04-C/**
```

### Explicitly forbidden

```text
any authoring/cases.json mutation
any authoring/expected.json mutation
any core.json suite mutation
any generated fixture/provenance mutation
any C3 decoder/runtime/observer/projection mutation
any C4 coordinator/comparison/provenance mutation
any production semantic source/header mutation
any public Product ABI or protobuf/schema mutation
any provider output -> expected generation
any bless/update-golden/accept-current-output path
any C6 aggregate C-IDEMPOTENCY/C-NO-MUTATION/C-OPEN-RECONCILIATION work
any C7 C-PROVIDER-DIFF or final C-GATE aggregation
any C8 workflow / exact-source CI orchestration
any GT-G1-05 Atomic Apply / SemanticGeneration / ChangeSet / CanonicalCommitStamp work
```

If implementation requires a file outside the create-only scope above, stop and return for P31 scope review.

---

## 6. Required C5 Interfaces

Create `src/core-corpus.ts` with verification-only types/functions. Reuse C4 `CaseIntent`, `CExpectedOutcome`, `ImplementationObservation`, `ConformanceResult`, and `coordinateCase`; do not redefine Product semantic models.

Required public surface:

```ts
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

export function validateCoreCoverage(input: {
  cases: readonly CaseIntent[];
  expected: readonly CExpectedOutcome[];
  suite: CoreSuite;
  observations: readonly ImplementationObservation[];
}): CoreCoverageReport;

export function runCoreCorpus(input: {
  cases: readonly CaseIntent[];
  expected: readonly CExpectedOutcome[];
  suite: CoreSuite;
  c3Evidence: C3ProjectionEvidence;
  c3SourceRef: string;
}): CoreCorpusRun;
```

Frozen behavior:

- `validateCoreCoverage` requires exact set equality between suite IDs, case intents, expected outcomes, and observed case IDs for the accepted 90-case inventory.
- It requires all 15 operation families and every Section 2 mandatory-family mapping.
- It rejects duplicate suite IDs, duplicate case records, duplicate expected records, duplicate provider records, missing Reference/Indexed records, extra accepted records, and wrong `operationFamily` mapping.
- It requires all selected expected records to be `AUTHORITY_MANUAL`, `mutationExpected=false`, and current accepted `openPolicy=true` count to remain zero.
- `runCoreCorpus` calls C4 `coordinateCase` exactly once per suite case, in the deterministic `suite.caseIds` order.
- Each call receives the two C3 observations for that case and stable distinct observation refs.
- Since current accepted OPEN count is zero, use `openAuthorityDecision: "UNRESOLVED"`; C4 ignores that decision for closed cases. If a selected expected record has `openPolicy=true`, fail before execution and route upstream instead of guessing.
- C5 does not turn `ConformanceResult.status` counts into a final Gate. Results are evidence for P34/C7. A semantic `FAIL` is preserved, not repaired or suppressed.

---

## 7. Durable C5 Evidence Contract

A future C5 P32 implementation commit must include the generator, but **not** its durable evidence. After freezing the C5 `source_ref`, run the generator and add one evidence-only descendant commit containing:

```text
verification/evidence/gates/G1/<C5-source_ref>/GT-G1-04-C/C-CORE-CORPUS.json
```

Frozen evidence format:

```json
{
  "format": "axiom-gt-g1-04-c-core-corpus-v1",
  "formatVersion": 1,
  "packageRef": "<C5 P31 immutable package_ref>",
  "sourceRef": "<C5 immutable source_ref>",
  "taskAnchor": {
    "revision": "34584c185d8db84034faeb9c3607b92e495ca8f2",
    "relation": "ancestor"
  },
  "authority": {
    "verification": "notion:3cc4c57a-590c-81ae-ab73-d75501c47169",
    "implementationPlan": "notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b"
  },
  "acceptedBasis": {
    "c3SourceRef": "c26c38feb192a7e584fa60a5ffbedf44f4b6e97a",
    "c3MaterializedRef": "855c114f36e4d4d4b9db9faaa28b96ae6d5249c6",
    "c4SourceRef": "06b50a781a7b36d6c19f920711ccd416e455aa0b",
    "c4MaterializedRef": "34584c185d8db84034faeb9c3607b92e495ca8f2"
  },
  "suiteId": "GT-G1-04-C-CORE",
  "selectedCaseCount": 90,
  "selectedExpectedCount": 90,
  "selectedObservationCount": 180,
  "operationFamilyCount": 15,
  "missingMandatoryFamilies": [],
  "wrongFamilyCaseIds": [],
  "unselectedCaseIds": [],
  "acceptedOpenPolicyCount": 0,
  "acceptedExpectedAllAuthorityManual": true,
  "expectedTruthWrites": 0,
  "providerOutputUsedAsExpected": false,
  "productionSemanticDeltaFromC3": 0,
  "results": []
}
```

`results` contains all 90 C4 `ConformanceResult` records in deterministic `core.json` suite order. It may contain `PASS` or `FAIL`; C5 evidence must report reality. C5 does not rewrite failed expected values or production semantics.

The evidence generator must:

1. Take only `--source-ref <sha>` and `--output <path>`.
2. Reject unsafe/unknown args.
3. Read the accepted C1/C2/C3/C4 basis without writing it.
4. Use `git rev-parse <task-anchor>:<path>` to prove C1 `cases.json`, `expected.json`, and `core.json` blobs are unchanged at the C5 source ref.
5. Read the C3 projection artifact from the immutable accepted C3 materialized ref, not from a mutable working-tree substitute.
6. Run the production-semantic no-drift check from C3 source to C5 source.
7. Call `runCoreCorpus` and serialize the exact result deterministically.
8. Never write under `authoring/`, `generated/`, schemas, C3/C4 evidence roots, or production runtime paths.
9. Generate byte-for-byte identical output for the same `source_ref`.

No second C5 evidence file is authorized at P31. C6/C7/C8 own their later evidence families.

---

## 8. Future Source Ref / Materialized Ref Contract

P31 does not invent implementation/evidence SHAs.

```text
source_ref
= future immutable C5 implementation commit whose net delta from this
  package contains only the four authorized C5 source/test/generator files
  and no durable C5 evidence directory

materialized_ref
= later immutable descendant commit adding only
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-CORE-CORPUS.json
  for that exact source_ref
```

Both must be pushed and independently reviewer-resolvable before P34.

The evidence commit must not modify implementation, C1/C2 truth/fixtures, C3/C4 contracts, or production semantics.

If hosted CI corroboration is needed before P34, use a C5-specific transport established after `source_ref`; do not cite or merge historical PR #57 as the C5 Gate object. A remote branch-resolvable immutable commit remains sufficient for source/evidence identity unless a later Gate explicitly requires a PR transport.

---

## 9. Task 1 — Freeze the 15-Operation Coverage Contract

**Files:**
- Create: `verification/packages/semantic-conformance-cli/src/core-corpus.ts`
- Test: `verification/packages/semantic-conformance-cli/test/core-corpus.test.mjs`

**Interfaces:**
- Consumes: accepted C4 `CaseIntent`, `CExpectedOutcome`, `ImplementationObservation` types.
- Produces: `MANDATORY_C5_COVERAGE`, `validateCoreCoverage(...)`, and the interfaces in Section 6.

- [ ] **Step 1: Write RED inventory/coverage tests.**

The test must load the accepted `cases.json`, `expected.json`, `core.json`, and immutable C3 projection evidence and assert failures for each synthetic corruption below:

```text
missing suite case
extra suite case
duplicate suite case
missing expected
duplicate expected
wrong operationFamily for a mapped case
missing Reference observation
missing Indexed observation
duplicate provider observation
unexpected openPolicy=true
non-AUTHORITY_MANUAL expected provenance
missing mandatory family anchor
```

The unmodified accepted inputs must assert:

```text
cases = 90
expected = 90
suite = 90
observations = 180
operation families = 15
missing mandatory families = 0
wrong-family mapped cases = 0
unselected cases = 0
```

- [ ] **Step 2: Run the package test and verify RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

Expected: FAIL because `core-corpus.ts` / coverage functions do not exist yet.

- [ ] **Step 3: Implement the minimal coverage types, frozen Section 2 mapping, set validation, provenance/OPEN validation, and provider-pair validation.**

Do not import file-system or Git concerns into `core-corpus.ts`; keep it pure over parsed values.

- [ ] **Step 4: Re-run package tests and confirm PASS.**

- [ ] **Step 5: Commit.**

```bash
git add verification/packages/semantic-conformance-cli/src/core-corpus.ts \
        verification/packages/semantic-conformance-cli/test/core-corpus.test.mjs
git commit -m "test(g1): freeze C5 core corpus coverage"
```

---

## 10. Task 2 — Batch the Accepted Corpus Through C4 Without Reopening C4

**Files:**
- Modify only the new C5-owned file: `verification/packages/semantic-conformance-cli/src/core-corpus.ts`
- Extend: `verification/packages/semantic-conformance-cli/test/core-corpus.test.mjs`

**Interfaces:**
- Consumes: accepted C4 `coordinateCase(...)` and C3 observation records.
- Produces: `runCoreCorpus(...) -> CoreCorpusRun`.

- [ ] **Step 1: Add RED batch-coordination tests.**

Prove:

```text
accepted 90-case suite -> exactly 90 ConformanceResult records
result order == core.json caseIds order
coordinate input uses exactly one reference + one indexed observation per case
observation refs are distinct and use the frozen g1-04-c://c3/... form
closed expected records use UNRESOLVED only as an ignored OPEN decision
any real openPolicy=true record fails before coordinateCase
manual expected object is unchanged after the run
C3 observation objects are unchanged after the run
one synthetic golden mismatch remains a ConformanceResult FAIL and is not rewritten
one synthetic provider divergence remains a ConformanceResult FAIL and is not hidden
one synthetic no-mutation failure remains a ConformanceResult FAIL and is not hidden
```

- [ ] **Step 2: Run tests and verify RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

- [ ] **Step 3: Implement `runCoreCorpus` as a deterministic loop over `suite.caseIds`.**

For each ID:

```ts
const result = coordinateCase({
  caseIntent,
  expected,
  reference,
  indexed,
  referenceRef,
  indexedRef,
  openAuthorityDecision: "UNRESOLVED",
});
```

Never change `coordinateCase`, `compare.ts`, `provenance.ts`, C3 observation data, or expected data.

- [ ] **Step 4: Re-run tests and confirm PASS.**

- [ ] **Step 5: Commit.**

```bash
git add verification/packages/semantic-conformance-cli/src/core-corpus.ts \
        verification/packages/semantic-conformance-cli/test/core-corpus.test.mjs
git commit -m "feat(g1): run C5 core corpus through coordinator"
```

---

## 11. Task 3 — Deterministic Source-Bound C5 Evidence Generator

**Files:**
- Create: `verification/tools/generate_g1_04_c5_evidence.mjs`
- Test: `verification/packages/semantic-conformance-cli/test/core-corpus-evidence.test.mjs`

**Interfaces:**
- Consumes: built `dist/core-corpus.js`, Git CLI read-only identity queries, accepted immutable C3 evidence, current accepted C1 corpus files.
- Produces: `C-CORE-CORPUS.json` matching Section 7.

- [ ] **Step 1: Add RED generator tests.**

Tests must prove:

```text
same source_ref twice -> byte-for-byte identical JSON
invalid/unknown CLI option -> deterministic failure
missing --source-ref -> failure
missing --output -> failure
source ref with changed C1 cases/expected/core blob -> failure
source ref with production semantic delta from C3 -> failure
accepted C3 evidence factsOnly != true -> failure
accepted C3 observationCount != 180 -> failure
accepted C3 noMutationObservations != 180 -> failure
output has 90 results, 15 operations, zero missing families
output records expectedTruthWrites=0
output records providerOutputUsedAsExpected=false
no output path under authoring/generated/runtime is accepted
```

Use temporary directories/files for output. Do not edit the repository truth roots in a test.

- [ ] **Step 2: Run tests and verify RED.**

```bash
cd verification
npm run test --workspace @axiom/semantic-conformance-cli
```

- [ ] **Step 3: Implement the generator with Node built-ins only.**

Use `node:child_process` `execFileSync("git", ...)` only for read-only `rev-parse`, `show`, and `diff --quiet` checks. Do not shell-concatenate untrusted arguments.

- [ ] **Step 4: Re-run package tests, typecheck, and build.**

```bash
cd verification
npm run typecheck --workspace @axiom/semantic-conformance-cli
npm run test --workspace @axiom/semantic-conformance-cli
npm run build --workspace @axiom/semantic-conformance-cli
```

- [ ] **Step 5: Commit.**

```bash
git add verification/tools/generate_g1_04_c5_evidence.mjs \
        verification/packages/semantic-conformance-cli/test/core-corpus-evidence.test.mjs
git commit -m "test(g1): add C5 core corpus evidence generator"
```

---

## 12. Task 4 — Freeze the Pure C5 Source Ref

**Files:** no new scope.

- [ ] **Step 1: Run focused verification.**

```bash
cd verification
npm run typecheck --workspace @axiom/semantic-conformance-cli
npm run test --workspace @axiom/semantic-conformance-cli
npm run build --workspace @axiom/semantic-conformance-cli
npm run validate
cd ..
git diff --check
```

- [ ] **Step 2: Prove closed inputs are unchanged from the task anchor.**

```bash
git diff --exit-code 34584c185d8db84034faeb9c3607b92e495ca8f2 HEAD -- \
  verification/corpus/semantic/v1/g1-04-c \
  verification/schemas/semantic \
  verification/fixture-author \
  runtime/semantic \
  schema/axiom/v1/proto
```

The only permitted apparent difference under `runtime/semantic` is **none**. The C5 source is TypeScript verification-only.

- [ ] **Step 3: Prove C4 accepted source is unchanged.**

```bash
git diff --exit-code 34584c185d8db84034faeb9c3607b92e495ca8f2 HEAD -- \
  verification/packages/semantic-conformance-cli/src/types.ts \
  verification/packages/semantic-conformance-cli/src/corpus.ts \
  verification/packages/semantic-conformance-cli/src/provenance.ts \
  verification/packages/semantic-conformance-cli/src/compare.ts \
  verification/packages/semantic-conformance-cli/src/coordinator.ts \
  verification/packages/semantic-conformance-cli/src/main.ts
```

- [ ] **Step 4: Record the current immutable commit as C5 `source_ref` and push it.**

The `source_ref` is the final implementation commit after Tasks 1-3. It is not this P31 package ref and not the later evidence commit.

- [ ] **Step 5: Confirm the source ref is reviewer-resolvable remotely before generating durable evidence.**

---

## 13. Task 5 — Materialize Evidence as a Later Descendant Only

**Files:**
- Create only after `source_ref` exists:
  `verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-CORE-CORPUS.json`

- [ ] **Step 1: Generate evidence twice into temporary files.**

```bash
cd verification
node tools/generate_g1_04_c5_evidence.mjs \
  --source-ref <C5-source_ref> \
  --output /tmp/c5-core-a.json
node tools/generate_g1_04_c5_evidence.mjs \
  --source-ref <C5-source_ref> \
  --output /tmp/c5-core-b.json
cmp /tmp/c5-core-a.json /tmp/c5-core-b.json
cd ..
```

Expected: byte-for-byte equality.

- [ ] **Step 2: Copy the deterministic artifact to the exact source-bound evidence path.**

```bash
mkdir -p verification/evidence/gates/G1/<C5-source_ref>/GT-G1-04-C
cp /tmp/c5-core-a.json \
  verification/evidence/gates/G1/<C5-source_ref>/GT-G1-04-C/C-CORE-CORPUS.json
```

- [ ] **Step 3: Verify the evidence-only diff.**

```bash
git diff --check
git status --short
```

Only the exact new evidence path may be present.

- [ ] **Step 4: Commit evidence only.**

```bash
git add verification/evidence/gates/G1/<C5-source_ref>/GT-G1-04-C/C-CORE-CORPUS.json
git commit -m "evidence(g1): materialize C5 core corpus"
```

- [ ] **Step 5: Record the resulting commit as `materialized_ref`, push it, and verify reviewer resolution.**

Do not modify the C5 source after this point. Any source repair creates a new `source_ref` and requires evidence regeneration bound to that new ref.

---

## 14. Test / Oracle Matrix

C5 P32 must produce evidence for all of the following dimensions:

| Dimension | Required check | Owner/oracle | C5 completion condition |
|---|---|---|---|
| Inventory identity | cases/expected/suite all exact 90-ID set | C1/C2 accepted roots | zero missing/extra/duplicate IDs |
| Operation coverage | all 15 V1 families present | P20 mandatory coverage | 15/15 |
| Mandatory semantic families | Section 2 mapping | Current Authority | zero missing/wrong-family anchors |
| Expected provenance | all selected expected `AUTHORITY_MANUAL` | C1 | 90/90 |
| OPEN | real accepted OPEN remains zero | Current Authority | 0; otherwise block Authority |
| Provider presence | Reference + Indexed for every selected case | accepted C3 evidence | 180 records; 2/case |
| No mutation | C4 checks each before/after | C3 facts + C4 coordinator | failures preserved as results; never waived |
| Golden comparison | disposition + terminal always | C1 manual expected + C4 | 90 result records materialized |
| Optional comparison | category/plan only if expected declares | C1 + C4 | no stronger inferred contract |
| Provider parity | C4 per-case diagnostic only | C4 | divergence preserved; C7 owns aggregate Gate |
| Determinism | same source -> same C5 evidence | C5 generator | byte-for-byte equal |
| Truth safety | no expected writes/provider->expected path | C5 scope/provenance | zero |
| Production semantic drift | no delta since C3 observation source | Git source identity | zero; otherwise block C3 basis |

A C5 implementation package can be mechanically complete even if one or more real semantic results are `FAIL`; those failures must be durable evidence for P34 classification. C5 P32 is forbidden to repair production semantics to force green results.

---

## 15. Exit Criteria

C5 may return from P32 to CONTROL_REVIEW only when all package-completeness conditions are true:

1. Future P32 source delta is limited to the four authorized C5 files.
2. Accepted C1 cases/expected and the single core suite are unchanged.
3. Accepted C2 generated fixtures/provenance are unchanged.
4. C3 observer/projection/tools/tests are unchanged.
5. C4 coordinator/comparison/provenance contract files are unchanged.
6. Production semantic source/header and schema/proto files are unchanged.
7. Exact accepted inventory remains 90 cases / 90 expected / 90 suite IDs.
8. Exactly 15 operation families are present.
9. Every Section 2 mandatory Current-Authority family is represented by the frozen accepted case anchor(s).
10. Zero mapped cases belong to the wrong operation family.
11. Zero suite cases are left unselected/unexecuted.
12. Accepted expected provenance is 90/90 `AUTHORITY_MANUAL`.
13. Accepted `openPolicy=true` remains zero; otherwise Authority blocker.
14. Accepted C3 basis supplies exactly 180 observations with Reference + Indexed for every suite case.
15. Accepted C3 basis reports 180/180 no-mutation observations and `factsOnly=true`.
16. Production semantic source is unchanged from C3 `source_ref`, so the accepted C3 observation basis remains valid.
17. C5 produces exactly 90 C4 `ConformanceResult` records in deterministic suite order.
18. Any golden mismatch/provider divergence/no-mutation failure remains visible in the corresponding result and evidence; no result is blessed or suppressed.
19. No C6 aggregate cross-cutting evidence, C7 final Gate/provider-diff aggregate, or C8 workflow is produced.
20. No GT-G1-05 code is touched.
21. Package tests/typecheck/build/workspace validation and `git diff --check` pass.
22. Same `source_ref` generates byte-for-byte identical `C-CORE-CORPUS.json`.
23. New immutable C5 `source_ref` is pushed/reviewer-resolvable.
24. Later evidence-only `materialized_ref` is pushed/reviewer-resolvable and contains the exact source-bound C5 evidence.

Semantic PASS count is **not** a P32 package-completeness precondition. It is a downstream CONTROL_REVIEW/Gate fact. A nonzero semantic FAIL count must route to P34 with evidence rather than trigger unauthorized production repair.

---

## 16. Blocked / Fail-Closed Return Behavior

### Repository divergence before P32

If the P32 execution cursor is neither the task anchor/package lineage nor a legal descendant with compatible scope:

```text
status = BLOCKED_EXECUTION_DIVERGENCE
```

Do not reset or repair history automatically.

### Missing mandatory family in accepted C1 truth root

```text
status = BLOCKED_UPSTREAM / C1_CORPUS_AUTHORITY_GAP
include = operationFamily + missing mandatory family + Current Authority ref
```

Do not author a new expected result in C5.

### Accepted generated input missing/invalid

```text
status = BLOCKED_UPSTREAM / C2_FIXTURE_MATERIALIZATION
include = caseId + generated path + violated C2 contract
```

Do not modify the C2 compiler/generated tree in C5.

### C3 observation missing, malformed, or stale

```text
status = BLOCKED_UPSTREAM / C3_OBSERVATION_BASIS_STALE
include = caseId/provider or production semantic delta
```

Do not edit C3 observer/projection in C5.

### C4 coordinator cannot consume an accepted observation/result shape

```text
status = BLOCKED_UPSTREAM / C4_COORDINATOR_CONTRACT
include = exact caseId + contract mismatch
```

Do not patch C4 inside C5.

### Authority conflict / new real OPEN

```text
status = BLOCKED_AUTHORITY
route = owning Current Authority stage
```

Do not infer `CURRENT_OPEN`, `CURRENT_CLOSED`, or a semantic winner from case names or implementation behavior.

### Real semantic mismatch discovered by C5

Do **not** repair production code. Materialize the C5 source-bound result faithfully and return:

```text
status = READY_FOR_CONTROL_REVIEW_WITH_FINDINGS
include = exact caseIds + C4 diagnostics + evidence path
next = P34 Gate classification
```

P34 decides implementation defect vs test/evidence gap vs Authority issue and owns repair routing.

### Required behavior needs a file outside P31 scope

```text
status = BLOCKED_SCOPE
route = C5 P31 package refresh
```

Do not widen the package opportunistically.

---

## 17. Explicit C6 / C7 / C8 / GT-G1-05 Boundary

C5 may execute existing cases that later participate in cross-cutting evidence, but it does not claim those later closures.

```text
C5 owns:
  15-Operation mandatory-family coverage
  single core-suite execution completeness
  per-case C4 conformance results
  C-CORE-CORPUS.json

C6 owns:
  cross-cutting idempotency aggregation
  independent C-NO-MUTATION aggregation
  closed-policy / OPEN reconciliation aggregation
  C-IDEMPOTENCY and related C6 evidence

C7 owns:
  final Reference/Indexed differential aggregation
  final C Gate aggregation / C-GATE.json

C8 owns:
  exact-source CI workflow and durable orchestration

GT-G1-05 owns:
  Atomic Apply
  SemanticGeneration
  ChangeSet
  CanonicalCommitStamp
  post-apply publication semantics
```

No C5 implementation may cross these ownership lines.

---

## 18. P31 Self-Review

### Spec coverage

- C5 objective recovered from P30: covered.
- All 15 operation families and mandatory P20 intent dimensions: explicitly mapped.
- C0-C4 accepted dependencies: listed and read-only.
- Expected-truth ownership: frozen to `AUTHORITY_MANUAL` only.
- Reference/Indexed obligations: exactly two accepted observations per case.
- C4 coordinator consumption: exact one-call-per-case contract.
- No mutation: retained per case without stealing C6 aggregate ownership.
- Oracle/golden strategy: manual expected primary, provider observations secondary.
- Deterministic evidence generator: exact file and output format frozen.
- Source/materialized ref split: frozen.
- Exit / blocked behavior: frozen.
- C6/C7/C8/GT-G1-05 boundary: explicit.
- Production semantic repair: forbidden.
- PR #57: not designated C5 transport/Gate object.

### Placeholder scan

No implementation-semantic `TBD`, `TODO`, guessed expected value, or unresolved product decision is authorized by this package. Future immutable SHAs are intentionally not fabricated; they are produced by P32/materialization and are identified by role (`source_ref`, `materialized_ref`).

### Type / interface consistency

`CoreSuite`, `C3ProjectionEvidence`, `MandatoryFamilySpec`, `CoreCoverageReport`, `CoreCorpusRun`, `validateCoreCoverage`, and `runCoreCorpus` are defined once in Section 6 and reused consistently by Tasks 1-3.

---

## 19. P31 Stop / Handoff Boundary

Once this docs-only package commit is reviewer-resolvable, C5 P31 is complete.

Expected control return:

```yaml
stage: P31
task: GT-G1-04-C/C5
status: READY_FOR_P32
package_ref: <immutable commit containing this plan>
task_anchor:
  revision: 34584c185d8db84034faeb9c3607b92e495ca8f2
  relation: ancestor
authority:
  verification: notion:3cc4c57a-590c-81ae-ab73-d75501c47169
  implementation_plan: notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b
dependencies:
  C0: CLOSED
  C1: CLOSED
  C2: CLOSED
  C3: CLOSED
  C4: CLOSED / ACCEPTED_FOR_DOWNSTREAM
scope:
  C5: PACKAGED
  C6: NOT_STARTED
  C7: NOT_STARTED
  C8: NOT_STARTED
  GT-G1-05: NOT_STARTED
resume_cursor: null
next_owner: aegis-implementation
next_stage: GT-G1-04-C / C5 P32
```

Stop at CONTROL_REASONING. Do not execute P32, do not call Codex, do not start C6-C8, do not start GT-G1-05, and do not call `aegis-gate-review` from this P31 occurrence.

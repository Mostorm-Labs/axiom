# GT-G1-04-C C7-R2 Provider Differential + Gate Rematerialization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Re-run the C7 provider-differential and aggregate Gate against the accepted P36 repaired 90-case semantic basis, producing source-bound `C-PROVIDER-DIFF.json` and `C-GATE.json` where provider parity remains secondary evidence and the overall C Gate passes only because all seven independent Gate conditions now pass.

**Architecture:** Reuse the existing C7 pure gate aggregator and existing evidence shape. Modify only the old C7 components that are hard-bound to the superseded `61 PASS / 29 FAIL` corpus and old C5/C6 refs. Consume the accepted P36 materialized evidence as the current semantic basis, keep human-reviewed `AUTHORITY_MANUAL` expected truth primary, freshly re-run fixture reproducibility, and materialize exactly two reviewer-resolvable evidence files after freezing a distinct C7-R2 source commit.

**Tech Stack:** TypeScript, Node.js ESM, Node test runner, Python 3 fixture compiler, Git, deterministic JSON evidence.

**Spec:**
- GT-G1-04-C P20 Verification Design Reconciliation v0.1 — `notion:3cc4c57a-590c-81ae-ab73-d75501c47169`
- GT-G1-04-C P20 TerminalPhase Classification Addendum v0.1 — `notion:3cd4c57a-590c-8165-973f-ee31d93f1116`
- GT-G1-04-C P30 Implementation Plan v0.1 — `notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`

## Global Constraints

- Manual golden correctness remains the primary correctness oracle; Reference/Indexed agreement remains secondary differential evidence.
- Expected truth remains exclusively `AUTHORITY_MANUAL`.
- Provider output must never be written to or used as expected truth.
- `expectedTruthWrites = 0` and `providerOutputUsedAsExpected = false` must remain true.
- GT-G1-04-C remains pre-apply: no product mutation, Atomic Apply, SemanticGeneration, ChangeSet, CanonicalCommitStamp, or post-apply publication.
- Production semantic source/header/API/schema/proto must not change.
- Authoring `cases.json` / `expected.json`, suite membership, fixture compiler, generated fixtures, and accepted P36 evidence are read-only in C7-R2.
- Current accepted semantic basis is P36 `90 PASS / 0 FAIL / 0 OBSERVATION_ONLY`, `90/90` provider agreement, `0` provider divergence, and `180/180` no-mutation.
- The historical C7 occurrence bound to `61/29` is superseded evidence only. It must not be consumed as current Gate truth and must not be rewritten.
- C8 remains `NOT_STARTED`; GT-G1-05 remains `NOT_STARTED`.
- No new dependency, package script, lockfile, workspace, runtime, or CI change is authorized.

---

## 0. P31 Package Identity / Control Boundary

Repository:

```text
Mostorm-Labs/axiom
```

Dedicated C7-R2 control branch:

```text
codex/gt-g1-04-c7-provider-gate-r2
```

Stage / occurrence:

```text
GT-G1-04-C / C7-R2 -> P31 Task Package Planning
```

Fresh accepted baseline:

```text
P36 source_ref        = 492d2f914f078a6e4ac8b567e07f7ec813c10107
P36 materialized_ref  = 9b73be589ae070bc602b8989f83d89745a54774e
P36 repair branch     = codex/gt-g1-04-c-p36-golden-repair
P36 branch HEAD       = 9b73be589ae070bc602b8989f83d89745a54774e
C7-R2 branch start    = 9b73be589ae070bc602b8989f83d89745a54774e
relation              = exact at branch creation
```

Task anchor for P32/P33:

```yaml
task_anchor:
  revision: 9b73be589ae070bc602b8989f83d89745a54774e
  relation: ancestor
```

`Task Anchor != Execution Cursor`.

At P31 completion:

```text
resume_cursor = null
P32           = NOT_STARTED
```

The immutable commit containing this plan is the C7-R2 `package_ref`; the control plane returns that SHA externally after this file is committed. The plan intentionally does not self-embed its own commit SHA.

Inherited control state:

```text
C0-C6 = CLOSED / ACCEPTED
P35   = CLOSED (TEST_DEFECT + EVIDENCE_GAP)
P36   = CLOSED / ACCEPTED_FOR_DOWNSTREAM

Historical C7:
  package_ref      = abe3bcb2b86d8f68b8b36d431c5a4e6e92a7593e
  source_ref       = 1c4d9b3ef2118ab7e31f20b418f03b0655c19cd4
  materialized_ref = 666082e114310503b019873fb9e12744c19bcc1e
  gate basis       = 61 PASS / 29 FAIL
  status           = SUPERSEDED BASIS; DO NOT USE AS CURRENT GATE

C7-R2 = P31 PACKAGE ONLY
C8    = NOT_STARTED
GT-G1-05 = NOT_STARTED
```

The accepted P36 head is a valid descendant of the historical C7 materialized ref; this is a new occurrence because upstream semantic evidence changed, not a P33 resume of the historical C7 execution.

---

## 1. Current Authority and C7-R2 Gate Contract

P20/P30 freeze the C7 purpose:

```text
manual golden correctness = primary correctness oracle
Reference/Indexed parity  = secondary differential confidence
```

C7-R2 must evaluate exactly seven blocking conditions independently:

```text
1. authority/provenance validation
2. mandatory corpus selected and valid
3. every blocking observation matches manual expected disposition/category/logical facts
4. zero canonical mutation for all cases
5. zero unexplained Reference/Indexed divergence
6. fixture regeneration reproducible
7. zero stale OPEN mapping
```

Current accepted P36 basis requires the expected C7-R2 state:

```text
1 authority/provenance       PASS
2 mandatory corpus           PASS
3 manual-golden correctness  PASS  (90 PASS / 0 FAIL / 0 OBSERVATION_ONLY)
4 no mutation                PASS  (180/180 unchanged, 0 mutation calls)
5 provider differential      PASS  (90/90 agreement, 0 divergence)
6 fixture reproducibility    PASS  only after C7-R2 freshly re-runs compiler twice
7 stale OPEN reconciliation  PASS  (acceptedOpenPolicyCount = 0)

Overall C Gate               PASS
failedConditions             []
```

This expected PASS is not permission to coerce evidence. If the freshly consumed accepted P36 artifacts do not satisfy these conditions, stop and return the exact blocker. Do not modify expected truth to make the Gate pass.

---

## 2. Trusted Immutable Inputs

### 2.1 Accepted P36 repaired semantic corpus

Reviewer-accessible materialized ref:

```text
9b73be589ae070bc602b8989f83d89745a54774e
```

Source-bound evidence root:

```text
verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C/
```

Consume these current artifacts:

```text
C-CORE-CORPUS.json
C-NO-MUTATION.json
C-PLAN-PROJECTION.json
P36-VERIFICATION-GOLDEN-REPAIR.json
```

Required facts from `C-CORE-CORPUS.json`:

```text
selectedCaseCount                  = 90
selectedExpectedCount              = 90
selectedObservationCount           = 180
operationFamilyCount               = 15
missingMandatoryFamilies           = []
wrongFamilyCaseIds                 = []
unselectedCaseIds                  = []
duplicateSuiteCaseIds              = []
acceptedOpenPolicyCount            = 0
acceptedExpectedAllAuthorityManual = true
expectedTruthWrites                = 0
providerOutputUsedAsExpected       = false
productionSemanticDelta            = 0
PASS                               = 90
FAIL                               = 0
OBSERVATION_ONLY                   = 0
```

Required facts from `C-NO-MUTATION.json`:

```text
sourceRef               = 492d2f914f078a6e4ac8b567e07f7ec813c10107
acceptedCases           = 90
observationCount        = 180
referenceObservations   = 90
indexedObservations     = 90
beforeAfterEqual        = 180/180
unexpectedHarnessErrors = 0
observerMutationCalls   = 0
expectedTruthReads      = 0
semanticCodecCalls      = 0
```

Required trust facts from `P36-VERIFICATION-GOLDEN-REPAIR.json`:

```text
sourceRef                    = 492d2f914f078a6e4ac8b567e07f7ec813c10107
productionSemanticDelta      = 0
authorityManualExpected      = true
expectedTruthWrites          = 0
providerOutputUsedAsExpected = false
providerDivergence           = 0
manualGolden.pass            = 90
manualGolden.fail            = 0
manualGolden.observationOnly = 0
corpus.cases                 = 90
corpus.observations          = 180
corpus.reference             = 90
corpus.indexed               = 90
corpus.noMutation            = 180
corpus.unexpectedHarnessErrors = 0
```

`C-PLAN-PROJECTION.json` remains facts-only implementation observation evidence and may be referenced by C7-R2 output, but C7-R2 must not rewrite or rematerialize it.

### 2.2 Human-reviewed truth and fixture sources

Read-only current roots at the task anchor:

```text
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/corpus/semantic/v1/g1-04-c/generated/**
verification/fixture-author/compile_g1_04_c.py
verification/schemas/semantic/g1-04-c-gate.schema.json
```

The compiler may execute only into temporary directories for C7-R2 reproducibility evidence.

### 2.3 Historical C7 evidence is explicitly non-current

Do not use the old source-bound artifacts under:

```text
verification/evidence/gates/G1/1c4d9b3ef2118ab7e31f20b418f03b0655c19cd4/GT-G1-04-C/
```

Those artifacts truthfully describe the superseded `61/29` state and remain historical evidence. C7-R2 must create a new source-bound evidence root keyed by its own new `source_ref`.

---

## 3. Current Implementation Reality That Must Be Repaired

At task anchor `9b73be589ae070bc602b8989f83d89745a54774e`, the historical C7 implementation is present and largely reusable.

`verification/packages/semantic-conformance-cli/src/gate.ts` is already generic: it accepts exactly seven conditions and returns PASS only when no condition fails. It already supports the new all-PASS state. **Do not modify it.**

`verification/packages/semantic-conformance-cli/test/gate.test.mjs` already proves both all-seven-PASS and independent failure behavior. **Do not modify it.**

The following historical bindings are stale and must be repaired:

1. `src/provider-diff.ts` hard-codes `61 PASS / 29 FAIL / 0 OBSERVATION_ONLY`, hard-codes 29 mismatch IDs by implication, always returns `manualGoldenCorrectness: "FAIL"`, and validates the historical C6 no-mutation envelope.
2. `test/provider-diff.test.mjs` reads historical C5/C6 evidence and asserts the exact 29 mismatches.
3. `test/gate-evidence.test.mjs` hard-codes the historical package ref, expects `goldenPassCount=61`, `goldenFailCount=29`, `manualGoldenCorrectness=FAIL`, `failedConditions=[manual-golden-correctness]`, and `C-GATE.status=FAIL`.
4. `generate_g1_04_c7_evidence.mjs` hard-binds the historical package/task anchor/C5/C6 refs, validates accepted roots relative to the old anchor, requires exactly six create-only C7 source files, and explicitly throws unless the Gate has the single manual-golden failure. Its gate schema helper also hard-codes `status=FAIL`.

C7-R2 is therefore a bounded verification-tool rebinding/rematerialization task, not a redesign of the C7 architecture.

---

## 4. Exact P32 Source Scope

A future C7-R2 P32 may modify **only** these four existing files:

```text
verification/packages/semantic-conformance-cli/src/provider-diff.ts
verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs
verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs
verification/tools/generate_g1_04_c7_evidence.mjs
```

No new source/test file is required or authorized.

The following existing C7 files are explicitly read-only because they already satisfy the new contract:

```text
verification/packages/semantic-conformance-cli/src/gate.ts
verification/packages/semantic-conformance-cli/test/gate.test.mjs
```

If implementation genuinely requires any fifth source/test file, any new dependency, or a modification outside the exact four-file source scope, stop before editing that extra path and return:

```text
BLOCKED_SCOPE / C7_R2_P31_REFRESH
```

---

## 5. Explicit Non-Goals / Forbidden Changes

Do NOT:

- modify `runtime/semantic/src/**` or production semantic headers;
- modify ObjectStore APIs or add production instrumentation;
- modify schema/proto/public ABI;
- modify `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json`;
- modify `verification/corpus/semantic/v1/g1-04-c/authoring/expected.json`;
- modify suite membership;
- modify `verification/fixture-author/compile_g1_04_c.py`;
- modify accepted `generated/**` fixtures or provenance;
- modify any accepted P36 evidence artifact;
- rewrite or delete historical C7 evidence;
- write provider output into expected truth;
- add bless/update-golden/accept-current-output behavior;
- re-run or redesign P35/P36;
- start C8 workflow/CI implementation;
- start GT-G1-05;
- implement Atomic Apply, SemanticGeneration, ChangeSet, CanonicalCommitStamp, or post-apply publication;
- claim a P34 Gate verdict from inside P32 code.

---

## 6. Task 1 — Make Provider Differential Consume the Current P36 Basis

**Files:**
- Modify: `verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs`
- Modify: `verification/packages/semantic-conformance-cli/src/provider-diff.ts`

**Interfaces:**
- Consumes: current P36 `C-CORE-CORPUS.json` and current P36 `C-NO-MUTATION.json`.
- Produces: `summarizeProviderDiff(input)` with the existing `ProviderDiffSummary` shape, but counts derived from the current corpus rather than frozen to the historical 61/29 occurrence.

- [ ] **Step 1: Rewrite the provider-diff test fixture paths to the accepted P36 evidence root.**

The test must read:

```text
verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C/C-CORE-CORPUS.json
verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C/C-NO-MUTATION.json
```

- [ ] **Step 2: Change the primary happy-path assertion to the current accepted truth.**

Required assertions:

```js
assert.equal(summary.status, "PASS");
assert.equal(summary.caseCount, 90);
assert.equal(summary.providerPairCount, 90);
assert.equal(summary.providerAgreement, "90/90");
assert.equal(summary.divergenceCount, 0);
assert.deepEqual(summary.divergenceCaseIds, []);
assert.equal(summary.goldenPassCount, 90);
assert.equal(summary.goldenFailCount, 0);
assert.equal(summary.observationOnlyCount, 0);
assert.equal(summary.manualGoldenCorrectness, "PASS");
assert.deepEqual(summary.goldenMismatchCaseIds, []);
```

- [ ] **Step 3: Preserve fail-closed differential tests.**

The test suite must still prove:

```text
- a synthetic PROVIDER_DIVERGENCE makes provider parity FAIL;
- altered 90/90/180/15 inventory is rejected;
- missing or duplicated provider pair is rejected;
- OPEN truth is rejected;
- non-AUTHORITY_MANUAL provenance is rejected;
- expected-truth writes are rejected;
- provider output used as expected is rejected;
- unexpected diagnostics are rejected;
- mismatch/divergence inventories remain deterministic and sorted;
- invalid no-mutation aggregate is rejected.
```

Because the accepted corpus has zero golden mismatches, construct synthetic golden mismatch inputs inside the test rather than depending on historical 29-case evidence.

- [ ] **Step 4: Run the focused provider-diff test before implementation and confirm it fails against the old hard-coded logic.**

Run after building current TypeScript output in the normal package flow:

```bash
cd verification
npm run build
node --test packages/semantic-conformance-cli/test/provider-diff.test.mjs
```

Expected before implementation: FAIL because historical code requires 61/29 and historical C6 no-mutation format.

- [ ] **Step 5: Generalize `ProviderDiffInput` away from the historical C6-specific name.**

Use:

```ts
export interface ProviderDiffInput {
  coreCorpusEvidence: unknown;
  noMutationEvidence: unknown;
}
```

Update all C7-R2 callers/tests in the authorized four-file scope accordingly.

- [ ] **Step 6: Replace historical `assertC6NoMutation` with a current no-mutation validator.**

Require the accepted P36 no-mutation envelope and facts:

```text
format                  = axiom-gt-g1-04-c-p36-no-mutation-v1
formatVersion           = 1
stage                   = P36
sourceRef               = 492d2f914f078a6e4ac8b567e07f7ec813c10107
acceptedCases           = 90
observationCount        = 180
referenceObservations   = 90
indexedObservations     = 90
beforeAfterEqual        = 180/180
unexpectedHarnessErrors = 0
observerMutationCalls   = 0
expectedTruthReads      = 0
semanticCodecCalls      = 0
```

Do not add assumptions that are absent from this evidence file; provenance/expected-truth trust facts are independently validated from the current core corpus and P36 repair evidence by the generator.

- [ ] **Step 7: Derive golden counts from `resultStatusCounts` and actual result records instead of hard-coding 61/29.**

Required invariants:

```text
PASS + FAIL + OBSERVATION_ONLY = 90
actual per-result counts equal resultStatusCounts
actual result count = 90
all 90 case IDs unique
exactly one Reference ref and one Indexed ref per case
known diagnostics only
FAIL must carry recognized golden-mismatch and/or provider-divergence diagnostics
PASS must carry no diagnostics
```

Return:

```ts
goldenPassCount: pass,
goldenFailCount: fail,
observationOnlyCount: observationOnly,
goldenMismatchCaseIds: mismatches.sort(),
manualGoldenCorrectness: fail === 0 && observationOnly === 0 ? "PASS" : "FAIL",
```

Provider parity remains independent:

```ts
status: divergences.length === 0 ? "PASS" : "FAIL"
```

- [ ] **Step 8: Run focused provider-diff tests and require PASS.**

```bash
cd verification
npm run build
node --test packages/semantic-conformance-cli/test/provider-diff.test.mjs
```

Expected: PASS.

---

## 7. Task 2 — Rebind the C7 Evidence Generator to P36 and the New Package

**Files:**
- Modify: `verification/tools/generate_g1_04_c7_evidence.mjs`

**Interfaces:**
- Consumes: immutable P36 materialized ref plus read-only current authoring/suite/compiler/schema roots.
- Produces: deterministic parsed values for exactly `C-PROVIDER-DIFF.json` and `C-GATE.json`.

- [ ] **Step 1: Replace historical immutable constants with the current C7-R2 lineage.**

Required constant values:

```text
TASK_ANCHOR          = 9b73be589ae070bc602b8989f83d89745a54774e
P36_SOURCE_REF       = 492d2f914f078a6e4ac8b567e07f7ec813c10107
P36_MATERIALIZED_REF = 9b73be589ae070bc602b8989f83d89745a54774e
```

`PACKAGE_REF` must be hard-bound to the immutable C7-R2 P31 package commit returned by the control plane for this plan.

Do not retain the historical C5/C6 source refs as current semantic Gate inputs. Historical refs may appear only in an explicitly labeled historical/superseded metadata field if needed for traceability; they must not supply current Gate conditions.

- [ ] **Step 2: Change source-scope verification to the exact four modified files.**

`PACKAGE_REF -> source_ref` must contain exactly these four `M` entries and no additions/deletions/renames:

```text
verification/packages/semantic-conformance-cli/src/provider-diff.ts
verification/packages/semantic-conformance-cli/test/provider-diff.test.mjs
verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs
verification/tools/generate_g1_04_c7_evidence.mjs
```

If any fifth changed path exists, evidence generation must fail.

- [ ] **Step 3: Rebase accepted-root immutability checks on the P36 task anchor.**

Require no delta from `TASK_ANCHOR` to `source_ref` under:

```text
runtime/semantic
schema/axiom/v1/proto
verification/corpus/semantic/v1/g1-04-c/authoring
verification/corpus/semantic/v1/g1-04-c/generated
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/fixture-author/compile_g1_04_c.py
verification/schemas/semantic/g1-04-c-gate.schema.json
verification/evidence/gates/G1/492d2f914f078a6e4ac8b567e07f7ec813c10107/GT-G1-04-C
```

Do not require the entire `verification/evidence/gates/G1` tree to be unchanged; historical evidence elsewhere is not a C7-R2 source surface, while the accepted P36 evidence root itself must remain byte-identical.

- [ ] **Step 4: Load current P36 artifacts via immutable `git show`.**

Read from `P36_MATERIALIZED_REF`:

```text
C-CORE-CORPUS.json
C-NO-MUTATION.json
C-PLAN-PROJECTION.json
P36-VERIFICATION-GOLDEN-REPAIR.json
```

Validate the exact facts frozen in Section 2 before using them.

- [ ] **Step 5: Invoke the updated provider differential with current evidence.**

Use:

```js
const providerDiff = summarizeProviderDiff({
  coreCorpusEvidence,
  noMutationEvidence,
});
```

Require current result:

```text
providerDiff.status                  = PASS
providerDiff.providerAgreement       = 90/90
providerDiff.divergenceCount         = 0
providerDiff.goldenPassCount         = 90
providerDiff.goldenFailCount         = 0
providerDiff.observationOnlyCount    = 0
providerDiff.goldenMismatchCaseIds   = []
providerDiff.manualGoldenCorrectness = PASS
```

- [ ] **Step 6: Freshly prove fixture reproducibility twice into temporary directories.**

Retain the existing two-run byte-for-byte compiler proof:

```bash
python3 verification/fixture-author/compile_g1_04_c.py --output <temporary-first>
python3 verification/fixture-author/compile_g1_04_c.py --output <temporary-second>
```

Required result:

```text
caseCount            = 90
byteIdentical        = true
authoringRootWrites  = 0
generatedRootWrites  = 0
```

No accepted fixture root may be regenerated in place.

- [ ] **Step 7: Build the seven Gate conditions from current evidence.**

Use the existing pure `evaluateC7Gate` implementation. The current condition statuses must be:

```text
authority-provenance      PASS
mandatory-corpus          PASS
manual-golden-correctness PASS
no-mutation               PASS
provider-differential     PASS
fixture-reproducibility   PASS
open-reconciliation       PASS
```

Require:

```text
gateSummary.status           = PASS
gateSummary.failedConditions = []
```

Remove the historical assertion requiring the single manual-golden failure.

- [ ] **Step 8: Make gate schema validation status-neutral except for matching the computed summary.**

`assertGateSchema` must still enforce the exact schema keys and identity:

```text
format        = axiom-g1-04-c-gate-v1
formatVersion = 1
provenance    = GATE_EVIDENCE
gateId        = GT-G1-04-C
sourceRef     = source_ref
```

But it must require:

```js
gate.status === gateSummary.status
```

and the C7-R2 happy path must produce `PASS`.

- [ ] **Step 9: Emit current trusted-dependency metadata.**

The differential envelope must identify at least:

```text
packageRef
sourceRef
taskAnchor.revision = 9b73be589ae070bc602b8989f83d89745a54774e
P36 source_ref       = 492d2f914f078a6e4ac8b567e07f7ec813c10107
P36 materialized_ref = 9b73be589ae070bc602b8989f83d89745a54774e
authorityManualExpected      = true
expectedTruthWrites          = 0
providerOutputUsedAsExpected = false
productionSemanticDelta      = 0
```

Do not label historical C5/C6 evidence as current trusted semantic input.

- [ ] **Step 10: Keep CLI and output contract unchanged.**

CLI remains exactly:

```text
node verification/tools/generate_g1_04_c7_evidence.mjs --source-ref <sha> --output-dir <directory>
```

Output remains exactly:

```text
C-PROVIDER-DIFF.json
C-GATE.json
```

No third evidence file is authorized.

---

## 8. Task 3 — Update End-to-End C7 Evidence Tests for the Passing Repaired Basis

**Files:**
- Modify: `verification/packages/semantic-conformance-cli/test/gate-evidence.test.mjs`

**Interfaces:**
- Consumes: the C7-R2 generator and the immutable C7-R2 P31 package ref.
- Produces: deterministic source-bound Gate evidence tests for the all-PASS state plus fail-closed CLI/lineage/output-root behavior.

- [ ] **Step 1: Bind `packageRef` to the immutable commit containing this P31 plan.**

The value must exactly equal the `package_ref` returned by the control plane after P31 materialization.

- [ ] **Step 2: Change the happy-path evidence assertions to the current state.**

Required assertions include:

```js
assert.equal(differential.status, "PASS");
assert.equal(differential.providerAgreement, "90/90");
assert.equal(differential.divergenceCount, 0);
assert.equal(differential.goldenPassCount, 90);
assert.equal(differential.goldenFailCount, 0);
assert.equal(differential.observationOnlyCount, 0);
assert.deepEqual(differential.goldenMismatchCaseIds, []);
assert.equal(differential.manualGoldenCorrectness, "PASS");
assert.equal(differential.fixtureReproducibility.byteIdentical, true);
assert.equal(differential.fixtureReproducibility.caseCount, 90);
assert.equal(differential.gateSummary.status, "PASS");
assert.deepEqual(differential.gateSummary.failedConditions, []);
```

`C-GATE.json` must assert `status === "PASS"`.

- [ ] **Step 3: Preserve deterministic evidence generation.**

Run generator twice for the same source ref into two empty temp directories and require byte-for-byte equality for both output files.

- [ ] **Step 4: Preserve fail-closed boundary tests.**

The test must continue to reject:

```text
invalid CLI argument count/order
package ref used as source ref
source ref that violates package/source scope
output under authoring root
output under generated root
output under runtime root
output under accepted P36 evidence root
```

- [ ] **Step 5: Run focused evidence tests and require PASS.**

```bash
cd verification
npm run build
node --test packages/semantic-conformance-cli/test/gate.test.mjs packages/semantic-conformance-cli/test/gate-evidence.test.mjs
```

Expected: PASS.

---

## 9. Task 4 — Full Verification Regression and Source Freeze

**Files:** no additional source files.

- [ ] **Step 1: Run the complete semantic-conformance CLI tests.**

```bash
cd verification
npm test
```

Expected: PASS.

- [ ] **Step 2: Run TypeScript checks.**

```bash
cd verification
npm run typecheck
npm run build
npm run validate
```

Expected: all PASS.

- [ ] **Step 3: Re-run the fixture compiler test suite without changing fixtures.**

```bash
python3 -m unittest verification/tests/test_g1_04_c_fixture_compiler.py
```

Expected: PASS.

- [ ] **Step 4: Run source hygiene.**

```bash
git diff --check
```

Expected: PASS.

- [ ] **Step 5: Prove exact source scope before committing source.**

Relative to the P31 `package_ref`, `git diff --name-status` must list exactly four `M` paths from Section 4 and nothing else.

- [ ] **Step 6: Prove forbidden roots are unchanged from the task anchor.**

There must be zero delta under production semantic source/headers, proto/schema, authoring expected/cases, suite, compiler, generated fixtures, and accepted P36 evidence root.

- [ ] **Step 7: Commit a distinct C7-R2 source commit and record it as `source_ref`.**

The source commit must contain only the exact four modified files.

---

## 10. Task 5 — Materialize Durable C7-R2 Evidence

**Files:**
- Add only after `source_ref` is frozen:
  - `verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-PROVIDER-DIFF.json`
  - `verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-GATE.json`

- [ ] **Step 1: Generate evidence from the exact source commit.**

```bash
node verification/tools/generate_g1_04_c7_evidence.mjs --source-ref <the exact frozen source SHA> --output-dir <an empty temporary directory>
```

- [ ] **Step 2: Verify the generated result before copying it into the source-keyed evidence root.**

Required final facts:

```text
provider differential      PASS
provider agreement         90/90
provider divergence        0
manual golden              90 PASS / 0 FAIL / 0 OBSERVATION_ONLY
manualGoldenCorrectness    PASS
fixture reproducibility    PASS / byte-identical
all seven Gate conditions  PASS
failedConditions           []
C-GATE status              PASS
expected truth writes      0
provider used as expected  false
production semantic delta  0
```

- [ ] **Step 3: Materialize exactly the two generated files under the exact source-keyed evidence directory.**

No other evidence file is authorized in this materialization commit.

- [ ] **Step 4: Commit evidence separately from source.**

The evidence commit must have the C7-R2 `source_ref` as its direct ancestor or otherwise be a valid evidence-only descendant with no source changes.

- [ ] **Step 5: Prove `source_ref -> materialized_ref` is evidence-only and exact.**

Required diff:

```text
A verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-PROVIDER-DIFF.json
A verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-GATE.json
```

No third path.

- [ ] **Step 6: Push the branch and require remote HEAD to equal the evidence materialization commit.**

That remote SHA is the required `materialized_ref` for P34 independent review.

---

## 11. Evidence / Oracle Contract

C7-R2 semantic correctness is established only by the conjunction of independently checked evidence:

```text
AUTHORITY_MANUAL current expected truth
+ accepted P36 90-case corpus
+ accepted P36 180-observation no-mutation proof
+ Reference/Indexed parity derived independently from golden correctness
+ fresh deterministic fixture regeneration
+ exact seven-condition Gate aggregation
+ exact package/source scope proof
+ production semantic delta = 0
+ durable source-bound evidence materialization
```

A passing provider differential alone is never sufficient. A passing aggregate Gate is valid only if all seven recorded condition inputs independently pass.

No provider output may create, rewrite, bless, or update expected truth.

---

## 12. Performance / Environment Constraints

C7-R2 introduces no product-runtime performance requirement because it is verification tooling only.

The evidence generator must remain deterministic and local:

```text
network access          = none required
new npm dependencies    = none
new Python dependencies = none
accepted fixture writes = 0
production execution    = not re-run by C7-R2 generator
```

C7-R2 consumes the already accepted P36 provider observations rather than re-executing Reference/Indexed production providers. Fresh provider execution belongs only to a separately authorized verification task if future evidence requires it.

---

## 13. Exit Criteria

P32 may return `READY_FOR_CONTROL_REVIEW` only when all of the following are true:

```text
1. task anchor ancestry is established from 9b73be589ae070bc602b8989f83d89745a54774e
2. package_ref is the immutable commit containing this P31 plan
3. source_ref is a distinct immutable commit
4. package_ref -> source_ref contains exactly four authorized modified files
5. production semantic delta = 0
6. authoring expected/cases delta = 0
7. fixture compiler delta = 0
8. accepted generated fixture delta = 0
9. accepted P36 evidence delta = 0
10. provider differential = PASS
11. provider agreement = 90/90
12. provider divergence = 0
13. manual golden = 90 PASS / 0 FAIL / 0 OBSERVATION_ONLY
14. manualGoldenCorrectness = PASS
15. no mutation = 180/180 and observerMutationCalls = 0
16. fixture reproducibility = PASS / byte-identical / 90 cases
17. all seven Gate conditions = PASS
18. C-GATE status = PASS
19. expectedTruthWrites = 0
20. providerOutputUsedAsExpected = false
21. semantic CLI tests = PASS
22. TypeScript typecheck/build/validate = PASS
23. fixture compiler tests = PASS
24. git diff --check = PASS
25. exactly two source-bound C7-R2 evidence files are materialized
26. source_ref -> materialized_ref is evidence-only
27. reviewer-accessible remote branch HEAD = materialized_ref
28. C8 remains NOT_STARTED
29. GT-G1-05 remains NOT_STARTED
```

---

## 14. Blocked Return Behavior

Fail closed. Do not widen scope or alter Authority/expected truth to make the Gate pass.

Return one of the following with exact facts:

```text
BLOCKED_EXECUTION_DIVERGENCE
```

when the task anchor/package ancestry cannot be established or observed repository history contradicts this package.

```text
BLOCKED_SCOPE / C7_R2_P31_REFRESH
```

when a fifth source/test file, new dependency, compiler change, fixture change, expected change, production change, schema change, or other unapproved path is required.

```text
BLOCKED_EVIDENCE
```

when the accepted P36 materialized evidence cannot be resolved, its required artifacts are absent, the exact result cannot be durably materialized, or remote reviewer access cannot be established.

```text
BLOCKED_IMPLEMENTATION
```

when the authorized C7-R2 implementation cannot reproduce the accepted P36 facts, provider divergence becomes nonzero, manual golden is not `90/0/0`, no-mutation fails, reproducibility fails, or any Gate condition remains FAIL after the current accepted inputs are consumed without changing expected truth.

If implementation discovers a contradictory or missing Current Authority requirement, stop without repairing it inside P32 and return an ownership blocker for routing to `aegis` with the earliest untrusted layer.

---

## 15. Required P32 Return Contract

The future code-surface return must include:

```yaml
stage: P32
task: GT-G1-04-C/C7-R2
status: READY_FOR_CONTROL_REVIEW

package_ref: <immutable P31 package commit returned by control plane>
task_anchor:
  revision: 9b73be589ae070bc602b8989f83d89745a54774e
  relation: ancestor
execution_start_ref: <actual starting revision recorded before edits>
source_ref: <distinct C7-R2 source commit>
materialized_ref: <durable reviewer-accessible evidence commit>

source_scope:
  modified_files: 4
  forbidden_delta: 0

manual_golden:
  pass: 90
  fail: 0
  observation_only: 0

provider_differential:
  status: PASS
  agreement: 90/90
  divergence: 0

no_mutation:
  before_after_equal: 180/180
  observer_mutation_calls: 0

fixture_reproducibility: PASS
c_gate: PASS
failed_conditions: []
expected_truth_writes: 0
provider_output_used_as_expected: false
production_semantic_delta: 0

evidence:
  - verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-PROVIDER-DIFF.json
  - verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-GATE.json

source_to_materialized_delta: evidence_only
remote_head_equals_materialized_ref: true

C8: NOT_STARTED
GT-G1-05: NOT_STARTED
```

The P32 executor must not issue the P34 verdict. After a durable `materialized_ref` exists, ownership stays with `aegis-implementation` until it returns the P32 result to the control surface; the next Primary Owner after that return is `aegis-gate-review` for independent P34 review.

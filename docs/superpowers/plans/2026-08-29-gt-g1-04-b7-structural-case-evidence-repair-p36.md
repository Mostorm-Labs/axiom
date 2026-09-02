# GT-G1-04-B B7 Structural Preconditions + Case-Resolved Evidence Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for this P36 repair. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Close the final B7 Gate gaps by proving A structural validity for every normal operation-specific fixture before B7 validation and by replacing range-only B-OP15 claims with reviewer-resolvable per-case machine evidence, without changing B7 production.

**Architecture:** Preserve the strong behavioral oracle already present at `9d3ccd1e7d1a9942cf3417db4502926575420f84`. Add structural-precondition checks to the existing test harness/call sites so each normal A->B fixture proves its exact seed records and exact operation payload are structurally admissible before the B7 validator runs. Then create a new source-bound B-OP15 whose normative case evidence is an explicit mapping from semantic case ID to exact GoogleTest name, structural status, expected result/projection, and fresh execution evidence.

**Tech Stack:** C++20, GoogleTest/CTest, `validatePayloadStructure`, ReferenceObjectStore, IndexedObjectStore, existing B7 expected-fragment helpers, JSON Gate evidence.

**Spec:**
- Frozen B7 P31: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-operation-specific-state-p31.md@84a95fbb46ef301833a4ce34a9503451f5d95579`
- First behavioral-oracle repair: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-behavioral-oracle-repair-p36.md@32950127b50da42a31a7d0ca7570321b1f86b2f4`
- Oracle-strengthening repair: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-oracle-strengthening-p36.md@5fa191e0ea9e15044d156e96d7158606028fde02`
- Execution-completeness repair: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-execution-completeness-repair-p36.md@da28f20d22d77e5d29dfa193a3e2c5ae4e68eb53`
- Current reviewed test snapshot: `9d3ccd1e7d1a9942cf3417db4502926575420f84`
- Current reviewed evidence occurrence: `e6a74ceb4640a7bc045c2a36a151404a2681bc8c`
- P34 blocker: `notion:3cb4c57a-590c-8117-ab80-f5060896d0c6`
- P35/P36 authorization: `notion:3cb4c57a-590c-8183-b0e3-df5cb7aeed78`
- Frozen B7 production implementation base: `f98087b29830ea50bc9984be9ef9dfb9aa88e13b`

## Global Constraints

- Primary defect = `TEST_DEFECT`.
- Secondary defect = `EVIDENCE_GAP`.
- `IMPLEMENTATION_DEFECT` is not established.
- Do not modify B7 production source or headers.
- Do not modify B2/B3/B-AUTH-02/B4/B5/B6, ObjectStore/ObjectIndex, schema/protobuf/wire, B8, PreparedApplyPlan, OperationEngine, GT-G1-04-C, or GT-G1-05.
- Preserve the current expected-fragment oracle, non-empty sentinels, Reference/Indexed parity, base no-mutation checks, Indexed rebuild checks, authoritative PLC_B09, RichText full-value tests, thirteen-family CountingStore coverage, and CON_B07 lookup differential.
- Preserve every historical B-OP15 occurrence. Never overwrite evidence under an older SHA.
- No reset/rebase/amend/squash/force-push.

---

## Task 1 — Synchronize and prove the reviewed starting state

**Files:** read-only.

- [ ] Run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git fetch origin
git rev-parse origin/codex/gt-g1-04-operation-apply
```

The expected remote HEAD is the commit containing this plan.

- [ ] If local HEAD is `e6a74ceb4640a7bc045c2a36a151404a2681bc8c`, verify the remote-only advancement is only this docs control-plane commit, then:

```bash
git merge --ff-only origin/codex/gt-g1-04-operation-apply
```

- [ ] Confirm B7 production is unchanged from the frozen base:

```bash
git diff --name-only f98087b29830ea50bc9984be9ef9dfb9aa88e13b -- \
  runtime/semantic/src/operation_specific_validation.cpp \
  runtime/semantic/src/rich_text_state_plan.cpp \
  runtime/semantic/include/canvas/semantic/operation_specific_validation.hpp \
  runtime/semantic/src/rich_text_state_plan.hpp
```

Expected: no output.

- [ ] Confirm current test reality before editing:

```text
expectCreateCase / expectReplaceCase / expectSplitCase
  -> exact expected fragment assertions exist
  -> non-empty rejection sentinel assertions exist
  -> Reference/Indexed + no-mutation + index rebuild exist

StructuralPreconditionsCoverCanonicalFixtures
  -> standalone representative smoke test only

normal expect* call sites
  -> do not yet validate their exact payload through validatePayloadStructure
```

If the repository does not match this state, stop with `BLOCKED_STARTING_STATE_MISMATCH`.

---

## Task 2 — Add reusable exact structural-projection helpers

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Add a canonical Operation wrapper:

```cpp
Operation structuralOperation(OperationPayload payload) {
    Operation operation{};
    operation.id = OperationId{id(7001)};
    operation.document_id = DocumentId{id(7002)};
    operation.schema_version = 1;
    operation.payload_version = 1;
    operation.payload = std::move(payload);
    return operation;
}
```

Use the repository's exact typed-ID construction if the existing constructors require a different spelling; keep IDs non-zero.

Add:

```cpp
template <typename Payload>
void expectPayloadStructurallyValid(const Payload& payload) {
    Operation operation = structuralOperation(OperationPayload{payload});
    ASSERT_TRUE(validatePayloadStructure(operation).ok());
}

void expectRecordsStructurallyValid(const std::vector<ObjectRecord>& records) {
    if (records.empty()) {
        return;
    }
    expectPayloadStructurallyValid(InsertObjectsOp{records});
}
```

The helper must test the actual payload object used by the B7 case, not a representative payload of the same family.

Add an explicit structural mode for the very small number of intentional A-invalid defense cases:

```cpp
enum class StructuralExpectation {
    kCanonical,
    kIntentionalDefenseInvalid,
};
```

Do not silently skip structural validation. Any skipped case must pass `kIntentionalDefenseInvalid` explicitly at the call site and include a short comment naming the defensive seam, e.g. invalid kind/version.

- [ ] Compile the focused B7 target after adding helpers.

---

## Task 3 — Integrate structural proof into all operation-specific case helpers

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Extend each expected-fragment helper to receive the exact operation payload and structural expectation.

Conceptual signatures:

```cpp
template <typename Payload, typename Fn>
void expectCreateCase(
    const std::vector<ObjectRecord>& initial,
    const Payload& payload,
    Fn&& invoke,
    StatefulIssue expected_issue,
    const CreateObjectsStatePlanInputs& expected_success,
    const CreateObjectsStatePlanInputs& non_empty_sentinel,
    StructuralExpectation structural = StructuralExpectation::kCanonical);

template <typename Payload, typename Fn>
void expectReplaceCase(
    const std::vector<ObjectRecord>& initial,
    const Payload& payload,
    Fn&& invoke,
    StatefulIssue expected_issue,
    const ReplaceObjectsStatePlanInputs& expected_success,
    const ReplaceObjectsStatePlanInputs& non_empty_sentinel,
    StructuralExpectation structural = StructuralExpectation::kCanonical);

template <typename Payload, typename Fn>
void expectSplitCase(
    const std::vector<ObjectRecord>& initial,
    const Payload& payload,
    Fn&& invoke,
    StatefulIssue expected_issue,
    const SplitStrokesStatePlanInputs& expected_success,
    const SplitStrokesStatePlanInputs& non_empty_sentinel,
    StructuralExpectation structural = StructuralExpectation::kCanonical);
```

Before seeding/calling production, canonical mode must run:

```cpp
expectRecordsStructurallyValid(initial);
expectPayloadStructurallyValid(payload);
```

Then execute the existing Reference/Indexed oracle unchanged.

For each call site, create the payload once and use that same object for both structural validation and B7 invocation:

```cpp
auto payload = SetTransformsOp{{{id(2), transform}}};
expectReplaceCase(
    initial,
    payload,
    [&](const auto& store, auto* out) {
        return validateSetTransformsState(payload, store, out);
    },
    StatefulIssue::kNone,
    expected,
    rs());
```

Do not reconstruct one payload for A validation and a different payload for B7.

### Canonical coverage requirement

Every normal semantic case in these families must use canonical structural mode:

```text
InsertObjects
SetPlacements
SetTransforms
PatchProperties
SetObjectSize
SetVectorPathGeometry
SetImageContent
AddStroke
SplitStrokes
AddEraseMasks
RemoveEraseMasks
EditRichText
SetConnectorContent
```

This includes positive cases and state-negative cases such as missing target, invalid parent/capability, missing mask, connector state failure, etc. Those failures are B-state failures and their payloads should still be A-structurally valid.

Only a test intentionally targeting an A-invalid defensive seam, such as `INS_B08_BadRecordRejectsWholeOutput`, may use `kIntentionalDefenseInvalid`.

- [ ] Run the focused B7 target.

### Fixture failure rule

If a canonical case fails `validatePayloadStructure`:

1. Treat it first as a fixture defect.
2. Repair only the fixture/payload values until A structural validation passes.
3. Do not modify production.

If the A-valid fixture then exposes a B7 result mismatch against the existing authority-derived expected fragment, preserve the RED and stop with `IMPLEMENTATION_DEFECT_DISCOVERED` as described in Task 6.

---

## Task 4 — Structural-proof the CountingStore performance paths

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

The thirteen CountingStore invocations are normal successful B7 paths and must also prove A structural validity for their exact payloads.

Refactor the current `check(...)` helper so each invocation receives its exact payload and performs:

```text
validate seeded records structurally where non-empty
validate exact operation payload structurally
run validator through CountingStore
assert success
assert all_objects_calls == 0
```

Keep all thirteen family names mechanically visible in the test source.

For `CON_B07_FreePointOnlyNeedsNoEndpointObjectLookup`, prove both exact SetConnectorContent payloads are structurally valid before the two validator calls, then preserve:

```text
free-point all_objects_calls == 0
attached all_objects_calls == 0
attached find_calls > free-point find_calls
```

- [ ] Run the focused B7 target again.

---

## Task 5 — Keep RichText helper truth separate

**File:** `runtime/semantic/tests/rich_text_state_plan_test.cpp`

No semantic redesign is authorized.

The existing `TXT_S01..TXT_S12` full-value oracle should remain unchanged unless a fixture must be adjusted to maintain canonical V1 style/content values after Task 3 integration.

These twelve tests are internal RichText helper tests, not ObjectStore-backed A->B operation cases. Their evidence structural status must be recorded as:

```text
N/A_INTERNAL_HELPER
```

Do not falsely claim `validatePayloadStructure` execution for these internal helper tests.

The separate `EDIT_B01..EDIT_B04` operation-specific integration cases must be structurally proven through the exact `EditRichTextOp` payload in Task 3.

- [ ] Run focused RichText:

```bash
cmake --build build-semantic-b2 --target canvas_semantic_rich_text_state_plan_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_rich_text_state_plan_test --gtest_color=no
```

---

## Task 6 — Run the repaired oracle against unchanged production

Before running, repeat the production freeze diff from Task 1. Expected: no output.

Run focused B7:

```bash
cmake --build build-semantic-b2 --target canvas_semantic_operation_specific_stateful_validation_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_operation_specific_stateful_validation_test --gtest_color=no
```

### Genuine RED hard stop

If any authority-derived B7 expected issue or expected fragment fails after its exact payload has passed A structural validation:

- do not edit production;
- keep the failing test;
- commit/push the test-only snapshot;
- do not create acceptance B-OP15 evidence;
- return:

```text
B7_P36 = IMPLEMENTATION_DEFECT_DISCOVERED
structural-proof test commit = <sha>
failing semantic case IDs = ...
exact GoogleTest names = ...
A structural precondition = PASS
expected issue/fragment = ...
actual issue/fragment = ...
likely owning production function = ...
production source changed = NO
B8 = NOT RELEASED
```

Return to independent P35 before any production repair.

### GREEN path

Only if focused RichText and focused B7 are fully GREEN with unchanged production, continue.

---

## Task 7 — Commit the structural-proof test snapshot and run fresh regression

Commit only authorized test changes:

```bash
git add runtime/semantic/tests/operation_specific_stateful_validation_test.cpp \
        runtime/semantic/tests/rich_text_state_plan_test.cpp

git commit -m "test(g1): bind B7 cases to structural preconditions"
```

It is acceptable for `rich_text_state_plan_test.cpp` to be unchanged if no fixture adjustment was needed; do not create a meaningless edit just to touch the file.

Record:

```text
B7_STRUCTURAL_ORACLE_SHA=<commit>
```

Push it before evidence generation.

Run fresh verification from this exact snapshot:

```bash
ctest --test-dir build-semantic-b2 \
  -R 'StatefulValidationContext|StagedObjectView|ReferenceObjectStore|IndexedObjectStore|ObjectStoreDifferential|HierarchyValidation|HierarchyCapability|ConnectorStatefulValidation|DeleteClosure|RestoreStatefulValidation|OperationSpecific|RichTextState' \
  --output-on-failure

cmake --build build-semantic-b2 -j2
ctest --test-dir build-semantic-b2 --output-on-failure

python3 tools/check_runtime_boundaries.py --root .
python3 tools/check_docs.py
git diff --check
```

Record fresh actual counts. Do not reuse 77/77, 12/12, 89/89, or 305/305 unless they are actually reproduced.

---

## Task 8 — Build case-resolved B-OP15 evidence

Create only:

```text
verification/evidence/gates/G1/<B7_STRUCTURAL_ORACLE_SHA>/GT-G1-04-B/B-OP15.json
```

Preserve every older B-OP15 occurrence.

### Top-level binding

Required:

```text
sourceCommit = testedCommit = B7_STRUCTURAL_ORACLE_SHA
productionImplementationBase = f98087b29830ea50bc9984be9ef9dfb9aa88e13b
packageRef = this plan @ its control-plane SHA
P31 ref
all prior P36 repair refs
current P34 blocker
current P35/P36 authorization
B5 delegated source/evidence
B6 delegated source/corrected evidence
```

### Normative semantic case evidence

Do not use range-only entries such as `INS_B01..B08` as the normative proof.

Add a `semanticCases` array/object containing one explicit record for every claimed P31 operation-specific semantic case.

Each operation-specific record must contain at minimum:

```json
{
  "caseId": "PLC-B09",
  "googleTest": "OperationSpecificStatefulValidation.PLC_B09_ValidFinalStateBatch",
  "structuralStatus": "PASS",
  "expectedIssue": "kNone",
  "expectedProjection": "two exact replacement ObjectRecords: id2 -> Sticky id1; id3 -> Group id4",
  "freshResult": "PASS",
  "referenceIndexedParity": true,
  "baseNoMutation": true,
  "indexedRebuild": true,
  "nonEmptyFailureSentinel": null
}
```

For rejection cases:

```text
nonEmptyFailureSentinel = true
```

For an intentional A-invalid defense case:

```text
structuralStatus = INTENTIONAL_DEFENSE_INVALID
structuralException = exact reason
```

Do not report it as canonical A->B coverage.

### RichText helper evidence

Add one explicit record for each `TXT-S01..TXT-S12`:

```text
caseId
exact GoogleTest name
structuralStatus = N/A_INTERNAL_HELPER
expectedValue summary
freshResult = PASS
```

Do not add Reference/Indexed claims to these internal helper-only tests.

### Support evidence

Record support tests separately from the semantic case matrix, including:

```text
FailureSentinelsAreNonEmpty
structural-precondition support/helper verification
CountingStore_CoversAllThirteenFamiliesWithoutAllObjectsScan
CON_B07 lookup differential
```

### Performance evidence

List all thirteen families individually:

```text
InsertObjects
SetPlacements
SetTransforms
PatchProperties
SetObjectSize
SetVectorPathGeometry
SetImageContent
AddStroke
SplitStrokes
AddEraseMasks
RemoveEraseMasks
EditRichText
SetConnectorContent
```

For each record the direct successful-path observation:

```text
allObjectsCalls = 0
```

Do not use one umbrella boolean without the family list.

### Fresh verification evidence

Record the exact fresh commands/results/counts for:

```text
focused RichText
focused B7
relevant B0-B7 regression
full semantic CTest
runtime-boundary check
docs check
git diff --check
```

---

## Task 9 — Two-step evidence materialization

First commit the new B-OP15:

```bash
git add verification/evidence/gates/G1/<B7_STRUCTURAL_ORACLE_SHA>/GT-G1-04-B/B-OP15.json
git commit -m "evidence(g1): bind B7 structural case evidence"
git push origin codex/gt-g1-04-operation-apply
```

Record:

```text
B7_EVIDENCE_SHA_1=<commit>
```

Then update only the new B-OP15 materialization metadata:

```text
sourceTestSnapshotPushed = true
initialEvidenceCommit = B7_EVIDENCE_SHA_1
initialEvidenceCommitPushed = true
historyRewritten = false
forcePush = false
```

Commit/push:

```bash
git commit -am "evidence(g1): close B7 structural evidence materialization"
git push origin codex/gt-g1-04-operation-apply
```

Record:

```text
B7_EVIDENCE_SHA_2=<commit>
```

Final remote branch HEAD must equal `B7_EVIDENCE_SHA_2`.

---

## Exit Criteria

P36 may return `READY_FOR_INDEPENDENT_P34_REREVIEW` only if all are true:

1. B7 production remains unchanged from `f98087b...`.
2. Every normal operation-specific semantic case structurally validates its exact seed records and exact operation payload before B7 execution.
3. Intentional A-invalid defense cases are explicit exceptions, not counted as canonical coverage.
4. Existing independent expected-fragment, sentinel, Reference/Indexed, no-mutation, Indexed rebuild, PLC_B09, RichText full-value, thirteen-family CountingStore, and CON_B07 evidence remain GREEN.
5. The new B-OP15 has explicit per-case records rather than range-only normative case claims.
6. Every operation-specific semantic case record names its exact GoogleTest and structural status.
7. RichText helper records use `N/A_INTERNAL_HELPER` rather than false A-structural claims.
8. Fresh focused/relevant/full/runtime/docs/diff verification is bound to the new test snapshot.
9. Two-step evidence materialization is complete and reviewer-accessible.
10. B8, PreparedApplyPlan, OperationEngine, GT-G1-04-C, and GT-G1-05 remain untouched.

## Required Final Report

```text
GT-G1-04-B B7 STRUCTURAL-CASE-EVIDENCE P36 RESULT

Starting local HEAD:
Fetched remote HEAD:
Fast-forward:
Package ref:
P34 blocker:
P35/P36 authorization:

Production source changed = NO

Structural helper added:
Canonical operation-specific cases structurally validated:
Intentional defense exceptions:
CountingStore exact payload structural validation:
CON_B07 exact payload structural validation:
Independent expected fragments preserved:
Non-empty sentinels preserved:
Reference/Indexed preserved:
Indexed rebuild preserved:
RichText full-value oracle preserved:

Focused RichText:
Focused B7:
Relevant regression:
Full semantic:
Runtime boundary:
Docs:
git diff --check:

If GREEN:
B7_STRUCTURAL_ORACLE_SHA:
B-OP15 path:
semantic case records:
RichText helper records:
B7_EVIDENCE_SHA_1:
B7_EVIDENCE_SHA_2:
Remote HEAD:

B8 started = NO
PreparedApplyPlan changed = NO
OperationEngine changed = NO
GT-G1-04-C started = NO
GT-G1-05 changed = NO
History rewritten = NO
Force push = NO

Status:
B7 = READY_FOR_INDEPENDENT_P34_REREVIEW
B8 = NOT RELEASED
```

Do not declare B7 PASS or ACCEPTED_FOR_DOWNSTREAM. Only the next independent P34 may release B8.

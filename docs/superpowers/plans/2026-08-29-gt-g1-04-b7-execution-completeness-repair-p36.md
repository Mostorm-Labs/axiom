# GT-G1-04-B B7 Oracle Execution-Completeness Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for this P36 repair. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Actually materialize the oracle-strengthening obligations already frozen in v0.6, without modifying B7 production, and produce reviewer-verifiable evidence that matches the checked-in tests.

**Architecture:** Keep B7 production frozen at `f98087b29830ea50bc9984be9ef9dfb9aa88e13b`. Repair only the two B7 test files. The current `f10cdacc...` snapshot is not accepted as a strong oracle because its diff did not replace the parity-only harness, did not change the RichText test file, and did not complete thirteen-family CountingStore or non-empty-sentinel coverage. The repair must make those changes visible in GitHub before a new B-OP15 can be emitted.

**Tech Stack:** C++20, GoogleTest/CTest, `validatePayloadStructure`, ReferenceObjectStore, IndexedObjectStore, test-only ObjectStore mutator, CountingStore, JSON Gate evidence.

**Spec:**
- Frozen B7 P31: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-operation-specific-state-p31.md@84a95fbb46ef301833a4ce34a9503451f5d95579`
- First behavioral-oracle repair: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-behavioral-oracle-repair-p36.md@32950127b50da42a31a7d0ca7570321b1f86b2f4`
- Oracle-strengthening contract: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-oracle-strengthening-p36.md@5fa191e0ea9e15044d156e96d7158606028fde02`
- Failed materialization snapshot: `f10cdacc7f6de8b050b0fed962b7651142733f8b`
- Failed evidence occurrence: `52dd72614b1dfaec634a401d3d591bbab08f44a7`
- P34 blocker: `notion:3cb4c57a-590c-81a1-947e-f3b4990e7687`
- P35/P36 authorization: `notion:3cb4c57a-590c-81d7-a7f8-daf75c56d67d`

## Global Constraints

- Primary defect = `TEST_DEFECT`; secondary = `EVIDENCE_GAP`.
- `IMPLEMENTATION_DEFECT` is not established.
- Do not modify B7 production source or headers.
- Modify only:
  - `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`
  - `runtime/semantic/tests/rich_text_state_plan_test.cpp`
- Test CMake may change only if a registration defect is independently proven first.
- Do not modify B2/B3/B-AUTH-02/B4/B5/B6, ObjectStore/ObjectIndex, schema/protobuf/wire, B8, PreparedApplyPlan, OperationEngine, GT-G1-04-C, or GT-G1-05.
- All expected issues and expected fragments are test-owned projections from P31/current authority, never copied from a production call.
- Every normal A->B fixture must prove A structural validity before the B7 validator is called. Explicit invalid-kind/version defense tests may be labeled exceptions.
- Every rejection uses a non-empty sentinel and proves exact output preservation.
- Every success/rejection proves Reference/Indexed parity, immutable base projection, and Indexed rebuild consistency.
- Every one of the thirteen B7 families gets direct CountingStore coverage with `allObjectsCalls == 0`.
- Preserve all historical evidence; never overwrite old B-OP15 occurrences.
- No reset/rebase/amend/squash/force-push.

---

## Task 1 — Synchronize and prove the previous repair was incomplete

- [ ] Run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git fetch origin
git rev-parse origin/codex/gt-g1-04-operation-apply
```

Expected remote HEAD is the commit containing this package.

- [ ] If local HEAD is `52dd72614b1dfaec634a401d3d591bbab08f44a7`, inspect the remote-only log and fast-forward with:

```bash
git merge --ff-only origin/codex/gt-g1-04-operation-apply
```

- [ ] Confirm the reviewed incomplete snapshot really has these properties:

```text
f10cdacc... changes operation_specific_stateful_validation_test.cpp only
rich_text_state_plan_test.cpp unchanged from 516ff401...
existing diff(...) still compares production Reference output to production Indexed output on success
CountingStore directly instruments only SetTransforms and SetObjectSize
several SplitStrokes rejection cases still use empty/default sentinels
```

If repository state differs, stop with `BLOCKED_STARTING_STATE_MISMATCH`.

---

## Task 2 — Replace parity-only harness with authority-owned expected-fragment helpers

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Delete/replace the current generic `diff(...)` success behavior.

Add these conceptual interfaces using repository types:

```cpp
template <typename Fn>
void expectCreateCase(
    const std::vector<ObjectRecord>& initial,
    Fn&& invoke,
    StatefulIssue expected_issue,
    const CreateObjectsStatePlanInputs& expected_success,
    const CreateObjectsStatePlanInputs& non_empty_sentinel);

template <typename Fn>
void expectReplaceCase(
    const std::vector<ObjectRecord>& initial,
    Fn&& invoke,
    StatefulIssue expected_issue,
    const ReplaceObjectsStatePlanInputs& expected_success,
    const ReplaceObjectsStatePlanInputs& non_empty_sentinel);

template <typename Fn>
void expectSplitCase(
    const std::vector<ObjectRecord>& initial,
    Fn&& invoke,
    StatefulIssue expected_issue,
    const SplitStrokesStatePlanInputs& expected_success,
    const SplitStrokesStatePlanInputs& non_empty_sentinel);
```

Each helper must independently assert:

```text
Reference issue == expected_issue
Indexed issue == expected_issue
Reference issue == Indexed issue

success:
  Reference output == expected_success
  Indexed output == expected_success
  Reference output == Indexed output

rejection:
  Reference output == non_empty_sentinel
  Indexed output == non_empty_sentinel

Reference base before == after
Indexed base before == after
Indexed indexMatchesRebuild == true
```

Expected success fragments must be constructed in the test from seeded records and payload values. Never call production to manufacture the expected fragment.

Use reusable non-empty sentinels for all three fragment types.

- [ ] Compile after replacing the helper. It is acceptable for existing cases to fail to compile until migrated; do not weaken the helper to preserve old call sites.

---

## Task 3 — Add real A structural-precondition guards

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Keep `validator.hpp` and replace the single standalone Shape smoke test as the only structural proof with reusable helpers:

```cpp
Operation structuralOperation(OperationPayload payload);

template <typename Payload>
void expectPayloadStructurallyValid(const Payload& payload);

void expectRecordsStructurallyValid(const std::vector<ObjectRecord>& records);
```

For every normal B7 test:

```text
validate seed ObjectRecords through InsertObjects structural projection where applicable
validate the operation payload through validatePayloadStructure
only then call the B7 state validator
```

Repair fixtures until guards pass. At minimum ensure structurally valid released-V1 values for:

```text
RichText ObjectRecord / paragraph/style values
VectorStroke
DabStroke
VectorPath geometry
ImageContent positive replacement
EraseMask geometry
Connector content/anchors
```

Do not count the single `AStructuralPreconditionGuardAcceptsCanonicalShape` smoke test as coverage for unrelated families.

Explicit tests that intentionally exercise defensive invalid kind/version state seams must be labeled exceptions and excluded from canonical-path structural coverage.

---

## Task 4 — Migrate all success cases to exact expected projections

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Migrate every positive operation-specific case to the new expected-fragment helpers.

Required exact projections:

```text
InsertObjects:
  creates == exact payload ObjectRecords

SetPlacements:
  replacements == exact seeded records with only Placement changed

SetTransforms:
  replacements == exact seeded records with only Transform changed

PatchProperties:
  exact PropertyBag result
  registry-default SET canonicalizes to absence
  CLEAR removes entry
  multiple fields -> exactly one replacement in sorted FieldId order

SetObjectSize:
  only authorized width/height fields change
  Image preserves resource/intrinsic/sourceRect/contentMode/Transform

SetVectorPathGeometry:
  exact replacement content + unrelated fields preserved

SetImageContent:
  exact complete ImageContent replacement + unrelated fields preserved

AddStroke:
  creates == exact stroke

SplitStrokes:
  source_delete_ids == exact canonical source IDs
  replacement_creates == exact flattened replacements sorted by ObjectId

AddEraseMasks / RemoveEraseMasks:
  exact target replacement and exact mask ordering

EditRichText:
  exact complete replacement ObjectRecord built independently in the test

SetConnectorContent:
  exact replacement record/content
```

A success case that only checks count/shape/parity is not complete.

---

## Task 5 — Make PLC_B09 the authoritative final-state batch case

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Use:

```text
Sticky S = id 1
incoming RichText = id 2, root
outgoing RichText = id 3, parent S
Group G = id 4, root

batch:
  id2 -> S
  id3 -> G
```

Expected final state is valid and expected issue is `kNone`.

Run it through the same Reference/Indexed expected-fragment helper and assert exact two replacements.

Rename/remove the older invalid-final-state test named as PLC_B09 so the case matrix has one unambiguous authoritative `PLC_B09` semantic occurrence. Preserve Git history; ordinary file edits are allowed.

---

## Task 6 — Strengthen TXT_S01..TXT_S12 in the RichText test file

**File:** `runtime/semantic/tests/rich_text_state_plan_test.cpp`

This file MUST have a substantive diff in this repair.

For all twelve cases assert complete expected values, not only `ok()`, counts, or one field.

Required minimum:

```text
TXT_S01 exact A😀XB document/run/style result
TXT_S02 exact surviving scalar sequence + run styles after cross-run deletion
TXT_S03 exact two paragraphs after split-at-zero
TXT_S04 exact two paragraphs after split-at-end
TXT_S05 exact non-empty sentinel unchanged
TXT_S06 exact merged text/runs + first id/style retained
TXT_S07 exact non-empty sentinel unchanged
TXT_S08 styleA != styleB; expect A/styleA, 😀/styleB, B/styleA; include adjacent-equal merge subcase
TXT_S09 complete ParagraphStyle replacement + text/runs unchanged
TXT_S10 step 1 creates paragraph id2; step 2 changes id2; assert exact final two-paragraph document
TXT_S11 exact non-empty sentinel unchanged after second-step failure
TXT_S12 exact one empty paragraph with same id/style and runs=[]
```

Do not call `prepareRichTextDeltaState` to create expected values.

---

## Task 7 — Complete rejection atomicity with non-empty sentinels

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Audit every operation-specific test whose expected issue is not `kNone`.

All must use the typed non-empty sentinel helper.

Specifically replace every SplitStrokes rejection call that currently passes `{}` as sentinel.

Make helper rejection branch assert sentinel non-empty before invoking production, so future empty-sentinel tests fail the test harness itself.

For every rejection prove:

```text
exact expected issue
Reference output unchanged
Indexed output unchanged
Reference base unchanged
Indexed base unchanged
Indexed indexMatchesRebuild
```

---

## Task 8 — Complete direct CountingStore coverage for all thirteen families

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

CountingStore must count:

```cpp
contains_calls
find_calls
all_objects_calls
children_calls
```

Directly invoke at least one structurally valid successful path for each:

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

For every family assert:

```text
all_objects_calls == 0
```

Record per-family observed find/children counts without declaring one universal count.

For `CON_B07_FreePointOnlyNeedsNoEndpointObjectLookup`, use CountingStore for two comparable cases:

```text
FreePoint-only
one AttachedEndpoint to existing connectable target
```

Assert both avoid `allObjects()` and attached performs endpoint-target lookup work beyond the free-point baseline.

The final source must make it mechanically visible that all thirteen family names are instrumented; do not encode only two families and generalize from source inspection.

---

## Task 9 — Run strengthened oracle against unchanged production; STOP on genuine RED

Before focused tests:

```bash
git diff --name-only f98087b29830ea50bc9984be9ef9dfb9aa88e13b -- \
  runtime/semantic/src/operation_specific_validation.cpp \
  runtime/semantic/src/rich_text_state_plan.cpp \
  runtime/semantic/include/canvas/semantic/operation_specific_validation.hpp \
  runtime/semantic/src/rich_text_state_plan.hpp
```

Expected: no output.

Then run both focused targets.

If any authority-derived expected projection fails:

1. Confirm its A structural guard passes.
2. Confirm expected fragment is test-owned from P31/current authority.
3. Preserve the RED test.
4. Commit/push test-only state.
5. Do not modify production.
6. Do not create an acceptance B-OP15.
7. Return:

```text
B7_P36 = IMPLEMENTATION_DEFECT_DISCOVERED
strengthened test commit = <sha>
failing cases = ...
expected = ...
actual = ...
A structural precondition = PASS
likely owning production function = ...
production source changed = NO
B8 = NOT RELEASED
```

---

## Task 10 — GREEN path: commit strong oracle and run fresh regression

Only if both focused targets are GREEN with unchanged production.

Commit exactly the test work:

```text
test(g1): complete B7 oracle strengthening contract
```

Record as `B7_COMPLETE_ORACLE_SHA` and push it before evidence generation.

The commit diff MUST include both test files. Reviewer should be able to see substantive edits to the differential harness, RichText assertions, failure sentinels, structural guards, and CountingStore coverage.

Run fresh:

```bash
ctest --test-dir build-semantic-b2 -R 'StatefulValidationContext|StagedObjectView|ReferenceObjectStore|IndexedObjectStore|ObjectStoreDifferential|HierarchyValidation|HierarchyCapability|ConnectorStatefulValidation|DeleteClosure|RestoreStatefulValidation|OperationSpecific|RichTextState' --output-on-failure
cmake --build build-semantic-b2 -j2
ctest --test-dir build-semantic-b2 --output-on-failure
python3 tools/check_runtime_boundaries.py --root .
python3 tools/check_docs.py
git diff --check
```

Record fresh actual counts only.

---

## Task 11 — Build reviewer-verifiable B-OP15

Create only:

```text
verification/evidence/gates/G1/<B7_COMPLETE_ORACLE_SHA>/GT-G1-04-B/B-OP15.json
```

Preserve all old B-OP15 occurrences.

The artifact must map every claimed semantic case to:

```text
exact GoogleTest name
structural-precondition status or explicit defense exception
expected StatefulIssue
expected fragment/value summary
fresh PASS result
Reference/Indexed parity
base no mutation
Indexed rebuild
non-empty sentinel for rejection
```

Performance evidence must list all thirteen instrumented families individually and their observed `allObjectsCalls` result. `instrumentedFamilies` must contain all thirteen names.

Do not write umbrella booleans such as `independentExpectedFragments=true` unless the artifact also points to the concrete executable helper/case mapping that makes the statement reviewer-checkable.

Include:

```text
sourceCommit = testedCommit = B7_COMPLETE_ORACLE_SHA
productionImplementationBase = f98087b29830ea50bc9984be9ef9dfb9aa88e13b
P31 ref
all three P36 repair refs
current P34 blocker/current P35-P36 authorization
B5/B6 delegated evidence refs
fresh focused/regression/full/runtime/docs/diff results
scope guards
```

Use the two-step materialization sequence:

1. commit/push initial evidence -> `B7_EVIDENCE_SHA_1`
2. update only materialization metadata with `initialEvidenceCommit`, `initialEvidenceCommitPushed=true`, no rewrite/force -> commit/push `B7_EVIDENCE_SHA_2`

Final remote HEAD must equal `B7_EVIDENCE_SHA_2`.

---

## Exit Criteria

B7 may return `READY_FOR_INDEPENDENT_P34_REREVIEW` only if all are true:

1. Strong-oracle commit contains substantive changes to BOTH B7 test files.
2. Parity-only success harness has been replaced by independent expected-fragment assertions.
3. All normal fixtures have A structural-precondition proof, with explicit defensive exceptions only.
4. One authoritative PLC_B09 proves valid complete-batch staging through Reference+Indexed expected-fragment checks.
5. TXT_S01..TXT_S12 are full-value oracles.
6. Every rejection uses a non-empty sentinel and proves exact preservation.
7. CountingStore directly covers all thirteen B7 families.
8. CON_B07 directly distinguishes FreePoint vs Attached lookup work.
9. B7 production remains unchanged from `f98087b...`.
10. Fresh focused/relevant/full/regression/tooling checks pass.
11. New B-OP15 describes only facts visible in the pushed tests and evidence.
12. B8/PreparedApplyPlan/OperationEngine/GT-G1-04-C/GT-G1-05 remain untouched.

## Required Final Report

```text
GT-G1-04-B B7 EXECUTION-COMPLETENESS P36 RESULT

Starting local HEAD:
Fetched remote HEAD:
Fast-forward:
Package ref:
P34 blocker:
P35/P36 authorization:

Production source changed = NO
Behavioral test files changed:

Parity-only helper removed/replaced:
Independent expected-fragment cases:
A structural-precondition coverage:
Authoritative PLC_B09:
TXT_S01..TXT_S12 full-value coverage:
Non-empty failure sentinel coverage:
13-family CountingStore coverage:
CON_B07 FreePoint-vs-Attached observation:

Focused RichText:
Focused B7:
Relevant regression:
Full semantic:
Runtime boundary:
Docs:
git diff --check:

If GREEN:
B7_COMPLETE_ORACLE_SHA:
B-OP15 path:
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

Do not declare B7 PASS yourself.
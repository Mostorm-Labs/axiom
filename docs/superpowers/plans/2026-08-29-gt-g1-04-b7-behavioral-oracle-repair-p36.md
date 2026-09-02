# GT-G1-04-B B7 Behavioral Oracle Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for this P36 repair. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Materialize the missing executable B7 behavioral oracle and truthful source-bound evidence without redesigning B7 or silently repairing production behavior.

**Architecture:** Keep production B7 source at `f98087b29830ea50bc9984be9ef9dfb9aa88e13b` unchanged during this occurrence. Expand the two existing focused test files so every frozen B7 semantic case executes against both ReferenceObjectStore and IndexedObjectStore, with no-mutation/index consistency and lookup observations. If any authority-derived behavioral test fails against current production, preserve/materialize that RED and stop for a new P35 implementation classification; do not edit production source in this repair.

**Tech Stack:** C++20, GoogleTest/CTest, ReferenceObjectStore, IndexedObjectStore, StagedObjectView, existing internal test/bootstrap mutator, JSON Gate evidence.

**Spec:**
- Frozen B7 P31 package: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-operation-specific-state-p31.md@84a95fbb46ef301833a4ce34a9503451f5d95579`
- Blocked P34: `notion:3cb4c57a-590c-8172-89a5-d35143ced4e4`
- P35/P36 authorization: `notion:3cb4c57a-590c-8154-8fbf-cd3d6a8227a8`
- Current production implementation occurrence: `f98087b29830ea50bc9984be9ef9dfb9aa88e13b`
- Historical unsupported evidence occurrence: `a9db80465ee8fac7176970414926525dbd04ce0b`

## Global Constraints

- Primary defect repaired here = `TEST_DEFECT`; secondary = `EVIDENCE_GAP`.
- No implementation defect is pre-declared. Current production is `UNVERIFIED` until this oracle runs.
- Do not modify `runtime/semantic/src/operation_specific_validation.cpp`, `runtime/semantic/src/rich_text_state_plan.cpp`, their headers, B2/B3/B-AUTH-02/B4/B5/B6, ObjectStore/ObjectIndex, schema/protobuf/wire, B8, GT-G1-04-C, or GT-G1-05.
- Modify only the two existing B7 test files. Modify test CMake only if a registration defect is actually discovered; the two targets already exist and normally require no CMake change.
- All expected outcomes come from the frozen B7 P31 package and current authorities, never from current production output.
- Every state-owning B7 case must execute against both ReferenceObjectStore and IndexedObjectStore unless the case is a pure internal RichText helper test with no ObjectStore dependency.
- Every validator failure test must prefill the output object with sentinel data and prove it is unchanged after rejection.
- Every validator success/failure test must prove the canonical base store projection is unchanged; IndexedObjectStore must still satisfy `indexMatchesRebuild`.
- Tests may call `allObjects()` to inspect before/after state. Production validators may not.
- Preserve historical `a9db804.../B-OP15.json`; never overwrite it.
- No amend/rebase/squash/reset/force-push.

---

## Task 1 — Synchronize and prove the defect

**Files:** read-only repository state and existing B7 tests/evidence.

- [ ] Inspect and fetch:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git fetch origin
git rev-parse origin/codex/gt-g1-04-operation-apply
```

Expected remote control-plane HEAD is the commit containing this plan.

- [ ] Verify `a9db80465ee8fac7176970414926525dbd04ce0b` is an ancestor, then fast-forward only.

- [ ] Re-open:

```text
runtime/semantic/tests/operation_specific_stateful_validation_test.cpp
runtime/semantic/tests/rich_text_state_plan_test.cpp
verification/evidence/gates/G1/f98087b29830ea50bc9984be9ef9dfb9aa88e13b/GT-G1-04-B/B-OP15.json
```

Confirm the defect before editing:

```text
operation-specific focused runtime tests = 1 SUCCEED boundary test
RichText focused runtime tests = 1 SUCCEED boundary test
B-OP15 nevertheless claims the full semantic case matrix
```

If this is not true, stop with `BLOCKED_STARTING_STATE_MISMATCH`.

---

## Task 2 — Build the shared differential fixture in the operation-specific test

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Keep the thirteen compile-time signature `static_assert`s.

Add deterministic fixture helpers with these responsibilities:

```cpp
ObjectId id(std::uint64_t);
ObjectRecord shape(...);
ObjectRecord image(...);
ObjectRecord vectorPath(...);
ObjectRecord richText(...);
ObjectRecord vectorStroke(...);
ObjectRecord dabStroke(...);
ObjectRecord connector(...);
ObjectRecord sticky(...);
ObjectRecord group(...);
EraseMaskRecord mask(...);
TextStyle textStyle(...);
ParagraphStyle paragraphStyle(...);
```

Use valid V1 structural values so B7 tests isolate B-state behavior rather than failing A preconditions.

Add a bootstrap helper using the existing internal ObjectStore test mutator to seed the same base projection into ReferenceObjectStore and IndexedObjectStore.

Add one differential harness per output fragment type. Conceptually each harness must:

```text
seed identical Reference + Indexed stores
capture reference_before + indexed_before
prefill both outputs with the same non-empty sentinel
run the same validator/payload against both stores
assert expected StatefulIssue independently
assert Reference issue == Indexed issue
on success: compare complete plan-fragment projection
on failure: assert sentinel output unchanged
assert both base projections unchanged
assert Indexed indexMatchesRebuild
```

Do not generate expected values by running one store and copying its result into the other. Expected issue/fragment comes from the test fixture definition.

For performance observations, add a small test-only `CountingStore` wrapper implementing the ObjectStore interface and counting `contains/find/allObjects/children` while delegating reads. Never alter production stores.

---

## Task 3 — Materialize InsertObjects + SetPlacements cases

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Create these exact GoogleTest cases; each uses the differential harness unless explicitly performance-only:

```text
INS_B01_GroupAndShapeStagedTogetherSucceeds
INS_B02_ExistingCandidateIdentityRejects
INS_B03_ConnectorAndStagedTargetSucceeds
INS_B04_ConnectorMissingAttachedTargetRejects
INS_B05_StickyShapeChildRejects
INS_B06_StickyTwoRichTextChildrenRejects
INS_B07_EmptyStickySucceeds
INS_B08_BadRecordRejectsWholeOutput

PLC_B01_MoveShapeUnderGroupSucceeds
PLC_B02_MissingTargetRejects
PLC_B03_MissingParentRejectsFromB3
PLC_B04_BatchCycleRejectsFromB3
PLC_B05_MoveRichTextIntoEmptyStickySucceeds
PLC_B06_SecondRichTextIntoOccupiedStickyRejects
PLC_B07_MoveShapeIntoStickyRejects
PLC_B08_MoveSoleRichTextOutLeavesEmptySticky
PLC_B09_MultiItemFinalStateValidatedAfterAllStaging
```

Expected issues exactly follow P31:

```text
INS-B02 -> kObjectAlreadyExists
INS-B04 -> kInvalidReference
INS-B05/B06 -> kInvalidApplicability
PLC-B02 -> kObjectMissing
PLC-B03 -> kInvalidReference
PLC-B04 -> kHierarchyCycle
PLC-B06/B07 -> kInvalidApplicability
all listed positive cases -> kNone
```

`PLC_B09` must use a batch whose intermediate item-by-item state would be invalid or incomplete but whose fully staged final state is valid, proving validation occurs after complete staging.

---

## Task 4 — Materialize deterministic replacement-family cases

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Create:

### SetTransforms

```text
TRN_B01_ValidTargetProducesReplacement
TRN_B02_MissingTargetRejects
TRN_B03_LateMissingTargetLeavesWholeOutputUnchanged
```

`TRN_B03` must contain at least two canonical items with the first valid and a later target missing; sentinel output must remain unchanged.

### PatchProperties

```text
PROP_B01_SetApplicableFieldProducesReplacement
PROP_B02_WrongKindApplicabilityRejects
PROP_B03_ClearRemovesExplicitEntry
PROP_B04_SetRegistryDefaultCanonicalizesToAbsence
PROP_B05_TwoFieldsOneObjectProduceOneReplacement
PROP_B06_MissingTargetRejects
PROP_B07_LateFailureLeavesOutputUnchanged
```

`PROP_B04` must cover at least one scalar default and one tagged-value default across subcases. `PROP_B05` asserts FieldId-sorted entries and exactly one replacement for the target.

### SetObjectSize

```text
SIZE_B01_ShapeSucceeds
SIZE_B02_ImageSucceedsAndPreservesNonSizeContent
SIZE_B03_StickySucceeds
SIZE_B04_GroupOrStrokeRejects
SIZE_B05_MissingTargetRejects
```

For Image, assert resource ID, intrinsic dimensions, source rect, content mode and Transform are unchanged.

### SetVectorPathGeometry

```text
PATH_B01_VectorPathSucceeds
PATH_B02_WrongKindRejects
PATH_B03_MissingTargetRejects
```

### SetImageContent

```text
IMG_B01_CompleteImageContentReplacementSucceeds
IMG_B02_WrongKindRejects
IMG_B03_MissingTargetRejects
```

The positive Image test must succeed without any resource/blob materialization fixture.

---

## Task 5 — Materialize Stroke + EraseMask cases

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

### AddStroke

```text
STR_B01_VectorStrokeCreateSucceeds
STR_B02_DabStrokeCreateSucceeds
STR_B03_ExistingIdRejects
STR_B04_NonStrokeRecordRejects
STR_B05_StrokeUnderGroupSucceeds
STR_B06_StrokeUnderStickyRejects
```

### SplitStrokes

```text
SPL_B01_SourceToTwoAbsentReplacementsSucceeds
SPL_B02_MissingSourceRejects
SPL_B03_NonStrokeSourceRejects
SPL_B04_ReplacementCollidesWithUnrelatedExistingId
SPL_B05_ReplacementReusesSourceIdRejects
SPL_B06_NonStrokeReplacementRejects
SPL_B07_InvalidReplacementParentOrCapabilityRejects
SPL_B08_OneFailingSplitLeavesAllFragmentsUnchanged
```

Positive Split assertion:

```text
source_delete_ids = canonical source IDs only
replacement_creates = flattened replacements sorted by ObjectId
no B5 hierarchy/connector cascade partitions exist in the B7 output type
```

### AddEraseMasks

```text
MASK_ADD_B01_VectorStrokeSucceeds
MASK_ADD_B02_DabStrokeSucceeds
MASK_ADD_B03_WrongKindRejects
MASK_ADD_B04_MissingTargetRejects
MASK_ADD_B05_ExistingMaskIdRejects
MASK_ADD_B06_LateFailureLeavesWholeOutputUnchanged
```

### RemoveEraseMasks

```text
MASK_REM_B01_AllRequestedMasksExistSucceeds
MASK_REM_B02_OneMissingMaskRejectsWholeOperation
MASK_REM_B03_WrongKindRejects
MASK_REM_B04_MissingTargetRejects
MASK_REM_B05_LateFailureLeavesWholeOutputUnchanged
```

No test may assume a document-global EraseMaskId index.

---

## Task 6 — Materialize the RichText staged oracle

**File:** `runtime/semantic/tests/rich_text_state_plan_test.cpp`

Replace the `BoundaryCompiles`-only file with executable tests for the exact internal interface:

```cpp
StatefulResult prepareRichTextDeltaState(
    const RichTextContent& current,
    const RichTextDelta& delta,
    RichTextContent* out);
```

Build valid V1 ParagraphStyle/TextStyle fixtures with explicit non-zero FontResourceId.

Create exactly these named cases:

```text
TXT_S01_UnicodeScalarInsertInAEmojiB
TXT_S02_DeleteAcrossRunBoundaryNormalizes
TXT_S03_SplitAtZero
TXT_S04_SplitAtEnd
TXT_S05_SplitParagraphIdCollisionRejects
TXT_S06_MergeAdjacentKeepsFirstIdAndStyle
TXT_S07_MergeNonAdjacentRejects
TXT_S08_SetInlineStyleSplitsAndMergesDeterministically
TXT_S09_SetParagraphStyleReplacesCompleteStyle
TXT_S10_SecondStepResolvesFirstStepStagedState
TXT_S11_SecondStepInvalidLeavesWholeOutputUnchanged
TXT_S12_EmptyParagraphCanonicalRunsRemainEmpty
```

`TXT_S01` must use text `A😀B` and prove canonical scalar positions 0..3 rather than byte/UTF-16 positions.

`TXT_S10` must make step 2 reference a paragraph identity created by step 1.

`TXT_S11` must prefill `out` with a sentinel RichTextContent, make step 1 valid and step 2 invalid, and assert the sentinel remains byte/value-equal.

Add EditRichText integration cases in `operation_specific_stateful_validation_test.cpp`:

```text
EDIT_B01_UnicodeScalarDeltaProducesReplacement
EDIT_B02_MissingObjectRejects
EDIT_B03_WrongKindRejects
EDIT_B04_InvalidStagedDeltaLeavesOutputUnchanged
```

Run Edit cases through Reference/Indexed differential harness.

---

## Task 7 — Materialize SetConnectorContent cases

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Create:

```text
CON_B01_ValidConnectorExistingConnectableTargetSucceeds
CON_B02_NonConnectorTargetObjectRejects
CON_B03_MissingAttachedTargetRejects
CON_B04_NonConnectableAttachedTargetRejects
CON_B05_ValidStablePortForShapeImageStickySucceeds
CON_B06_InvalidStablePortForActualTargetRejects
CON_B07_FreePointOnlyNeedsNoEndpointObjectLookup
```

Use B4 results directly; do not restate its connectability registry in production.

`CON_B07` must use CountingStore and distinguish the target Connector lookup from endpoint target lookups.

---

## Task 8 — Performance and no-full-scan assertions

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Using CountingStore, add a table-driven test covering at least one valid path for every family:

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

For every production-path invocation assert:

```text
allObjectsCalls == 0
```

Also record operation-specific observations required by P31:

```text
single-target replacement families -> bounded find lookups
PatchProperties -> one logical base target resolution per distinct ObjectId
SplitStrokes -> no DeleteClosure/full-store scan
mask operations -> per-target nested mask work only
Connector -> target Connector + required endpoint lookups only
```

Do not freeze one universal `findCalls == N` across all operations.

---

## Task 9 — Run the oracle against current production before any production edit

Build focused targets:

```bash
cmake --build build-semantic-b2 --target canvas_semantic_rich_text_state_plan_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_rich_text_state_plan_test --gtest_color=no

cmake --build build-semantic-b2 --target canvas_semantic_operation_specific_stateful_validation_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_operation_specific_stateful_validation_test --gtest_color=no
```

### If ANY required behavioral test fails

Do not modify production source.

1. Confirm failure is not a test-fixture/A-precondition mistake.
2. Keep the authority-derived failing test.
3. Commit the expanded test oracle as an ordinary test-only commit:

```text
test(g1): materialize B7 behavioral oracle
```

4. Push normally so the RED is reviewer-accessible.
5. Return:

```text
B7_P36 = IMPLEMENTATION_DEFECT_DISCOVERED
failing test commit = <sha>
case IDs = ...
expected StatefulIssue/fragment = ...
actual StatefulIssue/fragment = ...
likely owning production function = ...
production source modified = NO
B8 = NOT RELEASED
```

Do not create a replacement B-OP15 acceptance artifact in this RED branch occurrence. The next P35 will classify the concrete implementation failures.

### If ALL required behavioral tests pass

Continue to Task 10.

---

## Task 10 — Fresh regression verification on the test-expanded snapshot

Commit the expanded GREEN tests first:

```text
test(g1): materialize B7 behavioral oracle
```

Record exact SHA as `B7_REVALIDATED_SHA` and push it.

Then run fresh verification from that exact snapshot:

```bash
ctest --test-dir build-semantic-b2 -R 'StatefulValidationContext|StagedObjectView|ReferenceObjectStore|IndexedObjectStore|ObjectStoreDifferential|HierarchyValidation|HierarchyCapability|ConnectorStatefulValidation|DeleteClosure|RestoreStatefulValidation|OperationSpecific|RichTextState' --output-on-failure

cmake --build build-semantic-b2 -j2
ctest --test-dir build-semantic-b2 --output-on-failure

python3 tools/check_runtime_boundaries.py --root .
python3 tools/check_docs.py
git diff --check
```

Record actual test counts; do not copy the old `218/218` count.

---

## Task 11 — Rebuild truthful B-OP15 evidence

Create a NEW artifact only after all required behavioral tests and regressions pass:

```text
verification/evidence/gates/G1/<B7_REVALIDATED_SHA>/GT-G1-04-B/B-OP15.json
```

Required top-level binding:

```text
sourceCommit = testedCommit = B7_REVALIDATED_SHA
productionImplementationBase = f98087b29830ea50bc9984be9ef9dfb9aa88e13b
packageRef = docs/superpowers/plans/2026-08-29-gt-g1-04-b7-operation-specific-state-p31.md@84a95fbb46ef301833a4ce34a9503451f5d95579
repairRef = docs/superpowers/plans/2026-08-29-gt-g1-04-b7-behavioral-oracle-repair-p36.md@<CONTROL_PLANE_SHA>
P34 blocked review = notion:3cb4c57a-590c-8172-89a5-d35143ced4e4
P35/P36 authorization = notion:3cb4c57a-590c-8154-8fbf-cd3d6a8227a8
```

For every semantic case claimed in `caseMatrix`, include the exact GoogleTest name and its fresh PASS observation. Do not list a case that has no executable test.

Required evidence sections:

```text
implementedHere = 13 B7 families
DeleteObjects delegated to B5 source/evidence
RestoreObjects delegated to B6 source/corrected evidence

caseMatrix with exact executable test names
TXT-S01..TXT-S12 executable mapping
Reference/Indexed differential mapping
no-mutation/sentinel-output observations
Indexed indexMatchesRebuild observations
performance/CountingStore observations
focused test commands/results/counts
relevant B0-B7 regression command/result/count
full semantic command/result/count
runtime boundary result
docs result
git diff --check
scope guards
```

Correct materialization metadata truthfully:

```text
source/test snapshot pushed = true
evidence commit pushed = true   // only after evidence commit exists remotely
history rewritten = false
force push = false
```

Commit evidence separately:

```text
evidence(g1): rebind B7 to behavioral oracle
```

Push normally.

---

## Exit Criteria

P36 may return `READY_FOR_INDEPENDENT_P34_REREVIEW` only if:

1. The two focused targets contain actual behavioral assertions, not only `SUCCEED()` boundary tests.
2. All P31 required B7 semantic cases are executable and green.
3. TXT-S01..TXT-S12 are executable and green.
4. All thirteen operation families have Reference/Indexed differential evidence.
5. Rejection tests prove sentinel output unchanged.
6. Base stores remain unchanged and Indexed indexes match rebuild.
7. Production-path `allObjectsCalls == 0` is executable evidence.
8. The original production B7 files remain byte/commit-unchanged during this P36 occurrence.
9. Fresh relevant and full regression suites pass.
10. The new B-OP15 maps every claimed case to an executable test and has accurate materialization metadata.
11. B8/PreparedApplyPlan/OperationEngine remain untouched.

If any behavioral test reveals a production mismatch, exit through the RED path instead; do not self-authorize a production fix.

## Required Final Report

```text
GT-G1-04-B B7 BEHAVIORAL-ORACLE P36 RESULT

Starting local HEAD:
Fetched remote HEAD:
Fast-forward:
Control-plane package ref:
Frozen B7 P31 ref:
P34 blocker:
P35/P36 authorization:

Behavioral test files changed:
Production source changed = NO

Focused RichText cases/count:
Focused B7 cases/count:
13-family Reference/Indexed differential:
No-mutation/sentinel-output:
Indexed rebuild:
Performance/allObjects=0:

If RED:
  test-only commit SHA:
  failing case IDs:
  expected:
  actual:
  owning production function:
  status = IMPLEMENTATION_DEFECT_DISCOVERED

If GREEN:
  B7_REVALIDATED_SHA:
  relevant regression result:
  full semantic result:
  runtime boundary result:
  docs result:
  git diff --check:
  new B-OP15 path:
  evidence commit SHA:
  remote HEAD:
  status = READY_FOR_INDEPENDENT_P34_REREVIEW

B8 started = NO
PreparedApplyPlan changed = NO
OperationEngine changed = NO
GT-G1-04-C started = NO
GT-G1-05 changed = NO
History rewritten = NO
Force push = NO
```

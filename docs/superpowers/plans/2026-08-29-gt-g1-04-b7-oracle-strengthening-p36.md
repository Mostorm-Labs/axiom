# GT-G1-04-B B7 Oracle Strengthening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for this P36 repair. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Strengthen the existing B7 behavioral suite into an independent expected-projection oracle, validate normal fixtures against A structural preconditions, complete failure/performance evidence, and rebind B-OP15 without modifying B7 production code.

**Architecture:** Keep production B7 implementation frozen at `f98087b29830ea50bc9984be9ef9dfb9aa88e13b`. Repair only the two B7 test files so every success case compares the complete production fragment against a test-owned expected fragment, every rejection proves non-empty-sentinel atomicity, and normal fixtures are structurally admissible at the A/B boundary. Run the strengthened oracle first; if it exposes a genuine production mismatch, preserve the RED and stop for a concrete P35 implementation classification.

**Tech Stack:** C++20, GoogleTest/CTest, `validatePayloadStructure`, ReferenceObjectStore, IndexedObjectStore, test-only ObjectStore mutator, CountingStore instrumentation, JSON Gate evidence.

**Spec:**
- Frozen B7 P31: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-operation-specific-state-p31.md@84a95fbb46ef301833a4ce34a9503451f5d95579`
- First oracle-repair package: `docs/superpowers/plans/2026-08-29-gt-g1-04-b7-behavioral-oracle-repair-p36.md@32950127b50da42a31a7d0ca7570321b1f86b2f4`
- Current test snapshot: `516ff4016f85507ca7d09bde5de2bb5dd9fc8af8`
- Current insufficient evidence occurrence: `9e2e9fa2c4772def3c24eb39b85d6fc946b235aa`
- Current production implementation base: `f98087b29830ea50bc9984be9ef9dfb9aa88e13b`
- P34 blocker: `notion:3cb4c57a-590c-81f3-99e3-fc5ec6aec0c0`
- P35/P36 authorization: `notion:3cb4c57a-590c-819f-a725-e04c07616785`

## Global Constraints

- Primary defect = `TEST_DEFECT`; secondary = `EVIDENCE_GAP`.
- `IMPLEMENTATION_DEFECT` is NOT pre-declared.
- Do not modify B7 production source or headers during this occurrence.
- Modify only `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp` and `runtime/semantic/tests/rich_text_state_plan_test.cpp`. Test CMake may change only if a real registration defect is proven first.
- Do not modify B2/B3/B-AUTH-02/B4/B5/B6, ObjectStore/ObjectIndex, schema/protobuf/wire, B8, PreparedApplyPlan, OperationEngine, GT-G1-04-C, or GT-G1-05.
- All expected `StatefulIssue` and plan-fragment values come from the frozen P31 contract and authority-derived test fixtures, never from running production once and copying its output.
- For normal B7 cases, prove the payload and seeded ObjectRecords satisfy A structural preconditions before invoking the B7 state validator. Explicit defense-in-depth cases whose purpose is an invalid kind/version may be labeled and excluded from this structural-precondition assertion, but they must not be counted as normal canonical-path coverage.
- Every failure test must initialize the output with a non-empty sentinel and prove exact value equality after rejection.
- Every success/failure case must prove Reference/Indexed parity, immutable base projections, and Indexed `indexMatchesRebuild`.
- Every production-path performance case must prove `allObjectsCalls == 0` through CountingStore; source-code inspection alone is supporting evidence, not a substitute.
- Preserve all historical evidence. No amend/rebase/squash/reset/force-push.

---

## Task 1: Synchronize and freeze the reviewed occurrence

**Files:** read-only.

- [ ] Run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git fetch origin
git rev-parse origin/codex/gt-g1-04-operation-apply
```

Expected remote control-plane HEAD is the commit containing this plan.

- [ ] Verify `9e2e9fa2c4772def3c24eb39b85d6fc946b235aa` is an ancestor and fast-forward only.

- [ ] Confirm cumulative changes from `32950127...` through `516ff401...` are test-only and production B7 remains byte-identical to `f98087b...`.

If not, stop with `BLOCKED_STARTING_STATE_MISMATCH`.

---

## Task 2: Replace parity-only harness with independent expected-fragment harness

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

### Required test-owned helpers

Keep the thirteen signature `static_assert`s.

Replace the current `diff(...)` helper with explicit helpers that accept an authority-derived expected fragment:

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

Each helper must:

```text
seed identical Reference and Indexed stores
capture before projections
initialize both outputs to the same non-empty sentinel
invoke the same payload against both stores
assert Reference issue == authority-derived expected issue
assert Indexed issue == authority-derived expected issue
assert Reference issue == Indexed issue
if success:
  assert reference output == expected_success
  assert indexed output == expected_success
  assert reference output == indexed output
if rejection:
  assert reference output == non_empty_sentinel
  assert indexed output == non_empty_sentinel
assert both base projections unchanged
assert Indexed indexMatchesRebuild
```

The expected fragment must be constructed directly in the test case from fixture values. Do not derive it by calling the production validator or RichText production helper.

### Non-empty sentinels

Use reusable sentinels such as:

```cpp
CreateObjectsStatePlanInputs createSentinel() {
    return {{shape(900)}};
}

ReplaceObjectsStatePlanInputs replaceSentinel() {
    return {{shape(901)}};
}

SplitStrokesStatePlanInputs splitSentinel() {
    return {{id(902)}, {vectorStroke(903)}};
}
```

All failure paths, including every SplitStrokes rejection, must use these non-empty sentinels.

- [ ] Compile after harness replacement before changing case fixtures. Expected result may be RED until each case provides a complete expected fragment.

---

## Task 3: Add A-structural precondition guards to normal B7 fixtures

**Files:**
- `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`
- `runtime/semantic/tests/rich_text_state_plan_test.cpp` only as needed for shared valid fixture values.

Include `canvas/semantic/validator.hpp` in the operation-specific test.

Add a test-side helper:

```cpp
Operation structuralOperation(OperationPayload payload) {
    Operation op{};
    op.id = OperationId{ObjectId::fromUint64(7001)};
    op.document_id = DocumentId{ObjectId::fromUint64(7002)};
    op.schema_version = 1;
    op.payload_version = 1;
    op.payload = std::move(payload);
    return op;
}

template <typename Payload>
void expectStructurallyValid(const Payload& payload) {
    Operation op = structuralOperation(OperationPayload{payload});
    ASSERT_TRUE(validatePayloadStructure(op).ok());
}
```

If the concrete ID wrapper constructors differ, use the repository's existing typed-ID construction idiom while preserving non-zero IDs. Do not weaken the assertion.

For seeded records, add:

```cpp
void expectRecordsStructurallyValid(const std::vector<ObjectRecord>& records) {
    expectStructurallyValid(InsertObjectsOp{records});
}
```

Before every normal stateful case:

```text
assert seed records structurally valid
assert operation payload structurally valid
then invoke B7 validator
```

### Repair invalid fixture builders

At minimum correct these fixture classes until the structural guard is green:

- RichText ObjectRecord: canonical document with at least one paragraph; valid ParagraphStyle; empty text represented by `runs=[]`.
- EditRichText integration: all TextStyle values explicit and structurally valid, including non-zero FontResourceId, positive font size, allowed weight and valid color.
- SetImageContent positive payload: non-zero ResourceId, positive finite intrinsic and display sizes, valid content mode, valid optional source rect.
- VectorStroke/DabStroke: use a structurally valid released V1 StrokeRecord/BrushDescriptor consistent with the existing A validator. Prefer copying a known-valid fixture shape from the repository's existing A structural tests; after construction, the `expectStructurallyValid` guard is mandatory.
- EraseMask: use a non-empty supported geometry. For `SweptCircleMask`, create at least one finite `EraseCubicSegment` with positive endpoint radii and finite control points.
- VectorPath positive payload/record: ensure its geometry satisfies current A structural validation rather than relying on a default-initialized placeholder.

Explicit invalid-kind/version defense tests may skip the A-valid assertion only when their test name and comment say they are exercising the B2 defensive state seam. They are not evidence for the canonical A→B path.

- [ ] Run the focused operation target. If a fixture precondition fails, repair the fixture only; do not edit production.

---

## Task 4: Correct PLC_B09 to prove complete final-state staging

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Replace the current invalid-final-state fixture.

Use exactly this semantic shape:

```text
Sticky S = id 1
incoming RichText R_in = id 2, root
outgoing RichText R_out = id 3, current parent S
Group G = id 4, root

canonical SetPlacements items by ObjectId:
  id 2 -> parent S
  id 3 -> parent G
```

Why this fixture is authoritative:

```text
if capability were checked after item 1 only:
  Sticky S temporarily has R_in + R_out -> invalid cardinality

after the complete batch is staged:
  Sticky S has only R_in
  Group G has R_out
  final hierarchy/capability state is valid
```

Expected result:

```text
StatefulIssue::kNone
expected replacements exactly:
  R_in with parent S
  R_out with parent G
```

The test must fail if B7 validates Sticky cardinality before all replacements are staged.

---

## Task 5: Give every success case an exact expected plan projection

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

For each current positive case, provide an explicit `expected_success` object.

Required exact projections include:

### InsertObjects

```text
creates == exact inserted ObjectRecords in canonical payload order
```

### SetPlacements / SetTransforms

```text
replacements == exact original records with only placement/transform changed
all other fields byte/value-equal to seed records
```

### PatchProperties

- `PROP_B01`: exact changed PropertyBag.
- `PROP_B03`: cleared entry absent.
- `PROP_B04`: test at least two subcases: one scalar default and one tagged-value default; both canonicalize to absence.
- `PROP_B05`: exactly one replacement for the target; entries sorted by FieldId; both patch effects present.

### SetObjectSize

- Shape/Sticky: only width/height change.
- Image: assert resource ID, intrinsic size, source rect, content mode and Transform unchanged while display width/height change.

### SetVectorPathGeometry / SetImageContent

Assert exact new content and preservation of unrelated ObjectRecord fields.

### AddStroke

`creates == {exact stroke record}`.

### SplitStrokes

```text
source_delete_ids == exact canonical source IDs
replacement_creates == exact flattened replacements sorted by ObjectId
```

No hierarchy/connector delete closure is inferred.

### AddEraseMasks / RemoveEraseMasks

Assert exact target replacement including exact sorted remaining/added mask set and preservation of all unrelated fields.

### EditRichText integration

Construct expected RichTextContent in the test and assert the complete replacement ObjectRecord equals that expected value. Do not call `prepareRichTextDeltaState` to generate the expected value.

### SetConnectorContent

Assert the exact replacement record with exact new ConnectorContent and otherwise unchanged record.

- [ ] Run focused B7 after each family conversion. If a complete expected projection produces RED, follow the RED stop rule in Task 9.

---

## Task 6: Strengthen TXT_S01..TXT_S12 into full-value RichText oracles

**File:** `runtime/semantic/tests/rich_text_state_plan_test.cpp`

Keep the twelve exact case names but strengthen assertions.

### Required full assertions

- `TXT_S01`: input `A😀B`, insert `X` at scalar position 2, expected exact text `A😀XB`, same paragraph ID/style, normalized runs exactly as expected.
- `TXT_S02`: use two different adjacent runs such that deletion crosses their boundary; assert exact surviving scalar sequence, exact styles and normalized run vector, not merely `runs.size()`.
- `TXT_S03`: split at zero -> two exact paragraphs; left empty `runs=[]`, right contains all original text; new paragraph copies source ParagraphStyle.
- `TXT_S04`: split at end -> left retains all text, right is canonical empty paragraph; both exact IDs/styles asserted.
- `TXT_S05`: non-empty sentinel output remains exactly unchanged.
- `TXT_S06`: adjacent merge -> exact concatenated text/run normalization, first ID retained, first ParagraphStyle retained, second removed.
- `TXT_S07`: non-adjacent merge -> exact non-empty sentinel unchanged.
- `TXT_S08`: use `styleA != styleB`. Start with one `styleA` run over `A😀B`; style only scalar `😀` with `styleB`; expect exactly three runs: `A/styleA`, `😀/styleB`, `B/styleA`. Add a second subcase where styling makes adjacent runs equal and assert merge normalization.
- `TXT_S09`: assert complete ParagraphStyle replacement and text/runs unchanged.
- `TXT_S10`: step 1 creates paragraph ID 2; step 2 modifies paragraph ID 2; assert the exact resulting two-paragraph document, proving staged reference resolution.
- `TXT_S11`: non-empty sentinel exact equality after step-2 failure.
- `TXT_S12`: insert then delete back to empty -> one paragraph remains with `runs=[]`, same paragraph ID/style.

All TextStyle/ParagraphStyle fixtures must be valid V1 values.

- [ ] Run focused RichText. If any authority-derived expected value is RED, preserve it and follow Task 9.

---

## Task 7: Complete rejection atomicity

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Audit every test with `expected_issue != kNone`.

For each:

```text
sentinel is non-empty
Reference output starts equal to sentinel
Indexed output starts equal to sentinel
result issue is exact expected issue
Reference output remains exactly sentinel
Indexed output remains exactly sentinel
base projections unchanged
Indexed indexMatchesRebuild
```

Specifically repair the current SplitStrokes cases that use `{}` as their sentinel.

Add a small meta-helper or assertion that rejects an empty sentinel in the failure branch so future tests cannot silently regress to default-output checks.

---

## Task 8: Complete CountingStore coverage for all thirteen families

**File:** `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Expand CountingStore counters:

```cpp
mutable std::size_t contains_calls = 0;
mutable std::size_t find_calls = 0;
mutable std::size_t all_objects_calls = 0;
mutable std::size_t children_calls = 0;
```

Create a table/helper-driven performance section with at least one structurally valid successful invocation for each family:

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

For every invocation assert:

```text
all_objects_calls == 0
```

Also record operation-specific observations:

- single-target replacement families: bounded `find_calls`.
- PatchProperties: base target resolution scales with distinct object IDs, not patch count times full scans.
- SplitStrokes: no DeleteClosure/full-store scan.
- mask operations: only target lookup + nested mask work; no full-store scan.
- hierarchy/capability operations may use `children_calls` only for required relation/cardinality checks.

### CON_B07 requirement

Rewrite `CON_B07_FreePointOnlyNeedsNoEndpointObjectLookup` to use CountingStore.

Run two comparable cases:

```text
A. FreePoint-only Connector content
B. Same Connector with one AttachedEndpoint to an existing connectable target
```

Assert:

```text
both all_objects_calls == 0
attached case performs endpoint-target lookup work beyond the free-point baseline
free-point case performs no endpoint-target lookup beyond target/staging lookups
```

Record observed counts in evidence; do not generalize those exact counts to unrelated operations.

---

## Task 9: Run strengthened oracle against unchanged production and STOP on genuine RED

Before running, prove no production file changed:

```bash
git diff --name-only f98087b29830ea50bc9984be9ef9dfb9aa88e13b -- \
  runtime/semantic/src/operation_specific_validation.cpp \
  runtime/semantic/src/rich_text_state_plan.cpp \
  runtime/semantic/include/canvas/semantic/operation_specific_validation.hpp \
  runtime/semantic/src/rich_text_state_plan.hpp
```

Expected: no output.

Run:

```bash
cmake --build build-semantic-b2 --target canvas_semantic_rich_text_state_plan_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_rich_text_state_plan_test --gtest_color=no

cmake --build build-semantic-b2 --target canvas_semantic_operation_specific_stateful_validation_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_operation_specific_stateful_validation_test --gtest_color=no
```

### Genuine RED rule

If any authority-derived expected fragment/value fails:

1. Confirm the test's A structural-precondition guard passes.
2. Confirm expected value comes from P31/authority, not implementation inference.
3. Keep the failing tests.
4. Commit the strengthened oracle as test-only.
5. Push normally.
6. Do NOT change production.
7. Do NOT create an acceptance B-OP15.
8. Return:

```text
B7_P36 = IMPLEMENTATION_DEFECT_DISCOVERED
strengthened test commit = <sha>
failing case IDs = ...
expected issue/fragment = ...
actual issue/fragment = ...
A structural precondition = PASS
likely owning production function = ...
production source changed = NO
B8 = NOT RELEASED
```

Return to independent P35 before any production repair.

---

## Task 10: GREEN path — commit and rerun fresh regression

Only if both strengthened focused targets pass unchanged production.

Commit all test changes:

```text
test(g1): strengthen B7 independent behavioral oracle
```

Record exact commit as:

```text
B7_STRONG_ORACLE_SHA
```

Push it before evidence generation.

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

## Task 11: Create new truthful B-OP15 occurrence

Create:

```text
verification/evidence/gates/G1/<B7_STRONG_ORACLE_SHA>/GT-G1-04-B/B-OP15.json
```

Do not overwrite either historical artifact under `f98087b...` or `516ff401...`.

Required bindings:

```text
sourceCommit = testedCommit = B7_STRONG_ORACLE_SHA
productionImplementationBase = f98087b29830ea50bc9984be9ef9dfb9aa88e13b
p31Ref = ...@84a95fbb...
firstOracleRepairRef = ...@32950127...
oracleStrengtheningRef = this file @ control-plane commit
P34 blocker = notion:3cb4c57a-590c-81f3-99e3-fc5ec6aec0c0
P35/P36 authorization = notion:3cb4c57a-590c-819f-a725-e04c07616785
```

Evidence must map every claimed case to:

```text
exact GoogleTest name
A structural-precondition status where applicable
expected StatefulIssue
expected plan-fragment digest/projection summary
fresh PASS observation
Reference/Indexed parity
base no-mutation
Indexed rebuild
non-empty sentinel on rejection
```

Add a `performance` section with all thirteen families and observed CountingStore counters, including CON_B07 free-point vs attached comparison.

Add exact fresh focused/relevant/full verification commands and counts.

### Materialization closure without self-reference ambiguity

1. Create B-OP15 with:

```text
materialization:
  sourceTestSnapshotPushed = true
  initialEvidenceCommitPushed = false
```

2. Commit only the new evidence artifact:

```text
evidence(g1): rebind B7 to strengthened oracle
```

Record `B7_EVIDENCE_SHA_1` and push it.

3. After remote push is confirmed, update only the same B-OP15 metadata to:

```text
materialization:
  sourceTestSnapshotPushed = true
  initialEvidenceCommit = B7_EVIDENCE_SHA_1
  initialEvidenceCommitPushed = true
  historyRewritten = false
  forcePush = false
```

4. Commit this metadata-only correction:

```text
evidence(g1): close B7 evidence materialization metadata
```

Record `B7_EVIDENCE_SHA_2` and push it.

The final reviewer-accessible branch HEAD must be `B7_EVIDENCE_SHA_2`. This second commit is expected and is not history cleanup.

---

## Exit Criteria

P36 may return `READY_FOR_INDEPENDENT_P34_REREVIEW` only when all are true:

1. Success cases assert complete authority-derived expected fragments, not only Reference/Indexed equality.
2. Normal fixtures have executable A-structural precondition proof.
3. `PLC_B09` proves a valid final-state batch that would be invalid under premature per-item capability validation.
4. `TXT_S01..TXT_S12` assert complete staged values; `TXT_S08` uses genuinely different styles.
5. Every rejection uses a non-empty sentinel and proves exact unchanged output.
6. All thirteen B7 families have CountingStore `allObjectsCalls == 0` evidence.
7. `CON_B07` distinguishes free-point-only from attached-endpoint lookup work.
8. Production B7 source remains unchanged.
9. Fresh focused/relevant/full regression evidence is green.
10. New B-OP15 maps claimed cases to executable tests and expected projection summaries.
11. Final materialization metadata is closed through the two-step evidence sequence.
12. B8/PreparedApplyPlan/OperationEngine remain untouched.

## Required Final Report

```text
GT-G1-04-B B7 ORACLE-STRENGTHENING P36 RESULT

Starting local HEAD:
Fetched remote HEAD:
Fast-forward:
Control-plane package ref:
P34 blocker:
P35/P36 authorization:

Production source changed = NO
Behavioral test files changed:
A structural-precondition guard:
Independent expected-fragment oracle:
PLC_B09 valid-final-state proof:
RichText TXT_S01..S12 full-value oracle:
Non-empty sentinel coverage:
13-family CountingStore coverage:
CON_B07 lookup differentiation:

Focused RichText result:
Focused B7 result:
Relevant regression result:
Full semantic result:
Runtime boundary result:
Docs result:
git diff --check:

If RED:
  strengthened test commit SHA:
  failing case IDs:
  expected:
  actual:
  A structural precondition:
  production source changed = NO
  status = IMPLEMENTATION_DEFECT_DISCOVERED

If GREEN:
  B7_STRONG_ORACLE_SHA:
  new B-OP15 path:
  B7_EVIDENCE_SHA_1:
  B7_EVIDENCE_SHA_2:
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

Do not declare B7 PASS or downstream acceptance. Independent P34 retains the final Gate verdict.

# GT-G1-04-B B7 Operation-Specific Stateful Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the thirteen non-Delete/non-Restore V1 operation-specific state validators as read-only plan-fragment producers, reusing accepted B2/B3/B4/B-AUTH-02 primitives and stopping strictly before B8 `PreparedApplyPlan` composition.

**Architecture:** Keep accepted B2 generic state primitives unchanged. B7 lives in a new `operation_specific_validation` module that turns each normalized, structurally valid payload into one of three minimal value-only fragments: creates, replacements, or SplitStrokes source-deletes + replacement-creates. Placement-affecting operations stage the complete resulting graph and reuse B3 plus B-AUTH-02; Connector content reuses B4. RichText sequential staged semantics live in one B7-internal helper, not in B2 and not in B8.

**Tech Stack:** C++20, runtime/semantic, GoogleTest/CTest, ReferenceObjectStore, IndexedObjectStore, StagedObjectView, existing B2/B3/B4/B5/B6 APIs, source-bound JSON evidence.

**Spec:**
- Current semantic composition gate: Notion `3c94c57a-590c-8153-955e-dfbb4b6b7da3` (`G1-04-A Semantic Authority Closure Gate v0.1`).
- Current A structural authority: Notion `3c94c57a-590c-81cf-97eb-f6c89fce563e` (`Operation Structural Semantics V1 Closure v0.1`).
- Operation payload/state authority consumed by the semantic freeze: Notion `3c44c57a-590c-81b7-8f2c-e18ff60bb6c0` (`Operation Payload + Validation Rules v0.1`).
- B-AUTH-02 Current Authority: Notion `3cb4c57a-590c-815a-b8fa-cd785a837da7`.
- Field registry/default/clear closure: Notion `3c44c57a-590c-81d9-a77d-c51bd00419e1`.
- RichText document/delta contract: Notion `3c44c57a-590c-8163-9c2e-d63028f984b4`, consumed by the V1 final semantic gate.
- Existing base B plan: `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`.
- Current B6 accepted source/test snapshot: `871eec8172674c8fba85548434b5e96c7c30fbf8`; corrected current B-RESTORE evidence occurrence is materialized by `68dd235205253f7080d0a8d28daa957dfe2f0dbd`.
- Current B5 accepted source/test snapshot: `36669523ab3a5cc3e0c479cf6f0a5ae6119d240c`; current B-DELETE evidence occurrence is materialized by `af50bb0bf43d4a547fdee3a3b5ecb971dc841c9e`.

## Global Constraints

- B7 starts only after A normalization/envelope/payload structural validation has passed. Do not repeat A collection ordering, duplicate-key, finite numeric, leaf, hard-limit, UTF-8, geometry, BrushFamily, or wire validation.
- B2 `operation_state_validator.*` is accepted infrastructure and MUST NOT be modified by this package.
- B3 `hierarchy_validation.*` is accepted topology/existence/cycle ownership and MUST NOT be modified.
- B-AUTH-02 helper `hierarchy_capability_validation.*` is accepted parent/child capability + Sticky `0..1 RichText` ownership and MUST NOT be modified.
- B4 `connector_validation.*` is accepted endpoint/connectability/StablePort ownership and MUST NOT be modified.
- B5 production/test/evidence and B6 production/test/evidence are prerequisites, not implementation scope.
- B8 remains the first composition point for `PreparedApplyPlan` and `OperationEngine`. B7 MUST NOT add either type or dispatch on the fifteen-operation `OperationPayload` variant.
- No ObjectStore/ObjectIndex mutation, `ObjectStoreMutator`, SemanticGeneration, ChangeSet, CanonicalCommitStamp, History, DataBridge, Outbox, renderer state, resource availability query, network query, or snapshot bootstrap.
- Every validator is whole-operation atomic at the output boundary: build a local result and assign `*out` only after every state check succeeds. On failure, caller-provided output and base ObjectStore remain unchanged.
- Same canonical state + same normalized payload must yield the same `StatefulIssue` and plan fragment for ReferenceObjectStore and IndexedObjectStore.
- No `allObjects()` scan is permitted in production B7 validators. Operations may use `find`, StagedObjectView overlay lookup, `children(sticky)` for B-AUTH-02 cardinality, and per-target nested mask/text scans.
- Empty Sticky is valid. Do not reintroduce exactly-one Sticky child, child-order role inference, `primaryChildRole`, a schema/proto field, or a second hierarchy.
- `InsertObjects` does not enforce the older “Sticky + RichText must always be created together” wording as a runtime historical predicate. Under B-AUTH-02 that is a product/producer construction obligation; empty Sticky is valid semantic state.
- Resource bytes/materialization are not ObjectStore state for `SetImageContent`, RichText fonts, or stroke texture resources.
- Preserve all historical evidence and commits. No amend/rebase/squash/reset/force-push.

---

## File Structure

### Create

- `runtime/semantic/include/canvas/semantic/operation_specific_validation.hpp`
  - public B7 plan-fragment value types and thirteen explicit validator entry points.
- `runtime/semantic/src/operation_specific_validation.cpp`
  - operation-specific state algorithms; no top-level Operation dispatcher.
- `runtime/semantic/src/rich_text_state_plan.hpp`
  - B7-internal RichText staged-delta helper declaration.
- `runtime/semantic/src/rich_text_state_plan.cpp`
  - deterministic Unicode-scalar RichTextDelta planning over a value copy.
- `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`
  - thirteen operation families, Reference/Indexed parity, atomicity, performance instrumentation.
- `runtime/semantic/tests/rich_text_state_plan_test.cpp`
  - focused sequential RichTextDelta staged-state cases.

### Modify

- `runtime/semantic/CMakeLists.txt`
- `runtime/semantic/tests/CMakeLists.txt`

### Evidence-only after source/test commit

- `verification/evidence/gates/G1/<B7_SOURCE_SHA>/GT-G1-04-B/B-OP15.json`

### Forbidden modifications

- `runtime/semantic/include/canvas/semantic/operation_state_validator.hpp`
- `runtime/semantic/src/operation_state_validator.cpp`
- `runtime/semantic/include/canvas/semantic/staged_object_view.hpp`
- `runtime/semantic/src/staged_object_view.cpp`
- B3 hierarchy files
- B-AUTH-02 hierarchy-capability files
- B4 connector files
- B5 delete-closure files/tests/evidence
- B6 restore files/tests/evidence
- ObjectStore / ReferenceObjectStore / IndexedObjectStore / ObjectIndex
- schema/protobuf/wire/generated files
- `apply_plan.*`
- `operation_engine.*`
- GT-G1-04-C
- GT-G1-05

If a required behavior cannot be expressed within the file set above, STOP with the earliest owning blocker instead of widening scope.

---

## Frozen Public B7 Interfaces

Create `operation_specific_validation.hpp` with these exact value-only fragment types:

```cpp
#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <vector>

namespace canvas::semantic {

struct CreateObjectsStatePlanInputs final {
    std::vector<ObjectRecord> creates;
};

struct ReplaceObjectsStatePlanInputs final {
    std::vector<ObjectRecord> replacements;
};

struct SplitStrokesStatePlanInputs final {
    std::vector<ObjectId> source_delete_ids;
    std::vector<ObjectRecord> replacement_creates;
};

[[nodiscard]] StatefulResult validateInsertObjectsState(
    const InsertObjectsOp&, const ObjectStore&, CreateObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetPlacementsState(
    const SetPlacementsOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetTransformsState(
    const SetTransformsOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validatePatchPropertiesState(
    const PatchPropertiesOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetObjectSizeState(
    const SetObjectSizeOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetVectorPathGeometryState(
    const SetVectorPathGeometryOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetImageContentState(
    const SetImageContentOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateAddStrokeState(
    const AddStrokeOp&, const ObjectStore&, CreateObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSplitStrokesState(
    const SplitStrokesOp&, const ObjectStore&, SplitStrokesStatePlanInputs* out);
[[nodiscard]] StatefulResult validateAddEraseMasksState(
    const AddEraseMasksOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateRemoveEraseMasksState(
    const RemoveEraseMasksOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateEditRichTextState(
    const EditRichTextOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetConnectorContentState(
    const SetConnectorContentOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);

} // namespace canvas::semantic
```

No function accepts `Operation`, `OperationId`, `AppliedOperationView`, `ApplySource`, mutator, callback, generation, or history state. B1 idempotency and B8 composition stay outside B7.

### Frozen failure/output contract

For every function:

```text
success:
  StatefulIssue::kNone
  *out receives exactly the deterministic plan fragment

failure:
  return exact B-owned StatefulIssue
  *out unchanged
  base ObjectStore unchanged
  IndexedObjectStore indexMatchesRebuild unchanged
```

### Frozen B7 issue mapping

```text
missing existing target/source        -> kObjectMissing
new-object/replacement identity clash  -> kObjectAlreadyExists
wrong ObjectKind / Field applicability -> kInvalidApplicability
missing parent/reference               -> pass through kInvalidReference
hierarchy cycle                        -> pass through kHierarchyCycle
B4 Connector failure                   -> pass through B4 exact StatefulIssue
mask already-present/missing state     -> kMaskStateInvalid
RichText staged ref/range/order state  -> kTextStateInvalid
```

A structural/leaf failure is a violated B7 precondition; do not invent a second A validator inside B7.

---

### Task 1: RED-first public B7 boundary

**Files:**
- Create: `runtime/semantic/include/canvas/semantic/operation_specific_validation.hpp`
- Create test skeleton: `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:** exactly the frozen public API above.

- [ ] **Step 1: Add compile-level tests naming all thirteen validators and all three output fragment types.**

The test must instantiate each function signature through a typed function pointer so signature drift is a compile error.

```cpp
using InsertFn = StatefulResult (*)(
    const InsertObjectsOp&, const ObjectStore&, CreateObjectsStatePlanInputs*);
static_assert(std::is_same_v<decltype(&validateInsertObjectsState), InsertFn>);
```

Repeat explicitly for all thirteen functions.

- [ ] **Step 2: Register the focused target before production source exists.**

Target name:

```text
canvas_semantic_operation_specific_stateful_validation_test
```

- [ ] **Step 3: Run RED.**

```bash
cmake -S . -B build-semantic-b2 \
  -DCANVAS_BUILD_SEMANTIC=ON \
  -DBUILD_TESTING=ON \
  -DCANVAS_SEMANTIC_ENABLE_PROTOBUF=OFF \
  -DCANVAS_BUILD_POC01=OFF \
  -DCANVAS_POC01_BUILD_TESTS=OFF

cmake --build build-semantic-b2 \
  --target canvas_semantic_operation_specific_stateful_validation_test -j2
```

Expected RED: missing B7 header/source/symbols. Capture exact command, output, and exit code.

Do not commit the failing state.

---

### Task 2: InsertObjects and SetPlacements resulting-state validation

**Files:**
- Create/modify: `runtime/semantic/src/operation_specific_validation.cpp`
- Test: `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

**Consumes:** B2 `validateRecordStateForOperation`, B3 `validateStagedHierarchy`, B-AUTH-02 `validateStagedHierarchyCapabilities`, B4 `validateConnectorReferences`, `StagedObjectView`.

#### InsertObjects exact algorithm

```text
local CreateObjectsStatePlanInputs result
fresh StagedObjectView staged(apply_base)

for candidate in insert.objects:
  if !staged.stageCreate(candidate):
    return kObjectAlreadyExists

for candidate:
  validateRecordStateForOperation(candidate, kCreateAbsent)

hierarchy_edits = every candidate {id, placement}
validateStagedHierarchy(staged, hierarchy_edits)

affected_child_ids = every candidate id
validateStagedHierarchyCapabilities(staged, affected_child_ids)

for candidate where kind == Connector:
  extract ConnectorContent defensively
  validateConnectorReferences(staged, content)

result.creates = insert.objects
*out = move(result)
return kNone
```

Do not enforce exactly-one Sticky child. Empty Sticky is valid.

Required Insert cases:

```text
INS-B01 Group + Shape staged together -> kNone
INS-B02 existing candidate identity -> kObjectAlreadyExists
INS-B03 Connector + staged target -> kNone
INS-B04 Connector absent attached target -> kInvalidReference
INS-B05 Sticky + Shape child -> kInvalidApplicability
INS-B06 Sticky + two RichText children -> kInvalidApplicability
INS-B07 empty Sticky -> kNone
INS-B08 one bad record rejects whole fragment/output unchanged
```

#### SetPlacements exact algorithm

```text
local ReplaceObjectsStatePlanInputs result
fresh StagedObjectView staged(apply_base)
local replacements in payload item order

for item in placements.items:
  current = staged.find(item.object_id)
  if missing: return kObjectMissing
  validateRecordStateForOperation(*current, kPlacementTarget)
  replacement = *current
  replacement.placement = item.placement
  if !staged.stageReplace(replacement): return kObjectMissing
  replacements += replacement

hierarchy_edits = replacements {id, placement}
validateStagedHierarchy(staged, hierarchy_edits)
validateStagedHierarchyCapabilities(staged, every replaced id)

result.replacements = replacements
*out = move(result)
return kNone
```

Required placement cases:

```text
PLC-B01 move Shape under Group -> kNone
PLC-B02 missing target -> kObjectMissing
PLC-B03 missing parent -> kInvalidReference from B3
PLC-B04 batch cycle -> kHierarchyCycle from B3
PLC-B05 move RichText into empty Sticky -> kNone
PLC-B06 move second RichText into occupied Sticky -> kInvalidApplicability
PLC-B07 move Shape into Sticky -> kInvalidApplicability
PLC-B08 move sole RichText out of Sticky leaves empty Sticky -> kNone
PLC-B09 multi-item final-state batch validates after all replacements are staged
```

B3 topology always runs before B-AUTH-02 capability. Do not duplicate cycle traversal in B7.

---

### Task 3: Deterministic existing-object replacement families

**Files:**
- Modify: `runtime/semantic/src/operation_specific_validation.cpp`
- Test: `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

#### SetTransforms

For each canonical item:

```text
current = apply_base.find(object_id)
missing -> kObjectMissing
validateRecordStateForOperation(*current, kTransformTarget)
replacement = *current
replacement.transform = item.transform
append replacement
```

A already owns finite/canonical Transform2D validation. B7 does not re-check determinant/numeric rules.

Cases: valid target, missing target, batch atomicity, Reference/Indexed parity.

#### PatchProperties

Patches are canonical `(objectId, fieldId)` order and may contain multiple fields for one ObjectId. Produce exactly **one replacement per distinct target ObjectId**.

Algorithm:

```text
map<ObjectId, ObjectRecord> working

for patch in patches:
  on first patch for object:
    current = apply_base.find(object_id)
    missing -> kObjectMissing
    working[id] = *current

  record = working[id]

  SET:
    value = PropertyValue in patch.value
    requirePropertyApplicability(record, field_id, &value)
    if value equals immutable V1 registry default:
      remove field entry
    else:
      insert/replace field entry by field_id

  CLEAR:
    requirePropertyApplicability(record, field_id, nullptr)
    remove field entry

  otherwise:
    return kInvalidApplicability  // defensive only; A should have rejected it

for each working record:
  sort properties.entries by field_id
  append one replacement in ObjectId order
```

Registry defaults are exact:

```text
0x00000001 visible                  -> bool true
0x00000002 locked                   -> bool false
0x00000003 opacity                  -> float 1.0f
0x00000004 blendMode                -> BlendModeValue::kNormal
0x00000100 fillStyle                -> FillStyleValue{NoFill{}}
0x00000101 strokeStyle              -> StrokeStyleValue{NoStroke{}}
0x00000200 connectorStartDecoration -> ConnectorDecorationValue::kNone
0x00000201 connectorEndDecoration   -> ConnectorDecorationValue::kNone
```

Required cases:

```text
PROP-B01 SET applicable field -> replacement
PROP-B02 wrong target-kind applicability -> kInvalidApplicability
PROP-B03 CLEAR removes explicit entry
PROP-B04 SET registry-default canonicalizes to absence
PROP-B05 two different fields on one object -> one replacement containing both effects
PROP-B06 missing target -> kObjectMissing
PROP-B07 failure leaves output/base unchanged
```

#### SetObjectSize

Use B2 `kSizeTarget`. Update only:

```text
ShapeContent.width/height
ImageContent.width/height
StickyContent.width/height
```

For Image preserve `resource_id`, intrinsic dimensions, source_rect, content_mode and Transform exactly.

Cases: Shape/Image/Sticky positive; Group/Stroke wrong-kind rejection; missing target.

#### SetVectorPathGeometry

Require existing `VectorPath v1` through B2 `kVectorPathTarget`; replace only `VectorPathContent.geometry`.

#### SetImageContent

Require existing `Image v1` through B2 `kImageTarget`; replace the complete `ImageContent` value exactly. Never query BlobStore/network/resource materialization.

For every replacement family above, build local output and assign only after the full batch succeeds.

---

### Task 4: Stroke create/split and EraseMask state

**Files:**
- Modify: `runtime/semantic/src/operation_specific_validation.cpp`
- Test: `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

#### AddStroke

```text
local result
StagedObjectView staged(base)
if !staged.stageCreate(add.object): kObjectAlreadyExists
validateRecordStateForOperation(add.object, kStrokeTarget)
validateStagedHierarchy(staged, [{id, placement}])
validateStagedHierarchyCapabilities(staged, [id])
result.creates = [add.object]
assign success only
```

This explicitly proves AddStroke accepts only VectorStroke/DabStroke even though A validates generic ObjectRecord structure.

Cases:

```text
STR-B01 VectorStroke create -> kNone
STR-B02 DabStroke create -> kNone
STR-B03 existing ID -> kObjectAlreadyExists
STR-B04 non-stroke ObjectRecord -> kInvalidApplicability
STR-B05 stroke under Group -> kNone
STR-B06 stroke under Sticky -> kInvalidApplicability
```

#### SplitStrokes

Do not call B5 DeleteClosure. Split source removal is an explicit source-delete fragment, not subtree/cascade delete semantics.

Exact order:

```text
local result
StagedObjectView staged(base)

for split in splits:
  source = staged.find(source_stroke_id)
  missing -> kObjectMissing
  validateRecordStateForOperation(*source, kStrokeTarget)

for every replacement across all splits:
  // do this before deleting sources so a replacement cannot reuse a source ID
  if !staged.stageCreate(replacement): kObjectAlreadyExists
  validateRecordStateForOperation(replacement, kStrokeTarget)

for split in splits:
  if !staged.stageDelete(source_stroke_id): kObjectMissing

validateStagedHierarchy(staged, every replacement {id, placement})
validateStagedHierarchyCapabilities(staged, every replacement id)

result.source_delete_ids = source ids in canonical split order
result.replacement_creates = all replacements flattened then sorted by ObjectId
assign success only
```

No parentStrokeId/lineage field is created.

Required cases:

```text
SPL-B01 existing stroke -> two absent stroke replacements -> kNone
SPL-B02 missing source -> kObjectMissing
SPL-B03 source exists but non-stroke -> kInvalidApplicability
SPL-B04 replacement collides with unrelated existing ID -> kObjectAlreadyExists
SPL-B05 replacement reuses source ID -> kObjectAlreadyExists
SPL-B06 replacement non-stroke -> kInvalidApplicability
SPL-B07 replacement invalid parent/capability -> B3/B-AUTH-02 issue
SPL-B08 one failing split rejects all source-delete/create fragments
```

#### AddEraseMasks

A already proves payload mask IDs/geometry are structurally canonical and unique within the payload. B7 state rule is per target object:

```text
target exists
validate kStrokeTarget
for incoming mask:
  if same mask ID already exists on target.erase_masks:
    kMaskStateInvalid
replacement = target copy
merge incoming masks into replacement.erase_masks in ObjectId order
```

Mask identity collision is checked against the target object and payload, per authority; do not invent a document-global mask index.

Cases: VectorStroke success, DabStroke success, wrong target kind, missing target, existing mask collision, output/base atomicity.

#### RemoveEraseMasks

Strict V1 state rule:

```text
target exists
validate kStrokeTarget
EVERY requested mask ID must currently exist on that target
if any missing -> kMaskStateInvalid
replacement = target copy with all requested masks removed
```

Missing mask is not a replay no-op.

Cases: success, one missing ID rejects entire item/op, wrong kind, missing object, output/base unchanged.

---

### Task 5: RichText staged-delta planner + EditRichText

**Files:**
- Create: `runtime/semantic/src/rich_text_state_plan.hpp`
- Create: `runtime/semantic/src/rich_text_state_plan.cpp`
- Create: `runtime/semantic/tests/rich_text_state_plan_test.cpp`
- Modify: `runtime/semantic/src/operation_specific_validation.cpp`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Internal interface:**

```cpp
namespace canvas::semantic::internal {

[[nodiscard]] StatefulResult prepareRichTextDeltaState(
    const RichTextContent& current,
    const RichTextDelta& delta,
    RichTextContent* out);

} // namespace canvas::semantic::internal
```

Output unchanged on failure.

A already validates delta version, non-empty steps, UTF-8, style leaf validity, and structural numeric fields. B7 owns references/ranges/order against the current staged RichText document.

#### Unicode scalar rule

Implement deterministic UTF-8 scalar iteration locally; do not use platform UTF-16 indices, grapheme segmentation, ICU version-dependent legality, or byte offsets as semantic positions.

Internal helpers may include:

```cpp
std::optional<std::size_t> byteOffsetForScalar(
    std::string_view valid_utf8, std::uint32_t scalar_offset);
std::uint32_t scalarLength(std::string_view valid_utf8);
```

A guarantees valid UTF-8, but helpers must fail safely rather than read invalid bytes out of bounds.

#### Sequential delta rule

```text
result = current
for step in delta.steps IN PAYLOAD ORDER:
  validate references/ranges against result produced by previous step
  apply step to result value copy
  normalize affected paragraph runs
*out = move(result) only after all steps succeed
```

Run normalization after each relevant step:

```text
remove empty TextRuns
merge adjacent runs whose TextStyle is canonical-equal
preserve run order
never normalize Unicode text
never merge across paragraphs
```

#### Step semantics

**InsertText**

```text
paragraph must exist
scalar_offset in [0, paragraph scalar length]
insert payload text with payload's complete TextStyle
split runs as required
normalize
```

Never infer style from neighboring text/caret/UI.

**DeleteText**

```text
paragraph must exist
scalar_count > 0
start + count fully inside paragraph scalar range
delete across run boundaries as needed
paragraph may become empty (runs=[])
normalize
```

**SplitParagraph**

```text
source paragraph exists
scalar_offset in [0, length]
new_paragraph_id is non-zero and absent from the CURRENT STAGED RichText document
split text/runs at scalar offset
source keeps original paragraph id and left side
new paragraph is inserted immediately after source and gets right side
new paragraph initially copies source ParagraphStyle
normalize both sides
```

**MergeParagraph**

```text
first and second both exist
first != second
second must be immediately after first in current staged paragraph order
append second runs/text to first
keep first paragraph id and first ParagraphStyle
remove second paragraph
normalize first
document remains >= 1 paragraph
```

**SetInlineStyle**

```text
paragraph exists
scalar_count > 0
range fully inside paragraph
replace the complete TextStyle for the range
split runs as required
text scalar sequence unchanged
normalize
```

**SetParagraphStyle**

```text
paragraph exists
replace complete ParagraphStyle
text/runs unchanged
```

Any missing paragraph, colliding split paragraph ID, out-of-range scalar position/range, non-adjacent merge, or staged-step reference failure returns `kTextStateInvalid` and leaves caller output unchanged.

#### Focused RichText cases

At minimum:

```text
TXT-S01 insert at scalar boundary in A😀B uses Unicode scalar offset
TXT-S02 delete across run boundary + normalize
TXT-S03 split at zero
TXT-S04 split at end
TXT-S05 split new paragraph ID collision -> kTextStateInvalid
TXT-S06 merge adjacent -> first id/style retained
TXT-S07 merge non-adjacent -> kTextStateInvalid
TXT-S08 SetInlineStyle splits/merges runs deterministically
TXT-S09 SetParagraphStyle replaces complete style
TXT-S10 step 2 resolves state produced by step 1
TXT-S11 step 2 invalid => whole delta output unchanged
TXT-S12 empty paragraph remains one paragraph with runs=[]
```

#### EditRichText validator

```text
current = base.find(op.object_id)
missing -> kObjectMissing
validateRecordStateForOperation(*current, kRichTextTarget)
extract RichTextContent defensively
prepareRichTextDeltaState(current_content, op.delta, &next_content)
copy current ObjectRecord
replacement.content = next_content
result.replacements = [replacement]
assign success only
```

No platform font lookup or resource-materialization lookup.

---

### Task 6: SetConnectorContent consumption of B4

**Files:**
- Modify: `runtime/semantic/src/operation_specific_validation.cpp`
- Test: `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`

Exact algorithm:

```text
current = base.find(object_id)
missing -> kObjectMissing
validateRecordStateForOperation(*current, kConnectorTarget)
replacement = *current
replacement.content = payload.content
StagedObjectView staged(base)
staged.stageReplace(replacement)
validateConnectorReferences(staged, payload.content)
result.replacements = [replacement]
assign success only
```

Required cases:

```text
CON-B01 valid Connector + existing connectable target -> kNone
CON-B02 target object is not Connector -> kInvalidApplicability
CON-B03 attached target missing -> kInvalidReference
CON-B04 attached target exists but non-connectable -> B4 rejection
CON-B05 StablePort valid against actual Shape/Image/Sticky v1 -> kNone
CON-B06 invalid/unsupported StablePort for actual target -> B4 rejection
CON-B07 FreePoint-only Connector needs no ObjectStore reference lookup beyond target Connector
```

Do not duplicate StablePort registry or connectability matrix inside B7.

---

### Task 7: Differential, no-mutation, performance, regression

**Files:**
- Test: `runtime/semantic/tests/operation_specific_stateful_validation_test.cpp`
- Test: `runtime/semantic/tests/rich_text_state_plan_test.cpp`

#### Reference/Indexed differential

For every state-owning positive and negative B7 family, run equivalent fixtures against both stores and compare:

```text
StatefulIssue
create/replacement/delete fragment projection
base projection before/after
Indexed indexMatchesRebuild
```

At minimum all thirteen operation names must appear in the differential table.

#### No-mutation

Every failure helper pre-fills output with sentinel data and asserts it remains unchanged. Every success/failure records base `allObjects()` before/after in tests only; production validators must not call `allObjects()`.

#### Performance instrumentation

Required production-path observations:

```text
single-target Transform/Size/VectorPath/Image/RichText:
  bounded find lookup
  allObjects = 0

PatchProperties:
  one logical target resolution per distinct ObjectId
  work scales with patches + target PropertyBag entries
  allObjects = 0

InsertObjects:
  stageCreate identity check per candidate
  staged parent/Connector resolution uses overlay where self-contained
  allObjects = 0

SetPlacements:
  work scales with affected items + required parent/cycle/capability relations
  no full ObjectId scan
  allObjects = 0

SplitStrokes:
  work scales with source count + replacement count + replacement hierarchy refs
  no B5 DeleteClosure scan
  allObjects = 0

Add/RemoveEraseMasks:
  per-target nested mask scan only
  allObjects = 0

SetConnectorContent:
  one target Connector resolution + required endpoint target lookups
  allObjects = 0
```

Do not freeze a universal exact `find_calls == N` across operations where StagedObjectView may perform required relation lookups. Evidence must distinguish identity/target lookups from relation lookups.

#### Verification commands

After implementation:

```bash
cmake --build build-semantic-b2 \
  --target canvas_semantic_rich_text_state_plan_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_rich_text_state_plan_test \
  --gtest_color=no

cmake --build build-semantic-b2 \
  --target canvas_semantic_operation_specific_stateful_validation_test -j2
./build-semantic-b2/runtime/semantic/tests/canvas_semantic_operation_specific_stateful_validation_test \
  --gtest_color=no

ctest --test-dir build-semantic-b2 \
  -R 'StatefulValidationContext|StagedObjectView|ReferenceObjectStore|IndexedObjectStore|ObjectStoreDifferential|HierarchyValidation|HierarchyCapability|ConnectorStatefulValidation|DeleteClosure|RestoreStatefulValidation|OperationSpecific|RichTextState|canvas_semantic_runtime_boundaries' \
  --output-on-failure

cmake --build build-semantic-b2 -j2
ctest --test-dir build-semantic-b2 --output-on-failure

python3 tools/check_runtime_boundaries.py --root .
python3 tools/check_docs.py
git diff --check
```

Record actual target names/counts if CMake discovery differs, but do not omit either focused B7 suite.

---

## B-OP15 Evidence Contract

After GREEN verification, create one source/test/build-registration commit containing only authorized B7 files.

Suggested source commit:

```text
feat(g1): add operation-specific state validation
```

Record exact SHA as `B7_SOURCE_SHA` and push it before evidence generation.

Then create:

```text
verification/evidence/gates/G1/<B7_SOURCE_SHA>/GT-G1-04-B/B-OP15.json
```

This filename supersedes the older base-plan proposal `B-OP15-stateful.json`; the family name is `B-OP15`.

The artifact must contain:

```text
sourceCommit = testedCommit = B7_SOURCE_SHA
branch
packageRef = this document at its control-plane commit
B-AUTH-02 authority reference
B5 current-valid prerequisite source/evidence references
B6 current-valid prerequisite source/evidence references

implementedHere:
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

delegatedCurrentValid:
  DeleteObjects -> B5 source 36669523... / evidence occurrence af50bb0...
  RestoreObjects -> B6 source 871eec81... / corrected evidence occurrence 68dd235...

per-operation positive/negative case IDs
actual target kind/version where relevant
StatefulIssue
plan-fragment projection
Reference/Indexed parity
output/base no-mutation proof
Indexed rebuild consistency
performance/lookup observations
RED provenance
focused test results
relevant regression results
full semantic result
runtime-boundary result
docs result
git diff --check
scope guards
```

`B-OP15` is an aggregation/evidence family, not a second validator implementation. Delete and Restore MUST be references to accepted B5/B6 evidence, not reimplemented tests or source in B7.

Commit evidence separately:

```text
evidence(g1): bind B7 operation-specific state validation
```

Push normally.

Final executor status may be only:

```text
B7 = READY_FOR_INDEPENDENT_P34_REVIEW
B8 = NOT_YET_RELEASED
GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05 = NOT_AUTHORIZED
```

The executor must not declare `ACCEPTED_FOR_DOWNSTREAM`.

---

## Hard Stops

STOP and return the earliest owning classification if implementation appears to require any of the following:

```text
changing B2/B3/B4/B-AUTH-02 semantics
changing B5/B6
new ObjectKind/FieldId/proto/wire field
new Sticky child-role semantics
SplitStrokes lineage/parentStrokeId
mask global identity index not present in authority
resource availability as semantic validity
RichText grapheme/UTF-16/platform-editor semantics
PreparedApplyPlan
OperationEngine dispatch
Atomic Apply / SemanticGeneration / ChangeSet / History
GT-G1-04-C expected-outcome/golden authoring
GT-G1-05
```

Use Aegis classifications such as `MISSING_CONTRACT`, `AUTHORITY_CONFLICT`, `WIRE_SCHEMA_CHANGE_REQUIRED`, `CROSS_LANE_SCOPE_REQUIRED`, or `IMPLEMENTATION_DEFECT` as appropriate. Do not invent semantics to stay green.

---

## Commit Boundaries

The expected normal lineage is:

```text
<control-plane B7 P31 package commit>
↓
<B7 source/test commit>
↓
<B7 evidence-only commit>
```

No intermediate production commit is required. If a recovery commit is necessary due interruption, preserve it and report the exact lineage; do not rewrite history.

---

## Exit Criteria

B7 may return `READY_FOR_INDEPENDENT_P34_REVIEW` only when all are true:

1. The thirteen exact public validator signatures exist.
2. B2/B3/B-AUTH-02/B4 are reused and unchanged.
3. InsertObjects and SetPlacements validate complete staged hierarchy + capability state.
4. PatchProperties emits one canonical replacement per target and implements released default/Clear semantics.
5. SetObjectSize mutates only the authorized content size fields.
6. AddStroke and SplitStrokes enforce actual stroke kind state; Split does not call B5.
7. Add/RemoveEraseMasks enforce current per-object mask identity state.
8. EditRichText applies ordered steps against staged Unicode-scalar state and is whole-delta atomic.
9. SetConnectorContent delegates endpoint/connectability/StablePort to B4.
10. All thirteen families have Reference/Indexed positive/negative evidence and no canonical mutation.
11. `B-OP15.json` represents all fifteen operations by combining thirteen B7 rows with accepted B5 Delete and B6 Restore references.
12. No production `allObjects()` scan was introduced.
13. B8/PreparedApplyPlan/OperationEngine did not start.
14. Source and evidence commits are both remote-readable.

## Required Final Report

Return exactly enough information for independent P34 to reproduce the review:

```text
GT-G1-04-B B7 P32 RESULT

Starting HEAD
Package ref
Source commit SHA
Evidence commit SHA
Remote HEAD
Files changed per commit

RED command/result/exit code
Public interface signature check

13-family case matrix
B5 Delete delegated evidence ref
B6 Restore delegated evidence ref
15-operation B-OP15 coverage status

Reference/Indexed parity
No mutation / index rebuild
Performance observations

Focused RichText result
Focused B7 result
Relevant B0-B7 regression result
Full semantic result
Runtime boundary result
Docs result
git diff --check result

B2 changed = NO
B3 changed = NO
B-AUTH-02 changed = NO
B4 changed = NO
B5 changed = NO
B6 changed = NO
B8 started = NO
PreparedApplyPlan changed = NO
OperationEngine changed = NO
GT-G1-04-C started = NO
GT-G1-05 changed = NO
History rewritten = NO
Force push = NO

Status:
B7 = READY_FOR_INDEPENDENT_P34_REVIEW
B8 = NOT_YET_RELEASED
GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05 = NOT_AUTHORIZED
```

Do not declare B7 PASS or downstream acceptance; that belongs to independent P34.

# GT-G1-04-B B8 Fresh P31 — PreparedApplyPlan Composition + OperationEngine Planning Facade

> **Status:** P31 READY / PACKAGE MATERIALIZATION.
>
> This document supersedes **only Task Package B8** in
> `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`.
>
> B0-B7 package history, implementation, evidence, and independent review
> outcomes remain unchanged.
>
> **This document does not authorize B8 P32 by itself.**
> P32 requires a subsequent CONTROL_REVIEW authorization bound to the exact
> commit that materializes this file.

## 1. Accepted baseline

```text
B0 = ACCEPTED_FOR_DOWNSTREAM
B1 = ACCEPTED_FOR_DOWNSTREAM
B2 = ACCEPTED_FOR_DOWNSTREAM
B3 = ACCEPTED_FOR_DOWNSTREAM
B4 = ACCEPTED_FOR_DOWNSTREAM
B5 = ACCEPTED_FOR_DOWNSTREAM
B6 = ACCEPTED_FOR_DOWNSTREAM
B7 = ACCEPTED_FOR_DOWNSTREAM

B8 = NEXT_ELIGIBLE
B8 P32 = NOT_AUTHORIZED

B9 = NOT_AUTHORIZED
B10 = NOT_AUTHORIZED
GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05 = NOT_AUTHORIZED
````

B8 remains the first production composition point:

```text
B1 OperationId gate
      +
B5 DeleteObjects closure
      +
B6 RestoreObjects creates
      +
B7 thirteen operation-specific state-plan fragments
      ↓
B8 PreparedApplyPlan + OperationEngine::prepare
      ↓
STOP BEFORE MUTATION
```

B9 remains the owner of Reference / Indexed decision-and-plan differential.

B10 remains the owner of the complete fifteen-operation matrix and final
B-lane no-mutation gate.

Atomic mutation and all post-commit semantics remain outside B8.

---

## 2. Fresh P31 drift findings

### B8-P31-D01 — missing generic delete effect

Accepted B7 exposes:

```cpp
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
```

`SplitStrokes.source_delete_ids` are not B5 DeleteClosure semantics.

They must not be represented as though they were a
DeleteObjects subtree / connector-cascade closure.

Therefore B8 requires one generic prepared delete effect:

```cpp
std::vector<ObjectId> deletes;
```

B5-specific DeleteClosure provenance remains separate.

### B8-P31-D02 — touched_objects / touched_fields removed

Historical B8 included:

```text
touched_objects
touched_fields
```

These do not belong in the B8 pre-commit preparation boundary.

Post-commit semantic impact belongs to the existing ChangeSet contract.

B8 therefore carries prepared ObjectStore effects only and MUST NOT introduce
ChangeSet-like semantic-impact metadata.

### B8-P31-D03 — disposition belongs only to PrepareResult

A PreparedApplyPlan exists only when preparation succeeds.

Keeping a second disposition in PreparedApplyPlan permits contradictory state.

Therefore disposition is owned only by PrepareResult.

---

## 3. Refreshed B8 contract

```cpp
enum class PrepareDisposition : std::uint8_t {
    kPrepared = 0,
    kAlreadyApplied,
    kRejected,
};

struct PreparedApplyPlan final {
    Operation operation{};

    std::vector<ObjectRecord> creates;
    std::vector<ObjectRecord> replacements;
    std::vector<ObjectId> deletes;

    // Present only for DeleteObjects.
    // SplitStrokes source deletion MUST NOT populate this.
    std::optional<DeleteClosure> delete_closure;
};

struct PrepareResult final {
    PrepareDisposition disposition = PrepareDisposition::kRejected;
    StatefulResult error{};
    std::optional<PreparedApplyPlan> plan;
};

[[nodiscard]] PrepareResult prepareApplyPlan(
    const Operation& operation,
    const StatefulValidationContext& context);

class OperationEngine final {
public:
    [[nodiscard]] PrepareResult prepare(
        const Operation& operation,
        const StatefulValidationContext& context) const;
};
```

Required result invariants:

```text
kPrepared
  error.issue == kNone
  plan.has_value() == true

kAlreadyApplied
  error.issue == kNone
  plan.has_value() == false

kRejected
  error.issue != kNone
  plan.has_value() == false
```

No partial plan may escape on rejection.

---

## 4. Mandatory execution order

B8 MUST execute:

```text
normalized + structurally valid typed Operation
        ↓
B1 classifyOperation
        │
        ├─ kAlreadyApplied
        │     → kAlreadyApplied
        │     → error kNone
        │     → no plan
        │     → STOP BEFORE ObjectStore state validation
        │
        ├─ kCollision
        │     → kRejected
        │     → kOperationIdCollision
        │     → no plan
        │     → STOP BEFORE ObjectStore state validation
        │
        └─ kNew
              ↓
        accepted state planning
              ↓
        any StatefulResult failure
              → kRejected
              → exact lower-level issue
              → no plan
              ↓
        success
              ↓
        deterministic PreparedApplyPlan
              ↓
             STOP
```

B8 MUST NOT reimplement:

```text
canonical Operation equality
B1 idempotency policy
B2 shared state validation
B3 hierarchy semantics
B4 connector semantics
B5 DeleteClosure algorithm
B6 Restore eligibility
B7 operation-specific validation
```

---

## 5. Exact composition mapping

### InsertObjects

```text
B7 validateInsertObjectsState
→ plan.creates
```

### DeleteObjects

```text
B5 resolveDeleteClosure
→ plan.deletes = closure.final_delete_set
→ plan.delete_closure = closure
```

Required invariant:

```text
plan.deletes == plan.delete_closure->final_delete_set
```

### RestoreObjects

```text
B6 validateRestoreObjects
→ plan.creates
```

### SetPlacements

```text
B7 validateSetPlacementsState
→ plan.replacements
```

### SetTransforms

```text
B7 validateSetTransformsState
→ plan.replacements
```

### PatchProperties

```text
B7 validatePatchPropertiesState
→ plan.replacements
```

### SetObjectSize

```text
B7 validateSetObjectSizeState
→ plan.replacements
```

### SetVectorPathGeometry

```text
B7 validateSetVectorPathGeometryState
→ plan.replacements
```

### SetImageContent

```text
B7 validateSetImageContentState
→ plan.replacements
```

### AddStroke

```text
B7 validateAddStrokeState
→ plan.creates
```

### SplitStrokes

```text
B7 validateSplitStrokesState
→ plan.deletes = source_delete_ids
→ plan.creates = replacement_creates
→ plan.delete_closure = nullopt
```

This distinction from DeleteObjects is mandatory.

### AddEraseMasks

```text
B7 validateAddEraseMasksState
→ plan.replacements
```

### RemoveEraseMasks

```text
B7 validateRemoveEraseMasksState
→ plan.replacements
```

### EditRichText

```text
B7 validateEditRichTextState
→ plan.replacements
```

### SetConnectorContent

```text
B7 validateSetConnectorContentState
→ plan.replacements
```

No other operation populates `delete_closure`.

---

## 6. Determinism requirements

PreparedApplyPlan MUST:

* own all values;
* contain no mutable ObjectStore reference;
* expose no mutation callback;
* preserve the normalized typed Operation supplied to B8;
* deterministically order prepared effects;
* preserve accepted B5 DeleteClosure partitions exactly;
* not invent new semantic rules while composing outputs.

If a lower-level accepted contract cannot be composed without changing its
semantics, STOP and classify the earliest owning layer.

Do not repair the lower-level contract inside B8.

---

## 7. Future B8 P32 file scope

After a separate P32 authorization, B8 may create only:

```text
runtime/semantic/include/canvas/semantic/apply_plan.hpp
runtime/semantic/src/apply_plan.cpp

runtime/semantic/include/canvas/semantic/operation_engine.hpp
runtime/semantic/src/operation_engine.cpp

runtime/semantic/tests/apply_plan_test.cpp
runtime/semantic/tests/operation_engine_boundary_test.cpp
```

and modify only:

```text
runtime/semantic/CMakeLists.txt
runtime/semantic/tests/CMakeLists.txt
```

After a source/test commit, future B8 evidence belongs at:

```text
verification/evidence/gates/G1/<B8_SOURCE_SHA>/GT-G1-04-B/B-PLAN.json
```

None of this P32 source/test/evidence scope is authorized by the current
documentation-materialization step.

---

## 8. Future RED-first requirements

Future B8 P32 must prove:

1. AlreadyApplied short-circuits before ObjectStore state lookup.
2. collision short-circuits before ObjectStore state lookup.
3. collision maps to kOperationIdCollision.
4. state rejection preserves exact StatefulIssue.
5. rejected result exposes no partial plan.
6. DeleteObjects projects accepted B5 closure exactly.
7. RestoreObjects projects accepted B6 creates exactly.
8. SplitStrokes projects source deletes + replacement creates with
   delete_closure == nullopt.
9. representative B7 create and replacement projections compose correctly.
10. all fifteen Operation kinds have valid dispatch smoke coverage.
11. repeated preparation is deterministic.
12. PrepareResult invariants hold.
13. no mutation / SemanticGeneration / ChangeSet / History / DataBridge /
    Outbox dependency enters B8.

The all-fifteen dispatch check is smoke only.

Full fifteen-operation positive/negative conformance remains B10.

Reference / Indexed decision-and-plan differential remains B9.

---

## 9. Explicit non-goals

B8 does not authorize:

```text
B0-B7 modifications
Atomic Apply
ObjectStore canonical mutation
SemanticGeneration
ChangeSet
touched_objects
touched_fields
CanonicalCommitStamp
History
DataBridge
Outbox
snapshot bootstrap
B9 differential
B10 full operation matrix
GT-G1-04-C
GT-G1-05
```

---

## 10. Current lifecycle status

This document is a P31 package artifact.

After this file is committed and pushed, the code executor MUST STOP and
return the exact commit SHA.

Expected next control transition:

```text
P31 PACKAGE MATERIALIZED
        ↓
CONTROL_REVIEW verifies exact documentation-only commit
        ↓
separate B8 P32 authorization
```

Until that review occurs:

```text
B8 P32 = NOT_AUTHORIZED
```

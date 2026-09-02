# GT-G1-04-B B6 Restore Planner P31 Execution Addendum

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` for P32. Execute RED -> GREEN -> source-bound evidence. Do not redesign semantic authority during implementation.

**Goal:** Freeze the executable B6 boundary for `RestoreObjects` current-state eligibility and complete staged resulting-state validation, while preserving B1 idempotency ownership and avoiding tombstone/history semantics.

**Architecture:** B6 is a read-only state planner that is invoked only for a `RestoreObjects` operation already classified by B1 as `IdempotencyDisposition::kNew`. It checks candidate identity absence by staging each A-normalized candidate into a fresh `StagedObjectView`, validates the complete staged graph with existing B2/B3/B4 primitives, and emits deterministic create inputs only after whole-operation success. B6 does not own `AlreadyApplied` or collision disposition composition; B8 remains the first production `PreparedApplyPlan`/OperationEngine composition point.

**Tech Stack:** C++20, `runtime/semantic`, GoogleTest/CTest, `ReferenceObjectStore`, `IndexedObjectStore`, existing B0-B5 APIs, source-bound JSON evidence.

**Spec / Authority:**

- `docs/notion/authority/04-semantic-schema/02-operation-model/restoreobjects-identity-state-tombstone-eligibility-v1-authority-closure-v0.1.md`
- Notion source `3ca4c57a-590c-8150-b3bd-cb1d51eb0b83` — Current Authority
- `docs/notion/authority/04-semantic-schema/02-operation-model/operation-payload-validation-v0.1.md`
- `docs/notion/authority/04-semantic-schema/01-object-schema/connector-anchor-contract-v1-release-v0.1.md`
- `docs/notion/authority/07-runtime-data-flow/07-08-open-restore-catchup-v0.1.md`
- Base plan `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`

**P31 review base:** branch `codex/gt-g1-04-operation-apply` at `ba10bd2f082824d83d1a2aaa21711458156f1200` with B5 accepted downstream.

---

## 1. P31 Review Findings

### B6-P31-01 — old interface conflated B1 disposition with B6 state validation

The base B6 package proposed:

```cpp
StatefulResult validateRestoreObjects(
    const Operation&,
    const RestoreObjectsOp&,
    const StatefulValidationContext&,
    RestorePlanInputs* out);
```

while also requiring RST-B08 `AlreadyApplied`. Current repository reality separates:

```cpp
IdempotencyResult classifyOperation(
    const Operation&,
    const AppliedOperationView&);
```

from:

```cpp
StatefulResult
```

which cannot represent `AlreadyApplied` without conflating a non-plan disposition with successful state validation.

**Frozen correction:** B1 remains the sole idempotency classifier. B6 public planner is a **new-OperationId state-only function**. RST-B08 is verified as a dependency-ordering case using B1; B8 later composes B1 + B6 into `PrepareDisposition`.

### B6-P31-02 — `staged_connector_targets` has no authority-backed consumer

The old `RestorePlanInputs` included `staged_connector_targets`, but B8 consumes restore `creates` and no current authority requires a separate target-ID projection.

**Frozen correction:** do not invent this plan field. `RestorePlanInputs` contains deterministic `creates` only. Connector target/reference details belong in B-RESTORE evidence and are validated by B4 against the complete staged view.

### B6-P31-03 — identity lookup must not be doubled

`StagedObjectView::stageCreate()` already calls `contains()/find()` and therefore performs one base identity lookup for a fresh A-normalized unique candidate. A separate `requireAbsent()` pass followed by `stageCreate()` would double candidate base lookups.

**Frozen correction:** use a fresh `StagedObjectView` and call `stageCreate(record)` once per candidate. Under the B6 precondition that A has normalized candidate IDs to sorted/unique/non-zero and this is a fresh overlay with no staged deletes/replacements, `stageCreate()==false` maps to `StatefulIssue::kObjectAlreadyExists`.

### B6-P31-04 — ApplySource must not be invented in B6

`Operation` and the current state planner APIs contain no ApplySource. Authority says source labels must not alter Restore acceptance.

**Frozen correction:** do not add `ApplySource` to `Operation`, `StatefulValidationContext`, or B6. RST-B11 is boundary evidence: identical canonical operation + identical base state yields identical B6 output, and test-only LocalUndo/Replay/Remote labels are not passed into production validation.

### B6-P31-05 — Restore runtime must not reconstruct delete provenance

Authority treats “previously removed” as producer intent, not a hidden runtime historical predicate.

**Frozen correction:** B6 must not call `resolveDeleteClosure()`, inspect B5 evidence/history, require a prior Delete operation, infer a cascade provenance, or create tombstone/deletion-generation state. Target + Connector payload records are validated solely as the resulting staged graph.

---

## 2. Frozen B6 Public Interface

Create:

`runtime/semantic/include/canvas/semantic/restore_planner.hpp`

with:

```cpp
#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <vector>

namespace canvas::semantic {

struct RestorePlanInputs final {
    std::vector<ObjectRecord> creates;
};

[[nodiscard]] StatefulResult validateRestoreObjects(
    const RestoreObjectsOp& restore,
    const ObjectStore& apply_base,
    RestorePlanInputs* out);

} // namespace canvas::semantic
```

### Normative preconditions

The caller guarantees:

1. A-lane decode/envelope/payload normalization and structural validation already passed.
2. `restore.objects` is non-empty if required by A, contains non-zero unique ObjectIds, and is sorted by canonical ObjectId identity order.
3. B1 has already classified the full `Operation` as `IdempotencyDisposition::kNew`.
4. `apply_base` is the current canonical ObjectStore apply-base state.

B6 MUST NOT re-run A normalization or B1 classification inside this function.

### Output semantics

On success:

```text
out->creates == restore.objects
```

with exact normalized ordering and full `ObjectRecord` values preserved.

On any failure, `*out` remains unchanged.

No other public output projection is authorized in B6.

---

## 3. Frozen Execution Algorithm

Conceptual algorithm:

```text
local RestorePlanInputs result
fresh StagedObjectView staged(apply_base)

for candidate in restore.objects, normalized ObjectId order:
    if !staged.stageCreate(candidate):
        return kObjectAlreadyExists

for candidate in restore.objects, normalized ObjectId order:
    validateRecordStateForOperation(candidate, StateRule::kCreateAbsent)
    on failure: return existing B2 issue

hierarchy_edits = [(candidate.id, candidate.placement) for every candidate]
validateStagedHierarchy(staged, hierarchy_edits)
on failure: return existing B3 issue

for candidate in restore.objects, normalized ObjectId order:
    if candidate.kind == Connector:
        validateConnectorReferences(staged, ConnectorContent)
        on failure: return existing B4 issue

result.creates = restore.objects
*out = move(result) only on success
return kNone
```

### Required behavior

- All candidate records are staged before hierarchy or Connector reference validation.
- Parent/child order inside the payload therefore does not affect validity.
- Connector/target ObjectId ordering inside the payload therefore does not affect validity.
- Existing valid parent/target references may be resolved through `apply_base`.
- A restored Connector may reference a target restored in the same payload.
- Connector alone with absent AttachedEndpoint target returns `kInvalidReference`.
- Same-record and different-record identity collisions both return `kObjectAlreadyExists`.
- No base store mutation occurs.
- No `ObjectStoreMutator` call is permitted.

### Validation precedence boundary

The only newly frozen precedence is:

```text
B1 idempotency
-> candidate identity absence/staging
-> existing B2/B3/B4 staged-state validators
```

This addendum does not create new normative precedence among unrelated B2/B3/B4 defects. Negative fixtures should isolate one intended state failure rather than encode multi-defect precedence as authority.

---

## 4. RST-B01..RST-B12 Required RED / Oracle Matrix

Create:

`runtime/semantic/tests/restore_stateful_validation_test.cpp`

The test suite must expose case IDs in test names or evidence mapping.

### RST-B01 — all absent / valid

- Base: candidate IDs absent.
- Payload: structurally valid normalized records.
- Expected: `kNone`, `creates == payload objects`.

### RST-B02 — existing same record

- Base already contains byte/semantic-equivalent candidate record.
- Expected: `kObjectAlreadyExists`; no output change; no overwrite.

### RST-B03 — existing different record

- Base contains same ObjectId with different valid content.
- Expected: `kObjectAlreadyExists`; identical category to B02.

### RST-B04 — parent + child staged together

- Both IDs absent in base.
- Child parent_id refers to restored parent.
- Expected: success after complete staged hierarchy validation.

### RST-B05 — child / parent absent

- Child candidate refers to parent absent from both payload and base.
- Expected: `kInvalidReference` from B3 hierarchy validation.

### RST-B06 — target + Connector staged together

- Restore a V1-connectable Shape/Image/Sticky target plus a Connector with valid AttachedEndpoint/anchor.
- Expected: success using B4 against the complete staged view.

### RST-B07 — Connector alone / target absent

- Expected: `kInvalidReference`.

### RST-B08 — equivalent OperationId replay stops before existence

This is a B1/B6 dependency-ordering test, not a B6-state return value.

- Store an equivalent canonical Restore `Operation` in an `AppliedOperationView` fixture.
- `classifyOperation()` must return `kAlreadyApplied`.
- The restore state planner MUST NOT be invoked.
- Instrumented ObjectStore must report zero ObjectStore lookups for this ordering case.
- Do not invent `AlreadyApplied` inside `StatefulResult`.

### RST-B09 — new OperationId after prior restore

- AppliedOperationView has no entry for incoming new OperationId, so B1 returns `kNew`.
- Seed apply-base with the candidate record as the state left by a prior successful restore.
- B6 returns `kObjectAlreadyExists`.

### RST-B10 — one collision rejects entire batch

- Multi-record payload with one existing candidate ID.
- Expected: `kObjectAlreadyExists`; `*out` unchanged; base projection unchanged; no partial plan.

### RST-B11 — ApplySource parity by construction

- Do not add a production source parameter.
- Run the same operation/base-state fixture under test-only labels `LocalUndo`, `Replay`, `Remote` (or equivalent evidence grouping) without passing the label to B6.
- Expected B6 result and `RestorePlanInputs` are identical.
- Evidence must explicitly state that source labels are outside the B6 semantic API.

### RST-B12 — snapshot checkpoint without deleted-ID ledger

- Seed a checkpoint-like canonical ObjectStore with unrelated valid records and no tombstone/deleted-ID state.
- Candidate IDs are absent and staged graph is valid.
- Expected: success.
- Do not implement `restore_snapshot()` or snapshot metadata in B6.

---

## 5. Reference / Indexed Differential

Every state-owning RST case must be run against both:

- `ReferenceObjectStore`
- `IndexedObjectStore`

Compare:

```text
StatefulIssue
RestorePlanInputs.creates ordered ObjectRecord projection
base before/after projection
```

For Indexed success/failure fixtures, also prove `ObjectStoreMutator::indexMatchesRebuild(indexed)` after the read-only planner call.

The independent expected result comes from the RST authority fixture definition, not from production planner output.

---

## 6. Performance / Lookup Contract

B6 MUST NOT call `allObjects()` or perform an unrelated full-store scan during validation.

Use a CountingObjectStore or equivalent test wrapper to record at least:

```text
base find calls by ObjectId
base allObjects calls during planner execution
```

For a self-contained payload where restored parent/target references are also staged:

- each candidate identity causes exactly one base `find` through `stageCreate()`;
- candidate identity checks are not preceded by a separate `requireAbsent()` pass;
- staged parent/Connector-target resolution should hit the overlay, not rescan the base;
- `allObjects()` during planner execution == 0.

Fixtures that intentionally reference an already-existing parent/target may add required base reference lookups; evidence must distinguish these from candidate identity lookups rather than claiming all `find()` calls equal candidate count globally.

Do not modify ObjectStore/IndexedObjectStore/ObjectIndex to satisfy this package.

---

## 7. Authorized File Scope

### Create

```text
runtime/semantic/include/canvas/semantic/restore_planner.hpp
runtime/semantic/src/restore_planner.cpp
runtime/semantic/tests/restore_stateful_validation_test.cpp
```

### Minimal modify

```text
runtime/semantic/CMakeLists.txt
runtime/semantic/tests/CMakeLists.txt
```

### Evidence-only after source commit

```text
verification/evidence/gates/G1/<B6_SOURCE_SHA>/GT-G1-04-B/B-RESTORE.json
```

### Do not modify in B6

```text
runtime/semantic/include/canvas/semantic/idempotency.hpp
runtime/semantic/src/idempotency.cpp
runtime/semantic/include/canvas/semantic/stateful_validation.hpp
runtime/semantic/include/canvas/semantic/staged_object_view.hpp
runtime/semantic/src/staged_object_view.cpp
runtime/semantic/include/canvas/semantic/operation_state_validator.hpp
runtime/semantic/src/operation_state_validator.cpp
runtime/semantic/include/canvas/semantic/hierarchy_validation.hpp
runtime/semantic/src/hierarchy_validation.cpp
runtime/semantic/include/canvas/semantic/connector_validation.hpp
runtime/semantic/src/connector_validation.cpp
runtime/semantic/include/canvas/semantic/delete_closure.hpp
runtime/semantic/src/delete_closure.cpp
ObjectStore / ReferenceObjectStore / IndexedObjectStore / ObjectIndex
schema/protobuf/wire files
```

If implementation appears to require modifying any frozen dependency above, STOP and return the owning blocker instead of widening B6.

---

## 8. Explicit Non-goals / Hard Stops

B6 does NOT implement or authorize:

- tombstone ledger / deleted-ID ledger;
- deletion epoch or generation;
- proof that an ObjectId was historically deleted;
- B5 delete-closure reconstruction;
- Editor History lookup or before-image storage;
- Snapshot bootstrap / `restore_snapshot()`;
- Shared Data Runtime / Inbox / Outbox / server cursor / revision logic;
- `ApplySource` API changes;
- `PreparedApplyPlan` composition;
- `OperationEngine`;
- ObjectStore mutation;
- SemanticGeneration / ChangeSet / CanonicalCommitStamp;
- B7+ implementation;
- GT-G1-04-C;
- GT-G1-05.

Hard-stop classifications:

- Need for tombstone/history/sync eligibility -> `AUTHORITY_CONFLICT` or `MISSING_CONTRACT`.
- Need to change B1/B2/B3/B4/B5 frozen semantics -> stop and route owning layer.
- Need ObjectStore ABI/index expansion -> `MISSING_CONTRACT` / scope blocker.
- Need Atomic Apply or post-commit state -> `GT_G1_05_REQUIRED` / downstream scope blocker.

---

## 9. RED -> GREEN Execution

P32 must:

1. Write B6 tests first.
2. Run the focused B6 target and capture genuine RED because `restore_planner.hpp` / target does not yet exist.
3. Implement the smallest production planner satisfying this addendum.
4. Run focused B6 tests.
5. Run relevant B0-B5 regressions.
6. Run full semantic CTest.
7. Run runtime-boundary and docs checks.
8. Run `git diff --check`.

Do not weaken existing tests.

---

## 10. Source / Evidence Commit Boundary

After GREEN verification, create a source/test commit containing only authorized source/test/build-registration files.

Expected message:

```text
feat(g1): plan restore identity and staged-state validation
```

Record exact SHA as `B6_SOURCE_SHA`.

Then create source-bound evidence at:

```text
verification/evidence/gates/G1/<B6_SOURCE_SHA>/GT-G1-04-B/B-RESTORE.json
```

Commit evidence separately, e.g.:

```text
evidence(g1): bind GT-G1-04-B B6 restore planner
```

Never amend source after binding evidence. If source/test changes, create a new source snapshot and rerun required evidence.

---

## 11. B-RESTORE Evidence Contract

The evidence JSON must record:

- `sourceCommit == testedCommit == B6_SOURCE_SHA`;
- branch;
- base B plan ref;
- this B6 P31 addendum ref;
- B6 P31 authorization record;
- authority mirror path + blob/hash and Notion source page ID;
- RST-B01..RST-B12 individually;
- B1 disposition for RST-B08/RST-B09;
- exact `StatefulIssue` for B6 state cases;
- normalized candidate ObjectIds and `creates` projection;
- parent edges and Connector endpoint/anchor fixtures where applicable;
- Reference/Indexed parity;
- no-mutation before/after proof;
- Indexed index-rebuild match;
- candidate identity lookup counts;
- required reference lookup counts where present;
- planner `allObjects()` count;
- source-independence boundary for RST-B11;
- explicit absence of tombstone/history/sync inputs for RST-B12;
- exact RED command/result;
- focused/regression/full commands and exact pass counts;
- runtime-boundary/docs/diff-check results;
- scope guard proving B7+, PreparedApplyPlan, OperationEngine, GT-G1-04-C, and GT-G1-05 were not started.

No executor self-report alone is sufficient: source and evidence commits must be pushed to the reviewer-accessible remote branch before returning P32.

---

## 12. P32 Exit / Return Contract

P32 may return only:

```text
B6 = READY_FOR_INDEPENDENT_P34_REVIEW
```

or an explicit `BLOCKED_*` status.

P32 MUST NOT declare B6 accepted downstream.

Required report fields:

```text
GT-G1-04-B B6 P32 RESULT

Starting HEAD:
Branch:
B6 P31 package_ref:
B6 P31 authorization:

RED command/result:
Files created/modified:

RST-B01: ...
...
RST-B12: ...

B1 ordering evidence:
Reference/Indexed parity:
No mutation / index rebuild:
Candidate identity lookup counts:
Required reference lookup counts:
allObjects during planner:

Focused B6:
Relevant B0-B5 regressions:
Full semantic:
Runtime boundaries:
Docs:
git diff --check:

B6 source commit:
B-RESTORE path:
B6 evidence commit:
Remote HEAD:
Reviewer-accessible materialized_ref:

B7+ changed: NO
PreparedApplyPlan changed: NO
OperationEngine changed: NO
GT-G1-04-C started: NO
GT-G1-05 changed: NO
History rewritten: NO
Force push: NO
Merge/PR: NO

Status:
B6 = READY_FOR_INDEPENDENT_P34_REVIEW | BLOCKED_*
B7 = NOT_YET_RELEASED
GT-G1-04-C = DEFERRED
GT-G1-05 = NOT_AUTHORIZED
```

---

## 13. P31 Exit Criteria

B6 P31 is READY when:

- Current Restore authority is reviewer-accessible and agrees with the repository mirror on current-state eligibility, no tombstone, idempotency-before-state, staged parent/child + target/Connector validation, source parity, and snapshot boundary.
- B1/B6 disposition ownership is explicit.
- B6 public API cannot misrepresent `AlreadyApplied` as `StatefulResult::kNone`.
- one-look-up candidate staging strategy is explicit.
- all RST-B01..B12 have executable evidence obligations.
- Reference/Indexed/no-mutation/performance requirements are explicit.
- P32 source/evidence materialization boundary is explicit.
- B7+ and GT-G1-05 remain unreleased.

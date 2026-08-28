# GT-G1-04-B / B5 P31 Delete Closure Execution Addendum

**Stage:** P31 Task Packaging — targeted review/repair for B5 only.

**Purpose:** Close the execution ambiguity in B5 before P32 by binding DeleteObjects subtree semantics, Connector reverse lookup, combined fixed-point execution, deterministic output partitioning, verification, and evidence to the actual B0–B4 repository interfaces.

**Base implementation plan:** `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md@6357f7e19426f8c7300c11595c26dbb0cf13d8ae`.

**Repository reality reviewed through:** `3cca2b2db7a8844526e100213e29df9805b7927f` on `codex/gt-g1-04-operation-apply`.

**B4 downstream authority:** B4 tested source `1549b5c42b39ae677220b460dd2d63025fb22326`; corrected evidence lineage through `3cca2b2db7a8844526e100213e29df9805b7927f`; independent P34 evidence re-review recorded in Notion page `3ca4c57a-590c-8186-be74-dc36072c5d10`.

## 1. Frozen Authority Inputs

B5 consumes, and must not redesign, these existing contracts:

- `DeleteObjects` uses canonical hierarchy subtree delete semantics.
- V1 target deletion uses Cascade Connector Delete: if a Connector AttachedEndpoint references any object in the resolved delete set, that Connector is deleted as part of the same resolved plan.
- Delete must not reject an ordinary target merely because a Connector references it.
- Delete must not AutoDetach an endpoint or synthesize a canonical FreePoint.
- `DeleteObjects` payload remains requested ObjectIds only; the resolved delete set is derived apply-plan state.
- Connector-to-Connector attachment is invalid in V1 because Connector is not Connectable.
- Canonical mutation, ChangeSet, history before-images, generation, publication, and persistence are not B5 responsibilities.
- B5 consumes an A-normalized, structurally valid DeleteObjects payload. `object_ids` are non-zero, sorted by ObjectId identity, and unique before B5 runs.

The combination of subtree semantics and Connector cascade is interpreted as one monotone delete-closure problem. B5 computes the **least fixed point closed under both hierarchy-child edges and target->Connector reverse-reference edges**. This is not a new product rule; it is the deterministic composition of the two already-frozen delete invariants.

## 2. Current Repository Reality

The current repository exposes:

- `StagedObjectView::find(id)` for resulting-state visibility.
- `StagedObjectView::children(parent)` for deterministic staged hierarchy traversal.
- `StagedObjectView::allObjects()` for a deterministic resulting-state projection.
- `resolveDescendants(staged, root)` with strict-descendant semantics.
- `ObjectIndex` / `IndexedObjectStore` accelerate only parent->children hierarchy lookup.
- There is **no current Connector target->Connector reverse index** in ObjectStore, IndexedObjectStore, ObjectIndex, B4, Scene, SpatialIndex, or renderer state.
- `ConnectorContent` contains two endpoints; an AttachedEndpoint contains `target_object_id` and an anchor.

Therefore B5 must not assume a reverse Connector relation already exists, and it must not silently widen B5 by modifying shared ObjectStore/ObjectIndex architecture.

## 3. P31 Decision — Reverse Lookup Ownership

B5 owns an **operation-local, read-only Connector reverse relation** built inside `delete_closure.cpp`.

No new public reverse-index ABI is authorized.

No modification to these existing modules is authorized by B5 P32:

- `ObjectStore`
- `ReferenceObjectStore`
- `IndexedObjectStore`
- `ObjectIndex`
- `StagedObjectView`
- `hierarchy_validation.*`
- `connector_validation.*`

The implementation may define a private helper/type inside `runtime/semantic/src/delete_closure.cpp`, conceptually equivalent to:

```text
TargetObjectId -> sorted unique Connector ObjectIds
```

The helper is derived once from the current `StagedObjectView` projection and discarded after the B5 call. It is not canonical state, not serialized state, and not a long-lived cache.

### 3.1 Construction contract

Build the reverse relation exactly once per `resolveDeleteClosure()` call from the resulting staged projection:

```text
staged.allObjects()
  -> keep visible ObjectKind::kConnector v1 records
  -> inspect ConnectorContent.start / end
  -> for each AttachedEndpoint(targetObjectId)
       add connectorId under targetObjectId
  -> ignore FreePoint endpoints
  -> deduplicate connectorId when both endpoints reference the same target
  -> sort/deduplicate each adjacency list by ObjectId identity
```

`StagedObjectView::allObjects()` is allowed in B5 **only for this one operation-local Connector relation materialization**. It must not be called once per fixed-point iteration.

This full projection scan is a B5 V1 correctness path, not a precedent that relaxes B4's no-scan rule. A future optimization may introduce a maintained derived Connector reverse index only through a separately reviewed architecture/optimization task.

### 3.2 Preconditions

B5 consumes canonical/staged state that has already satisfied the B2/B4 kind/content/referential invariants. It must not invent a second malformed-Connector semantic classification. If implementation reality shows B5 cannot safely rely on those upstream invariants, stop with `MISSING_CONTRACT` rather than adding a new B5 error category.

## 4. Frozen B5 Public Interface

Create only:

```text
runtime/semantic/include/canvas/semantic/delete_closure.hpp
runtime/semantic/src/delete_closure.cpp
runtime/semantic/tests/delete_closure_test.cpp
```

Modify only minimal build/test registration:

```text
runtime/semantic/CMakeLists.txt
runtime/semantic/tests/CMakeLists.txt
```

Public output remains:

```cpp
struct DeleteClosure {
    std::vector<ObjectId> requested_delete_ids;
    std::vector<ObjectId> resolved_hierarchy_closure;
    std::vector<ObjectId> resolved_connector_cascade_closure;
    std::vector<ObjectId> final_delete_set;
};

StatefulResult resolveDeleteClosure(
    const StagedObjectView& staged,
    std::span<const ObjectId> requested_ids,
    DeleteClosure* out);
```

Do not add canonical mutation callbacks, ChangeSet fields, generation, history records, commit stamps, publication hooks, or OperationEngine behavior.

## 5. Output Semantics

All output vectors use deterministic ascending `ObjectId` identity ordering and contain unique IDs.

### `requested_delete_ids`

Exact A-normalized DeleteObjects payload IDs. B5 does not add or remove IDs from this field.

### `resolved_hierarchy_closure`

IDs first added to the fixed point by a canonical hierarchy child edge, excluding IDs already present in `requested_delete_ids` and excluding IDs already assigned a stronger first-discovery reason.

This field therefore contains strict hierarchy additions, not the requested roots themselves.

### `resolved_connector_cascade_closure`

Connector IDs first added because an AttachedEndpoint target was already in the current delete set, excluding IDs already present as requested or hierarchy additions.

A Connector that is directly requested is not reported again as a cascade addition. A Connector already included by hierarchy is not reported again as a cascade addition.

### `final_delete_set`

Sorted unique union of requested IDs, hierarchy additions, and Connector-cascade additions at the least fixed point.

The reason partitions are diagnostic/apply-plan projections; only `final_delete_set` expresses the complete deletion membership.

## 6. Missing Target and Failure Atomicity

Validate requested IDs in their already-normalized deterministic order before producing a successful closure.

For the first requested ID not visible through `StagedObjectView`:

```text
return StatefulIssue::kObjectMissing
```

B5 must construct the result in local temporary state and assign `*out` only on success. A failure must not expose a partially populated DeleteClosure.

B5 does not reject a Connector because its target is being deleted; that Connector is included in the closure instead.

## 7. Combined Fixed-Point Execution

The normative execution model is a worklist/wave fixed point over two directed relations:

```text
hierarchy edge: parent -> child
connector edge: target -> referencing Connector
```

Initialize:

```text
S = requested_delete_ids
frontier = requested_delete_ids
```

Then repeat until `frontier` is empty:

1. For every ID in the current frontier, query `staged.children(id)` and collect previously unseen children as hierarchy additions.
2. Add those hierarchy additions to `S`.
3. For every ID newly relevant in this wave (the previous frontier plus the hierarchy additions), query the operation-local reverse Connector relation and collect previously unseen referencing Connectors as cascade additions.
4. Add those Connector additions to `S`.
5. The next frontier is the sorted unique set of all IDs added during this wave.

Equivalent single-worklist implementations are permitted only if they produce the same deterministic membership and reason partition.

### 7.1 Why hierarchy participates in the fixed point

A cascaded Connector is still an ObjectRecord in the canonical hierarchy. Current B3 authority intentionally does not invent an ObjectKind parent-capability matrix. Therefore a valid current document cannot assume that a cascaded Connector has no children merely because product UX normally treats Connectors as non-containers.

Closing the final delete set under hierarchy edges after Connector additions prevents an orphan if a cascaded object has descendants, without inventing any new parent-capability rule.

### 7.2 V1 Connector constraint

Do **not** create Connector-to-Connector AttachedEndpoint fixtures. Connector is not Connectable in V1.

The combined fixed point can still require multiple waves without illegal Connector attachment. Example:

```text
Delete Shape A
  -> Connector C1 references A, so C1 cascades
  -> Shape B is a hierarchy child of C1, so B enters via subtree semantics
  -> Connector C2 references B, so C2 cascades
```

This is the preferred multi-wave fixed-point fixture because all AttachedEndpoint targets remain V1-connectable Shapes.

## 8. Performance Contract

B5 must have bounded traversal behavior:

- requested single-ID existence uses staged `find` / `requireExisting` semantics;
- hierarchy expansion uses `StagedObjectView::children(parent)` and therefore the existing IndexedObjectStore hierarchy index for base records;
- each ID admitted to the delete set is processed as a hierarchy parent at most once by the B5 worklist;
- the staged full projection is scanned at most once to materialize the operation-local Connector reverse relation;
- fixed-point iterations do not call `allObjects()` again;
- each reverse adjacency lookup is against the operation-local map/index;
- no Scene, SpatialIndex, renderer, Skia, route resolver, or world-space geometry may participate.

Required instrumentation must distinguish:

```text
base allObjects call count
base children call count
base find call count
fixed-point wave count
reverse relation lookups
```

The P32 evidence must prove that adding fixed-point waves does not multiply full-store scans.

## 9. Required RED / Oracle / Differential Cases

Before implementation, add B5 tests that fail because the B5 interface/behavior does not yet exist.

At minimum cover:

1. `DeleteDirectTargetCascadesConnector`
   - requested Shape A
   - Connector C1 attached to A
   - final = A + C1

2. `DeleteDescendantTargetCascadesExternalConnector`
   - requested Group/parent G
   - descendant Shape A
   - external Connector C1 attached to A
   - final = G + A + C1

3. `MultipleConnectorsReferenceOneDeletedTarget`
   - C1/C2 both reference A
   - both cascade exactly once

4. `TwoEndpointsHitDeleteSetDeduplicatesConnector`
   - one Connector has both AttachedEndpoints targeting objects in the delete set
   - connector appears once

5. `FreeFreeConnectorIsUnrelated`
   - free/free Connector never enters reverse relation

6. `DirectlyRequestedConnectorIsNotDoubleReportedAsCascade`

7. `HierarchyIncludedConnectorIsNotDoubleReportedAsCascade`

8. `MissingRequestedTargetReturnsObjectMissingAndLeavesOutUnchanged`

9. `InputAlreadyNormalizedOrderProducesDeterministicOutput`
   - output vectors are sorted unique and independent of store insertion order

10. `RepeatedEvaluationProducesIdenticalClosure`

11. `ReferenceAndIndexedStoresProduceIdenticalDeleteClosure`

12. `CombinedFixedPointNeedsMultipleWavesWithoutConnectorToConnectorAttachment`
   - A Shape requested
   - C1 references A
   - B Shape is hierarchy child of C1
   - C2 references B
   - final includes A, C1, B, C2
   - evidence records hierarchy and Connector additions by wave

13. `StagedCreateReplacementDeleteAreObservedByReverseRelation`
   - resulting staged Connector content/visibility, not stale base content, determines cascade

14. `NoCanonicalMutation`
   - base projections unchanged
   - staged projection unchanged
   - IndexedObjectStore index still matches rebuild

15. `OneProjectionScanRegardlessOfFixedPointWaves`
   - runtime instrumentation proves one base/staged full projection scan for reverse-relation materialization and no per-wave full scan

The independent oracle should model the graph as test-only adjacency sets derived from fixture declarations, not by calling `resolveDeleteClosure()` or the production reverse-map builder to obtain expected values.

## 10. Evidence Contract — B-DELETE

Create source-bound evidence only after the B5 source commit exists:

```text
verification/evidence/gates/G1/<B5_SOURCE_SHA>/GT-G1-04-B/B-DELETE.json
```

Source and evidence commits remain separate occurrences.

B-DELETE must record:

- `sourceCommit` and `testedCommit` = exact B5 source SHA;
- branch;
- B5 P31 authorization record;
- base plan commit and this P31 addendum commit;
- requested IDs;
- concrete hierarchy edges used by each case;
- concrete Connector IDs and endpoint target IDs;
- per-wave `hierarchyAdditions` and `connectorAdditions`;
- the four DeleteClosure vectors;
- expected vs actual outcome;
- Reference/Indexed parity;
- no-mutation/index-rebuild proof;
- source inspection versus runtime instrumentation provenance;
- full projection scan count and children/find/reverse lookup measurements;
- exact configure/build/test commands and results;
- scope guard proving B6+ / PreparedApplyPlan / OperationEngine / GT-G1-05 were not started.

Do not fabricate a multi-wave case with illegal Connector-to-Connector attachment.

## 11. P32 Non-goals / Hard Stops

B5 P32 must stop rather than widen scope if implementation appears to require:

- changing ObjectStore ABI;
- changing IndexedObjectStore or ObjectIndex to add a maintained Connector reverse index;
- changing B3 hierarchy semantics;
- changing B4 connectability or Connector validation semantics;
- introducing a parent-capability matrix;
- allowing Connector-to-Connector attachment;
- mutating canonical state;
- creating ChangeSet/history/generation/publication behavior;
- starting RestoreObjects B6;
- starting PreparedApplyPlan / OperationEngine;
- starting GT-G1-05;
- changing schema/protobuf/wire contracts.

Use `MISSING_CONTRACT`, `AUTHORITY_CONFLICT`, or the appropriate Aegis blocked status rather than inventing behavior.

## 12. P32 Commit Boundary and Return Contract

Expected source commit message:

```text
feat(g1): resolve delete subtree connector closure
```

The source commit contains B5 source/tests/build registration only. B-DELETE evidence is committed separately after fresh verification.

P32 must return:

```text
GT-G1-04-B B5 P32 RESULT

Starting HEAD
B5 authorization record
B5 addendum commit
Files changed
RED evidence
Focused B5 result
Relevant semantic regression result
Reference/Indexed differential
Combined fixed-point multi-wave case
No-mutation result
Projection scan / children / find / reverse-lookup measurements
Exact commands
B5 source commit
B-DELETE evidence path
B5 evidence commit
B6+ files changed: yes/no
PreparedApplyPlan/OperationEngine changed: yes/no
GT-G1-05 changed: yes/no
Status: READY_FOR_INDEPENDENT_P34_REVIEW | BLOCKED_*
```

P32 must not declare B5 accepted for downstream. Only independent P34 may release B5.
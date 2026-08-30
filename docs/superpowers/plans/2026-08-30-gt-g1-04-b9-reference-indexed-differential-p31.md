# GT-G1-04-B B9 Fresh P31 — ReferenceObjectStore / IndexedObjectStore Differential

> **Status:** P31 READY / PACKAGE MATERIALIZATION.
>
> This document supersedes **only Task Package B9** in
> `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`.
>
> B0-B8 accepted implementation, evidence, and review outcomes remain unchanged.
>
> **This document does not authorize B9 P32 by itself.**
> P32 requires a subsequent CONTROL_REVIEW authorization bound to the exact
> commit that materializes this file.

## 1. Accepted baseline and repository anchor

Fresh repository reconciliation before this package observed:

```text
repo:   Mostorm-Labs/axiom
branch: codex/gt-g1-04-operation-apply
expected HEAD: bc80d0a3c1376b0334f79d8e2f5e7588cefff645
observed HEAD: bc80d0a3c1376b0334f79d8e2f5e7588cefff645
result: FRESH_BASELINE_MATCH
```

No new remote commit, branch divergence, or repository-tree drift exists relative
to the expected B8 materialized evidence ref.

Lifecycle state inherited without re-review:

```text
B0-B7 = ACCEPTED_FOR_DOWNSTREAM / UNCHANGED

B8 P31 = READY / MATERIALIZED / CORROBORATED
B8 P36 = REPAIRED / REVERIFIED
B8 P34 = PASS_WITH_FINDINGS
B8 = ACCEPTED_FOR_DOWNSTREAM

B9 = NEXT_ELIGIBLE / NOT_EXECUTED
B9 P32 = NOT_AUTHORIZED

B10 = NOT_AUTHORIZED
GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05 = NOT_AUTHORIZED
```

Accepted B8 refs:

```text
B8 P31 package_ref:
534ce78a4edd0d275890ce68eb726e3d925fc06b

B8 final tested source:
3ccc8fa33d526e8db6e643c1925dab7dd247af1c

B8 final materialized evidence / B9 trusted repository anchor:
bc80d0a3c1376b0334f79d8e2f5e7588cefff645

B8 final evidence:
verification/evidence/gates/G1/3ccc8fa33d526e8db6e643c1925dab7dd247af1c/GT-G1-04-B/B-PLAN.json
```

B0-B7 individual historical refs remain inherited through the accepted B8
baseline and are not reopened, rewritten, or re-audited by B9.

Repository-backed execution contract for future B9 P32:

```yaml
task_anchor:
  revision: bc80d0a3c1376b0334f79d8e2f5e7588cefff645
  relation: ancestor
resume_cursor: null
```

`resume_cursor` is null because B9 has not executed.

---

## 2. Current Authority binding

B9 is constrained by the following already-established repository authority and
accepted implementation contracts:

1. `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`
   - B8 -> B9 DAG edge.
   - B9 purpose, oracle, trace classes, mismatch diagnostics, evidence family,
     lookup instrumentation, and exit criteria.
   - `B-DIFF` is limited to B state decisions and plan projections and is not
     GT-G1-04-C golden authority.
2. `docs/superpowers/plans/2026-08-26-g1-semantic-kernel-codex-package.md`
   - GT-G1-03 establishes `ReferenceObjectStore` as the permanent differential
     oracle.
   - GT-G1-03 establishes Indexed lookup/full-scan constraints.
   - GT-G1-04 consumes ReferenceObjectStore as the state lookup oracle.
3. `docs/superpowers/plans/2026-08-30-gt-g1-04-b8-prepared-apply-plan-p31.md`
   at package ref `534ce78a4edd0d275890ce68eb726e3d925fc06b`.
   - This is the accepted refreshed B8 `PreparedApplyPlan` projection contract.
   - It supersedes the historical B8 field shape without changing B9 ownership.
4. Accepted B0-B8 source/evidence lineage represented by the trusted anchor and
   B8 refs in section 1.

Repository implementation reality confirms the accepted B8 plan shape is:

```cpp
struct PreparedApplyPlan final {
    Operation operation{};
    std::vector<ObjectRecord> creates;
    std::vector<ObjectRecord> replacements;
    std::vector<ObjectId> deletes;
    std::optional<DeleteClosure> delete_closure;
};
```

and `DeleteClosure` retains exactly:

```text
requested_delete_ids
resolved_hierarchy_closure
resolved_connector_cascade_closure
final_delete_set
```

No new semantic authority is introduced by this package.

If execution discovers that this oracle or comparison boundary materially
conflicts with Current Authority, stop with `BLOCKED_AUTHORITY` and route the
earliest untrusted layer. Do not repair Authority inside B9.

---

## 3. B9 objective

B9 proves one narrowly scoped statement:

> For identical canonical base state, identical applied-operation lookup state,
> and the same normalized Operation input, `OperationEngine::prepare` produces
> equivalent B-lane state decisions and equivalent current
> `PreparedApplyPlan` projections when backed by `ReferenceObjectStore` versus
> `IndexedObjectStore`.

Why this oracle is valid:

- GT-G1-03 explicitly establishes `ReferenceObjectStore` as the permanent
  correctness-first differential oracle for ObjectStore behavior.
- B9 does not ask ReferenceObjectStore to define product intent, final error
  taxonomy, commit behavior, or C golden outcomes.
- The compared SUT boundary is B read-only planning only.

B9 therefore answers:

```text
Does the production-oriented IndexedObjectStore change B's decision or plan
projection compared with the accepted reference store for the same input?
```

It does **not** answer:

```text
Is every semantic outcome independently correct for every positive/negative
operation case?
```

That latter full matrix remains B10 / later independent conformance authority.

---

## 4. Differential contract

### Input

Each observation receives the same:

```text
canonical bootstrap records
applied-operation fixture
normalized typed Operation
fixture/trace identifier
fixed deterministic seed when randomized selection is used
```

Canonical records are seeded into each store only through the existing
internal test/bootstrap mutator seam. B9 does not add a production mutation API.

### System Under Test

```text
OperationEngine::prepare(
  operation,
  StatefulValidationContext{IndexedObjectStore, AppliedOperationView})
```

### Reference / Oracle

```text
OperationEngine::prepare(
  same operation,
  StatefulValidationContext{ReferenceObjectStore, same AppliedOperationView})
```

### Normalization / accepted equivalence

Inputs are already normalized typed Operations. B9 does not repeat A-lane
normalization and does not create a second semantic canonicalizer.

Comparison is exact typed projection equality. The comparator MUST NOT sort or
otherwise normalize emitted plan collections to hide an ordering difference;
B8 already requires deterministic ordering.

Exact comparison boundary:

```text
PrepareResult.disposition
PrepareResult.error.issue
PrepareResult.plan presence

if plan present:
  plan.operation
  plan.creates             (exact ordered ObjectRecord sequence)
  plan.replacements        (exact ordered ObjectRecord sequence)
  plan.deletes             (exact ordered ObjectId sequence)
  plan.delete_closure presence

if delete_closure present:
  requested_delete_ids
  resolved_hierarchy_closure
  resolved_connector_cascade_closure
  final_delete_set
```

The comparator also records canonical base-store projections before and after
planning. Planning must leave both unchanged.

No equality rule may ignore a semantic field, reorder a result, coerce an
error, or collapse `nullopt` and an empty DeleteClosure.

### Comparison output

A test-only `DifferentialObservation` may be defined locally inside the B9 test
translation unit. It is not a production API or semantic authority.

It records at minimum:

```text
fixture_id / trace_id
operation_index
OperationId
operation kind/name
reference disposition + issue
indexed disposition + issue
reference plan projection
indexed plan projection
before/after store projections
lookup counters for the indexed-side observation when enabled
```

### Mismatch classification

B9 test diagnostics classify the first observed mismatch by semantic path:

```text
DISPOSITION_MISMATCH
STATE_ISSUE_MISMATCH
PLAN_PRESENCE_MISMATCH
PLAN_OPERATION_MISMATCH
PLAN_CREATES_MISMATCH
PLAN_REPLACEMENTS_MISMATCH
PLAN_DELETES_MISMATCH
DELETE_CLOSURE_PRESENCE_MISMATCH
DELETE_CLOSURE_REQUESTED_MISMATCH
DELETE_CLOSURE_HIERARCHY_MISMATCH
DELETE_CLOSURE_CONNECTOR_CASCADE_MISMATCH
DELETE_CLOSURE_FINAL_SET_MISMATCH
BASE_PROJECTION_MISMATCH
PLANNING_MUTATION_OBSERVED
INDEXED_SINGLE_TARGET_FULL_ENUMERATION
```

The failure message MUST include:

```text
first operation index
OperationId
operation name
fixture/trace id
fixed seed when applicable
semantic path / mismatch class
reference-side summary
indexed-side summary
```

Do not continue reporting later mismatches as though the first divergence were
already understood.

### Determinism

- Hand-authored fixtures are deterministic.
- Randomized bounded selection uses a checked-in fixed seed and fixed bound.
- Re-running the same seed/input bank must produce the same ordered observation
  sequence and same first-mismatch location.
- Evidence serialization must use stable operation order and stable field order;
  volatile timestamps are metadata only and never participate in parity.

### Failure artifact

Future B9 evidence is source-bound at:

```text
verification/evidence/gates/G1/<B9_SOURCE_SHA>/GT-G1-04-B/B-DIFF.json
```

An optional separate lookup artifact is permitted only if it makes the proof
clearer:

```text
verification/evidence/gates/G1/<B9_SOURCE_SHA>/GT-G1-04-B/B-DIFF-lookup.json
```

No GT-G1-04-C golden fixture or expected-outcome authority may be created.

---

## 5. Frozen B9 fixture / trace scope

B9 uses two complementary suites.

### A. Hand-authored deterministic differential trace

The trace MUST include operations that exercise the parent-plan B9 families:

```text
insert / absent identity
placement / hierarchy
connector reference / StablePort state
DeleteObjects cascade closure
RestoreObjects batch / staged graph
mask state
RichText state
idempotency AlreadyApplied / collision decision
```

The hand-authored trace MUST contain every one of the fifteen V1 Operation names
at least once so that every OperationEngine dispatch family participates in the
Reference/Indexed comparison:

```text
InsertObjects
DeleteObjects
RestoreObjects
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

This is **trace-presence coverage only**. It is not the B10 requirement for at
least one positive and one negative row for each operation family.

### B. Fixed-seed bounded randomized differential trace

The P32 test must define checked-in constants equivalent to:

```text
seed:        0xB9D1FF01
trace bound: 256 prepare observations
```

The generator/selector operates only over a bounded bank of normalized typed
Operations and canonical bootstrap records owned by the test. It must not infer
new product semantics or auto-bless production output.

The fixed-seed run is required to detect state/store implementation drift that
a small hand-authored trace may miss. Increasing the bound later is a test-only
strengthening; changing semantics or oracle rules requires a new package.

---

## 6. Indexed lookup instrumentation boundary

B9 does not reopen GT-G1-03 or rewrite IndexedObjectStore.

Accepted implementation reality already provides indexed `contains`/`find`
through the store's keyed record container. B9 must additionally prove that its
single-target planning paths do not request a full canonical enumeration.

Use a test-only counting ObjectStore adapter around the IndexedObjectStore
(similar to the accepted B8 CountingStore seam) and record calls to:

```text
size
contains
find
allObjects
children
```

For B9's named single-target lookup suite:

```text
allObjects_calls == 0
```

is mandatory. `find`/`contains` and authority-required `children` traversal are
allowed.

This proves the B planning layer does not introduce ObjectId full enumeration
on indexed single-target paths while relying on the accepted GT-G1-03 store
contract for the implementation complexity of `find`/`contains` themselves.

Delete cascade, whole-store comparison at test boundaries, and bounded fixture
enumeration are not mislabeled as single-target lookup paths.

If satisfying this requirement would require a new production ObjectStore API
or new production instrumentation surface, STOP with
`CROSS_LANE_SCOPE_REQUIRED` rather than expanding B9.

---

## 7. Future B9 P32 file scope

After a separate P32 authorization, B9 may create only:

```text
runtime/semantic/tests/stateful_validation_differential_test.cpp
```

and modify only:

```text
runtime/semantic/tests/CMakeLists.txt
```

The new test target may include `runtime/semantic/src` privately only to access
existing test/bootstrap seams already used by accepted ObjectStore tests.

No production semantic source/header change is authorized by this package.

No new compare tool is authorized because the differential can be performed
in-process with exact typed values. If implementation proves an external tool
is genuinely required, STOP and return for package amendment instead of adding
it silently.

After the source/test commit, evidence-only files may be added only under:

```text
verification/evidence/gates/G1/<B9_SOURCE_SHA>/GT-G1-04-B/
```

with the names defined in section 4.

---

## 8. RED-first and required verification

Future B9 P32 must start with tests that fail before the B9 differential harness
exists and then prove all of the following:

1. Same deterministic fixture + same Operation produces identical disposition
   and exact StatefulIssue across Reference and Indexed stores.
2. Prepared results have exact plan-presence parity.
3. Prepared plans have exact typed equality over the current B8 projection:
   operation, creates, replacements, deletes, and DeleteClosure.
4. DeleteObjects compares all four accepted DeleteClosure partitions.
5. SplitStrokes compares generic `deletes + creates` and requires
   `delete_closure == nullopt` on both sides.
6. AlreadyApplied and collision decisions match across both stores.
7. Hand-authored trace contains all fifteen Operation names at least once.
8. Fixed-seed bounded randomized trace passes with the frozen seed/bound.
9. Repeating the same fixed-seed trace produces identical ordered observations.
10. Planning leaves both stores' canonical projections unchanged.
11. A deliberately divergent **test-only fixture** is detected by the
    comparator and reports the first mismatch path/index/OperationId/store side;
    this must not modify production store behavior.
12. Indexed single-target lookup suite records `allObjects_calls == 0`.
13. Existing `object_store_differential_test`, B8 `apply_plan_test`, and
    OperationEngine boundary tests remain green.
14. Full semantic CTest remains green.

Required verification layers for a future source/test commit:

```text
focused B9 CTest
existing ObjectStore differential + B8 ApplyPlan regression
full semantic CTest
repository-native hosted G1 Semantic Codec exact-head CI
aggregated repository PR gate / platform-protocol regression as required by the
current branch CI DAG
```

CI/regression runs are supporting evidence. The B9 oracle remains the exact
Reference/Indexed typed comparison described above.

---

## 9. Evidence contract for future P34

`B-DIFF.json` must bind at least:

```text
format/version
GT-G1-04-B/B9
package_ref
sourceCommit / testedCommit
branch
ReferenceObjectStore oracle identity
IndexedObjectStore SUT identity
hand-authored fixture/trace IDs
15-operation trace-presence manifest
fixed randomized seed = 0xB9D1FF01
random trace bound = 256
observation count
parity result
first mismatch = null on PASS, otherwise full mismatch diagnostic
lookup counters / single-target allObjects result
before/after no-mutation projection proof
focused/full test commands and results
exact-head CI run IDs/status when available
explicit non-ingress flags for B10 / C / GT-G1-05
```

Evidence must describe observations. It must not define new semantic expected
outcomes.

P34 must be able to independently resolve the exact source commit and the later
evidence-only materialized commit.

---

## 10. Explicit non-goals / authorization boundary

B9 does not authorize:

```text
B0-B8 rework or re-review
ReferenceObjectStore semantic changes
IndexedObjectStore semantic changes or ObjectIndex rewrite
ObjectStore API expansion
production instrumentation API
new semantic validation rules
new PrepareResult / PreparedApplyPlan fields
Atomic Apply
SemanticGeneration
ChangeSet
CanonicalCommitStamp
History
DataBridge
Outbox
renderer / scene / resource work
GT-G1-04-C fixture compiler / golden corpus
B10 full 15-operation positive/negative matrix
GT-G1-05
unrelated refactor
post-commit behavior
```

In particular:

```text
B9 all-15 coverage = differential trace presence
B10 all-15 coverage = full positive/negative operation matrix
```

They are not interchangeable.

---

## 11. Blocked return behavior

Future B9 P32 must fail closed:

```text
BLOCKED_AUTHORITY
  Current Authority no longer supports the frozen oracle/comparison semantics.

TEST_ORACLE_INSUFFICIENT
  Reference/Indexed parity cannot be observed independently enough to prove B9.

CROSS_LANE_SCOPE_REQUIRED
  proof would require store API/production semantic changes, C golden data,
  Atomic Apply, or another lane's ownership.

BLOCKED_EXECUTION_DIVERGENCE
  accepted package/anchor ancestry cannot be established or repository state
  contradicts authorized scope.

BLOCKED_EVIDENCE
  exact source/test result cannot be materialized for independent P34 review.
```

Do not silently repair Authority, production semantics, or another package.

---

## 12. Materialization sequence and current lifecycle status

This document is a P31 package artifact only.

Required lifecycle remains:

```text
P31 documentation-only package commit
        ↓
CONTROL_REVIEW verifies exact package commit
        ↓
separate B9 P32 authorization
        ↓
source/test commit
        ↓
independent verification / exact-head CI
        ↓
separate evidence-only commit containing B-DIFF
        ↓
P34
```

The future P32 surface handoff must carry:

```yaml
stage: P32
stage_owner: aegis-implementation
package_ref: <this reviewed materialization commit>
task_anchor:
  revision: bc80d0a3c1376b0334f79d8e2f5e7588cefff645
  relation: ancestor
resume_cursor: null
```

Until CONTROL_REVIEW explicitly authorizes that handoff:

```text
B9 P32 = NOT_AUTHORIZED
B10 = NOT_AUTHORIZED
GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05 = NOT_AUTHORIZED
```

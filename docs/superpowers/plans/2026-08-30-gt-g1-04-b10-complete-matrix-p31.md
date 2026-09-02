# GT-G1-04-B B10 P31 — Complete Fifteen-Operation Matrix and B No-Mutation Gate

> **Status:** P31 PACKAGE / PENDING CONTROL_REVIEW
>
> **Task:** `GT-G1-04-B / B10`
>
> **Stage:** `P31 Task Packaging`
>
> **Stage owner:** `aegis-implementation`
>
> **Execution surface:** `CONTROL_REASONING`
>
> This document supersedes **only Task Package B10** in
> `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`.
>
> B0-B9 accepted implementation, evidence, and final Gate outcomes remain
> inherited and are not reopened by this package.
>
> **This document does not authorize B10 P32.** P32 remains NOT_AUTHORIZED until
> an independent CONTROL_REVIEW explicitly approves this exact materialized P31
> package ref.

---

## 1. Purpose and inherited control state

B10 is the final **B-lane gate/evidence package**, not a second validator
implementation and not an Atomic Apply package.

Its purpose is to assemble one reviewer-checkable B result that proves:

1. all fifteen V1 Operation families have stateful positive and negative
   coverage through `OperationEngine::prepare`;
2. `Rejected`, `AlreadyApplied`, and `Prepared` planning paths do not mutate
   canonical ObjectStore state, IndexedObjectStore derived-index state, or the
   caller-supplied applied-operation view; and
3. the B authority boundary still terminates at immutable
   `PreparedApplyPlan`, before GT-G1-05 Atomic Apply concerns.

Inherited without re-review:

```text
B0-B9 = ACCEPTED_FOR_DOWNSTREAM

B10 P31 = CURRENT_STAGE
B10 P32 = NOT_AUTHORIZED

GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05   = NOT_AUTHORIZED
```

Accepted B9 final refs:

```text
B9 package_ref:
3a276468e39b3f2171d4425f1b60a8469cc35819

B9 source_ref:
e2cd81215501abb942cfdf92d3e239778fac7531

B9 materialized_ref:
c5b1347539fae3aae9dfe640b51a6ccd08a7dbc8
```

B9 P34 is inherited as PASS with no formal findings. B10 MUST NOT reopen B8
PreparedApplyPlan semantics, B9 differential design, B8/B9 P35/P36, or the
question of whether `ReferenceObjectStore` is the accepted differential oracle.

---

## 2. P33-style repository reconciliation for this P31 occurrence

Fresh remote repository observation before materializing this package:

```text
repo:   Mostorm-Labs/axiom
branch: codex/gt-g1-04-operation-apply
historical B10 task anchor: c5b1347539fae3aae9dfe640b51a6ccd08a7dbc8
observed remote HEAD:      c5b1347539fae3aae9dfe640b51a6ccd08a7dbc8
post-anchor remote commits: none
```

Execution contract:

```yaml
task_anchor:
  revision: c5b1347539fae3aae9dfe640b51a6ccd08a7dbc8
  relation: ancestor
resume_cursor: null
```

P33 classification:

```text
ANCHOR_DESCENDANT_WITHOUT_CURSOR
```

Rationale: `resume_cursor` is null because B10 has not executed; the accepted
anchor is an ancestor of the observed branch HEAD (in this observation, equal to
it). No post-anchor remote delta exists to reconcile.

The anchor commit is the B9 evidence-only materialization whose parent is the B9
source ref. Its committed delta is confined to B9 evidence. At this observation
there is no committed B10 P31 package, no
`g1_04_b_operation_matrix_test.cpp`, no `g1_04_b_no_mutation_test.cpp`, and no
committed B10 evidence directory.

This P31 observation is about committed repository state. It does not claim
knowledge of any uncommitted state outside the reviewer-accessible branch.

Any future P32 resume must repeat P33 reconciliation against both this
`task_anchor` and the reviewed `package_ref`; a branch advance is not itself a
failure, but its descendant delta must be inspected before execution.

---

## 3. Current Authority binding

B10 consumes, and does not repair or reinterpret, the Current Authority already
bound by the parent B plan:

- `docs/superpowers/plans/2026-08-28-gt-g1-04-b-stateful-validation.md`
  - B8+B9 -> B10 DAG edge;
  - B10 purpose, file scope, fifteen-operation coverage, no-mutation gate,
    evidence and performance boundary.
- `docs/superpowers/plans/2026-08-26-g1-semantic-kernel-codex-package.md`
  - accepted ObjectStore Reference/Indexed oracle and lookup constraints.
- `docs/notion/authority/04-semantic-schema/02-operation-model/operation-payload-validation-v0.1.md`
- `docs/notion/authority/04-semantic-schema/02-operation-model/operation-structural-semantics-v1-closure-v0.1.md`
- `docs/notion/authority/04-semantic-schema/02-operation-model/restoreobjects-identity-state-tombstone-eligibility-v1-authority-closure-v0.1.md`
- `docs/notion/authority/04-semantic-schema/01-object-schema/connector-anchor-contract-v1-release-v0.1.md`
- `docs/notion/authority/04-semantic-schema/01-object-schema/objectkind-version-registry-v1-release-v0.1.md`
- `docs/notion/authority/04-semantic-schema/01-object-schema/field-registry-v1-release-v0.1.md`
- `docs/notion/authority/04-semantic-schema/00-overview/g1-04-a-semantic-authority-closure-gate-v0.1.md`
- `docs/notion/authority/07-runtime-data-flow/07-03-operation-semantic-document-v0.1.md`
- `docs/notion/authority/07-runtime-data-flow/07-08-open-restore-catchup-v0.1.md`
- `docs/notion/authority/07-runtime-data-flow/07-09-special-flows-v0.1.md`
- `docs/notion/authority/07-runtime-data-flow/07-11-generation-changeset-apply-batch-v0.1.md`
- accepted refreshed B8 package
  `docs/superpowers/plans/2026-08-30-gt-g1-04-b8-prepared-apply-plan-p31.md`;
- accepted B9 package
  `docs/superpowers/plans/2026-08-30-gt-g1-04-b9-reference-indexed-differential-p31.md`.

The Semantic Conformance Golden Corpus may be consumed only as a coverage-list
input where the parent plan already permits it. B10 MUST NOT create, infer, or
bless GT-G1-04-C expected-outcome authority.

If any required semantic expectation is absent from the released authority,
return `MISSING_CONTRACT`. If repository behavior materially conflicts with
Current Authority, return `AUTHORITY_CONFLICT` to CONTROL_REVIEW. Do not repair
Authority in B10.

---

## 4. Dependencies and current repository reality

Dependencies are satisfied by accepted B1-B9.

B10 additionally consumes these existing repository seams/contracts:

```text
OperationEngine::prepare(const Operation&, const StatefulValidationContext&)
PrepareResult
PrepareDisposition::{kPrepared,kAlreadyApplied,kRejected}
StatefulIssue
PreparedApplyPlan
AppliedOperationView
ReferenceObjectStore
IndexedObjectStore
runtime/semantic/tests/a_lane_no_mutation_test.cpp
runtime/semantic/tests/stateful_validation_differential_test.cpp
runtime/semantic/tests/object_store_differential_test.cpp
internal::ObjectStoreMutator test/bootstrap mutation seam
internal::ObjectStoreMutator::indexMatchesRebuild(...)
```

Current `PreparedApplyPlan` is the accepted B8 projection:

```text
operation
creates
replacements
deletes
optional delete_closure
```

Current `AppliedOperationView` is read-only through `find(OperationId)`.

`internal::ObjectStoreMutator` remains explicitly internal/test/bootstrap-only.
B10 may use it to seed deterministic fixtures and to inspect the existing Indexed
index invariant, but MUST NOT expose it as production semantic API or call it
from production planning code.

The existing A-lane no-mutation test proves a narrower structural-validation
property. It is an input/reference pattern, not a substitute for B10's
`OperationEngine::prepare` no-mutation matrix.

B9's differential test already owns Reference/Indexed decision/plan comparison,
fixed-seed differential coverage, deliberate divergence detection, and indexed
single-target lookup instrumentation. B10 MUST consume/re-run that evidence; it
MUST NOT create a second differential architecture.

---

## 5. Objective

For every one of the fifteen V1 Operation families, prove on both
`ReferenceObjectStore` and `IndexedObjectStore` that:

- at least one released-authority valid state produces the expected B-lane
  positive result and deterministic `PreparedApplyPlan` projection;
- at least one released-authority invalid state produces the expected B-lane
  state rejection;
- canonical state before and after `prepare()` is identical;
- the caller-supplied applied-operation view before and after `prepare()` is
  identical; and
- for IndexedObjectStore, the derived index remains equivalent to rebuild.

Separately prove `AlreadyApplied` is also fully no-mutation.

B10 therefore proves coverage and B-boundary integrity. It does not perform the
plan.

---

## 6. Exact file scope

### 6.1 P31 materialization scope

This P31 occurrence is **docs/control-plane only**:

```text
CREATE
docs/superpowers/plans/2026-08-30-gt-g1-04-b10-complete-matrix-p31.md
```

No source, test, evidence, schema, generated file, or Authority file may be
modified while materializing P31.

### 6.2 Future P32 authorized scope

Only after CONTROL_REVIEW approval of this exact package_ref:

```text
CREATE
runtime/semantic/tests/g1_04_b_operation_matrix_test.cpp

CREATE
runtime/semantic/tests/g1_04_b_no_mutation_test.cpp

MODIFY
runtime/semantic/tests/CMakeLists.txt
```

Expected test target names, following current repository naming conventions:

```text
canvas_semantic_g1_04_b_operation_matrix_test
canvas_semantic_g1_04_b_no_mutation_test
```

B10 is expected to be test/evidence-only at P32. Production semantic source or
header changes are **not authorized**. If the matrix exposes a real production
defect, stop, classify it, and return to CONTROL_REVIEW; do not silently repair
production inside B10.

Likewise, changing schemas, wire contracts, Authority, B0-B9 historical evidence,
or production instrumentation is outside this package.

---

## 7. Interfaces consumed and outputs produced

### Consumed

- normalized, structurally valid typed `Operation` from the accepted A boundary;
- `OperationEngine::prepare` and existing B1-B8 state/planning logic;
- read-only `StatefulValidationContext`;
- `AppliedOperationView`;
- `ReferenceObjectStore` and `IndexedObjectStore`;
- accepted B9 differential test/oracle and lookup-counting pattern;
- existing test/bootstrap-only fixture mutation seam;
- existing Indexed index rebuild invariant seam;
- accepted B0-B9 evidence lineage.

### Produced by P32 tests/evidence only

- complete fifteen-family logical coverage matrix;
- deterministic positive/negative result observations;
- deterministic `PreparedApplyPlan` projections;
- canonical before/after projections;
- applied-operation-view before/after projections;
- Indexed index before/after consistency observations;
- no-mutation results for Rejected, AlreadyApplied, and Prepared;
- B10 source-bound aggregation/binding for all required B evidence families.

No production API is produced by B10.

---

## 8. Frozen fifteen-operation coverage matrix contract

The logical matrix contains **exactly fifteen operation families**, no aliases
and no omitted V1 family:

1. `InsertObjects`
2. `DeleteObjects`
3. `RestoreObjects`
4. `SetPlacements`
5. `SetTransforms`
6. `PatchProperties`
7. `SetObjectSize`
8. `SetVectorPathGeometry`
9. `SetImageContent`
10. `AddStroke`
11. `SplitStrokes`
12. `AddEraseMasks`
13. `RemoveEraseMasks`
14. `EditRichText`
15. `SetConnectorContent`

For each family, P32 MUST define at least two logical cases:

```text
POSITIVE: released-authority valid state -> PreparedApplyPlan
NEGATIVE: released-authority state failure -> Rejected + exact StatefulIssue
```

These are **30 minimum logical cases**. Each logical case is evaluated against
both store implementations, so the minimum store-specific observation count is
60. Additional cases are allowed only for required special/rejection/Restore
coverage; they do not change the exactly-fifteen-family requirement.

Each logical case and each store-specific evaluation records at minimum:

```text
case_id
operation_name
polarity: positive | negative
state_case / fixture_id
store_implementation: ReferenceObjectStore | IndexedObjectStore
PrepareDisposition
StatefulIssue
plan_present
full deterministic PreparedApplyPlan projection when present
canonical ObjectStore projection before
canonical ObjectStore projection after
AppliedOperationView projection before
AppliedOperationView projection after
Indexed indexMatchesRebuild before/after when applicable
relevant children(parent) projection when the case exercises hierarchy
```

The plan projection is exact typed projection equality using accepted B8/B9
field/order semantics. B10 MUST NOT sort, normalize, or omit plan fields to make
a mismatch disappear.

The negative case for each family MUST be selected from a state rule already
owned and accepted by B1-B7 for that operation family. P32 may not invent a new
StatefulIssue or weaken an existing expectation merely to make the matrix pass.

Required rejection/special snapshots across the matrix, at minimum:

```text
missing target
wrong kind / invalid applicability
missing parent or semantic reference
hierarchy cycle
invalid connector target/state
invalid erase-mask state
invalid RichText state
whole-batch identity/state collision
OperationId collision
```

A single well-formed case may satisfy more than one coverage label only when the
accepted validator actually reports the frozen primary `StatefulIssue`; the
evidence must make the mapping explicit.

`AlreadyApplied` is not the positive row for a family. It is a separate
idempotency/no-mutation obligation.

---

## 9. Restore RST-B01..RST-B12 linkage

B10 does not redesign Restore semantics.

The `RestoreObjects` matrix section MUST link and source-bind all existing
accepted Restore cases:

```text
RST-B01
RST-B02
RST-B03
RST-B04
RST-B05
RST-B06
RST-B07
RST-B08
RST-B09
RST-B10
RST-B11
RST-B12
```

`operation-matrix.json` and `summary.json` must record, for every RST identifier:

```text
provider evidence / accepted test mapping
B10 exact-source revalidation result
store implementation coverage
linked B10 logical case(s)
```

If any RST identifier cannot be traced to reviewer-accessible accepted evidence
and revalidated at the B10 exact source, return `BLOCKED_EVIDENCE`; do not
recreate or reinterpret the Restore authority.

---

## 10. No-mutation oracle

B10's independent no-mutation test translation unit is
`g1_04_b_no_mutation_test.cpp`.

### 10.1 Canonical ObjectStore projection

For every tested prepare invocation:

1. bootstrap the fixture through the existing internal test-only seam;
2. capture a deterministic canonical store projection **before** `prepare()`;
3. invoke `OperationEngine::prepare`;
4. capture the same projection **after** `prepare()`;
5. require exact equality.

This applies to `kRejected`, `kAlreadyApplied`, and `kPrepared`.

### 10.2 Indexed derived index

For `IndexedObjectStore`:

```text
indexMatchesRebuild(before) == true
indexMatchesRebuild(after)  == true
canonical projection before == after
relevant children(parent) before == after
```

The rebuild check remains a test/bootstrap invariant seam only.

### 10.3 Caller-supplied AppliedOperationView

The P32 test may provide a deterministic test implementation of
`AppliedOperationView` whose backing entries can be projected for assertion.
For every prepare invocation:

```text
AppliedOperationView projection before == after
```

This includes equivalent replay (`AlreadyApplied`) and collision/rejection.
No production mutation accessor may be added to `AppliedOperationView` for this
test.

### 10.4 Rejected

Every rejected matrix case proves all three remain unchanged:

```text
ObjectStore canonical projection
Indexed ObjectIndex projection/invariant where applicable
caller-supplied AppliedOperationView
```

### 10.5 AlreadyApplied

At least one equivalent replay fixture for each store proves:

```text
disposition == kAlreadyApplied
plan absent
ObjectStore unchanged
Indexed index unchanged/consistent
AppliedOperationView unchanged
```

### 10.6 Prepared

Every positive matrix case proves:

```text
disposition == kPrepared
plan present
ObjectStore unchanged
Indexed index unchanged/consistent
AppliedOperationView unchanged
```

A `PreparedApplyPlan` is data, not an executable commit object. `prepare()` MUST
NOT:

```text
apply ObjectStore writes
increment SemanticGeneration
produce ChangeSet
produce CanonicalCommitStamp
write History
publish DataBridge / Outbox
perform local echo
```

B10's boundary statement is therefore frozen as:

```text
PreparedApplyPlan != Atomic Apply
```

If proving this statement requires adding production Apply hooks or
instrumentation, return `CROSS_LANE_SCOPE_REQUIRED` rather than expanding B10.

---

## 11. Reference / Indexed oracle responsibilities

`ReferenceObjectStore` remains the accepted correctness-first differential
ObjectStore oracle; `IndexedObjectStore` remains the production-oriented store.

B10 uses them for:

- executing every logical matrix case on both stores;
- proving no mutation on both stores;
- proving Indexed derived-index consistency; and
- consuming the already accepted B9 differential result for broader decision /
  plan parity.

B10 MUST NOT implement another Reference-vs-Indexed comparator framework.
Where exact cross-store plan parity is needed, reuse the B9 test helpers/pattern
or re-run the B9 target rather than creating a parallel oracle architecture.

The matrix itself is a coverage and no-mutation gate; it does not turn
`ReferenceObjectStore` into independent human-reviewed C semantic golden
authority.

---

## 12. RED tests for future P32

P32 follows RED -> GREEN without changing semantic authority.

### RED package setup

1. Create `g1_04_b_operation_matrix_test.cpp` with the frozen fifteen-family
   case registry and assertions.
2. Create `g1_04_b_no_mutation_test.cpp` with Rejected / AlreadyApplied /
   Prepared before-after oracles for both stores.
3. Add only the two test targets to `runtime/semantic/tests/CMakeLists.txt`.
4. Record the first expected RED result before completing the fixtures/assertion
   implementation. Acceptable RED is test/compile failure attributable to the
   newly introduced B10 test obligations, not a manufactured production change.

Required test groups should be named so evidence can select them deterministically,
for example:

```text
G104B10OperationMatrix.*
G104B10NoMutation.*
```

The RED transcript is evidence of test-first execution, not a Gate outcome.

If RED immediately reveals a pre-existing production semantic defect, do not
change the expected result to match production and do not fix production under
B10. Return the appropriate defect/blocker to CONTROL_REVIEW.

---

## 13. GREEN and regression verification for future P32

P32 may claim execution complete only after all of the following pass against
one exact `source_ref`:

1. build the two B10 test targets;
2. focused B10 operation-matrix tests;
3. focused B10 no-mutation tests;
4. accepted B8 PreparedApplyPlan tests relevant to plan projection;
5. accepted B9 Reference/Indexed differential test, including deliberate
   divergence detector and single-target lookup constraint;
6. all runtime semantic CTest targets;
7. repository-relevant Python verification / evidence validation used by the G1
   semantic lane;
8. `git diff --check`;
9. exact-source hosted CI for `source_ref`;
10. deterministic source-bound evidence generation and validation.

The execution report/evidence MUST record the **exact commands actually used**,
including build directory/configuration and test counts. P31 deliberately does
not invent a platform-specific build-directory command that is not stable
Authority; P32 must use the repository's current supported semantic verification
entrypoints and persist the exact invocation.

No local-only success is review-ready.

---

## 14. Evidence contract and exact source binding

All B10 evidence is rooted at:

```text
verification/evidence/gates/G1/<B10_SOURCE_SHA>/GT-G1-04-B/
```

Minimum native B10 artifacts:

```text
operation-matrix.json
no-mutation-results.json
summary.json
B-OP15.json
B-NOMUT.json
```

The B10 evidence root must also contain source-bound family records for:

```text
B-IDEM.json
B-STATE.json
B-HIER.json
B-CONN.json
B-DELETE.json
B-RESTORE.json
B-OP15.json
B-PLAN.json
B-NOMUT.json
B-DIFF.json
```

### 14.1 Historical-family aggregation rule

B10 MUST NOT blindly copy B0-B9 evidence into a new SHA directory and MUST NOT
modify historical B0-B9 evidence.

For a family first proven upstream, the B10 `<family>.json` is a **B10
source-binding/aggregation record**. It records at minimum:

```text
evidence_family
consumer_source_ref == B10_SOURCE_SHA
provider task/package
provider package_ref when available
provider source_ref
provider materialized_ref
provider artifact path
provider artifact content/blob hash or equivalent immutable identity
B10 exact-source revalidation command(s)
B10 exact-source result/count
status
```

The provider artifact remains the historical evidence authority; the B10 binding
record proves that the accepted family remains satisfied at the B10 exact source.

For B10-native `B-OP15` and `B-NOMUT`, the family record points directly to the
new matrix/no-mutation observations at `B10_SOURCE_SHA` and records any accepted
upstream providers it aggregates.

`B-PLAN` must bind the accepted B8 plan evidence and B10 exact-source plan tests.
`B-DIFF` must bind the accepted B9 evidence and B10 exact-source rerun. It must
not recreate B9's comparator design.

If an accepted family cannot be given both a durable provider binding and a B10
exact-source revalidation result, P32 returns `BLOCKED_EVIDENCE`.

### 14.2 `operation-matrix.json`

Must record:

```text
source_ref
exact fifteen family list
all logical case IDs and polarity
all store-specific evaluations
PrepareDisposition / StatefulIssue
full deterministic plan projection
canonical before/after
AppliedOperationView before/after
Indexed invariant result
required rejection coverage labels
RST-B01..RST-B12 linkage
```

### 14.3 `no-mutation-results.json`

Must record at minimum:

```text
source_ref
Rejected observations for both stores
AlreadyApplied observations for both stores
Prepared observations for both stores
canonical before/after equality
AppliedOperationView before/after equality
Indexed indexMatchesRebuild before/after
PreparedApplyPlan != Atomic Apply boundary assertion
```

### 14.4 `summary.json`

Must be a reviewer index containing:

```text
package_ref
source_ref
materialized_ref or pending-materialization placeholder before commit
commands
focused test counts/results
full semantic CTest counts/results
relevant Python verification results
hosted exact-source CI identity/result
git diff --check result
15-operation coverage summary
RST-B01..RST-B12 summary
Reference/Indexed no-mutation summary
ObjectIndex consistency summary
all B evidence-family bindings
performance constraint result
B boundary statement
```

Evidence serialization must be deterministic except explicitly labeled volatile
metadata. Evidence must be validated before materialization.

---

## 15. Durable materialization and future P32 return contract

Three immutable refs are mandatory for a successful P32 handoff:

```yaml
P32_return:
  package_ref: <reviewed B10 P31 materialization commit>
  source_ref: <exact B10 source/test commit>
  materialized_ref: <reviewer-accessible evidence-only commit>
```

Semantics:

- `package_ref` is the exact P31 commit CONTROL_REVIEW approved;
- `source_ref` is the exact source/test commit on which all tests and hosted CI
  ran;
- `materialized_ref` is a durable reviewer-accessible evidence commit whose
  evidence is bound to `source_ref`.

`materialized_ref` is mandatory.

None of the following is CONTROL_REVIEW-ready:

```text
local-only source commit
uncommitted worktree state
local-only test transcript
unmaterialized CI result
an evidence path that is not committed/reviewer-accessible
```

If evidence cannot be durably materialized without changing tested source,
return:

```text
BLOCKED_EVIDENCE
```

Do not claim review-ready.

The evidence-only materialization commit may have `source_ref` as an ancestor and
may add only B10 evidence/control material permitted by the reviewed package. It
must not silently alter the tested semantic source/test tree.

---

## 16. Performance constraints

B10 is a gate/evidence package and MUST NOT introduce production hot-path work.

Inherited B9 requirement remains:

```text
Indexed single-target prepare paths:
allObjects_calls == 0
```

The B10 matrix may enumerate bounded store projections for test oracle purposes,
but all such projection and invariant inspection MUST occur **outside** the
`OperationEngine::prepare` lookup-count measurement window:

```text
capture before projection / invariant
reset counters
call Indexed OperationEngine::prepare
capture counters immediately
capture after projection / invariant
```

Test-oracle enumeration is not planner enumeration and must not contaminate the
measurement.

If a matrix case passes only by changing a production algorithm or weakening the
lookup constraint, stop and classify the defect. B10 may not silently absorb a
performance repair.

---

## 17. Frozen lane boundaries and non-goals

### A owns

```text
wire / envelope / normalization / stateless structural validation
```

B10 starts from normalized and structurally valid typed Operations. It MUST NOT
redo A0-A3 or generate A-invalid inputs as B state cases.

### B owns through this package

```text
stateful read-only validation
resulting-state planning
immutable PreparedApplyPlan
coverage / no-mutation gate evidence
```

### C owns later

```text
independent human-reviewed semantic golden authority
```

B10 MUST NOT create C golden fixtures, expected-outcome authority, or bless
production output as truth.

### GT-G1-05 owns later

```text
Atomic Apply
ObjectStore writes
SemanticGeneration
ChangeSet
CanonicalCommitStamp
History
DataBridge / Outbox publication
local echo / post-commit publication
```

B10 MUST NOT start or emulate those behaviors.

Additional non-goals:

- no ObjectStore production API additions;
- no production instrumentation additions;
- no new differential framework;
- no Restore semantic redesign;
- no wire/schema change;
- no Authority repair;
- no rewriting or overwriting B0-B9 evidence;
- no GT-G1-04-C or GT-G1-05 implementation.

---

## 18. Blocked return behavior

P31/P32 fail closed. At minimum support:

```text
AUTHORITY_CONFLICT
MISSING_CONTRACT
WIRE_SCHEMA_CHANGE_REQUIRED
TEST_ORACLE_INSUFFICIENT
CROSS_LANE_SCOPE_REQUIRED
BLOCKED_EVIDENCE
BLOCKED_EXECUTION_DIVERGENCE
```

Routing rules:

- released Authority conflicts with required expectation -> `AUTHORITY_CONFLICT`;
- required state rule is unpublished/ambiguous -> `MISSING_CONTRACT`;
- proof requires protobuf/wire/schema change -> `WIRE_SCHEMA_CHANGE_REQUIRED`;
- current test seams cannot prove the frozen invariant without production
  pollution -> `TEST_ORACLE_INSUFFICIENT`;
- proof/fix requires C or GT-G1-05 responsibility -> `CROSS_LANE_SCOPE_REQUIRED`;
- exact-source evidence/CI cannot be durably reviewer-materialized ->
  `BLOCKED_EVIDENCE`;
- P33 sees incompatible source/test/evidence/Authority delta, or execution cannot
  remain bound to the reviewed package/source -> `BLOCKED_EXECUTION_DIVERGENCE`.

If B10 discovers a real B0-B9 implementation defect:

```text
DO NOT change matrix expectation to match production
DO NOT bless production output
DO NOT silently repair under this package
STOP and return the defect + first failing case to CONTROL_REVIEW
```

CONTROL_REVIEW decides whether a new repair occurrence/package is required.

---

## 19. Future B10 exit criteria

B10 implementation/evidence is complete only when all are true:

- [ ] exactly 15 V1 Operation families are represented;
- [ ] each family has at least one Prepared positive state case;
- [ ] each family has at least one state-rejected negative case;
- [ ] required rejection/special snapshots are mapped and visible;
- [ ] `RST-B01..RST-B12` are all linked and exact-source revalidated;
- [ ] `B-IDEM`, `B-STATE`, `B-HIER`, `B-CONN`, `B-DELETE`, `B-RESTORE`,
      `B-OP15`, `B-PLAN`, `B-NOMUT`, and `B-DIFF` are source-bound at the B10
      exact source;
- [ ] every Rejected case proves ObjectStore / AppliedOperationView no mutation
      and Indexed index consistency where applicable;
- [ ] `AlreadyApplied` proves the same no-mutation property on both stores;
- [ ] every Prepared case proves the same no-mutation property on both stores;
- [ ] ReferenceObjectStore and IndexedObjectStore are both covered;
- [ ] IndexedObjectStore index remains equivalent to rebuild;
- [ ] B9 single-target indexed `allObjects_calls == 0` remains true;
- [ ] B boundary ends at immutable `PreparedApplyPlan`;
- [ ] no C / GT-G1-05 ingress occurred;
- [ ] focused B10 tests pass at exact `source_ref`;
- [ ] all runtime semantic CTest targets pass at exact `source_ref`;
- [ ] relevant Python verification/evidence checks pass;
- [ ] exact-source hosted CI passes;
- [ ] `git diff --check` passes;
- [ ] deterministic B10 evidence validates and is bound to exact `source_ref`;
- [ ] reviewer-accessible mandatory `materialized_ref` exists;
- [ ] `package_ref`, `source_ref`, and `materialized_ref` are reported exactly.

B10 implementation/P32 MUST NOT declare final Gate PASS. Final PASS/BLOCKED belongs
to the later independent P34 final Gate review.

---

## 20. P31 implementation-control completeness review

This package freezes the required P31 control dimensions:

```text
Task ID / Purpose                         FROZEN
Current Authority refs                    FROZEN
Dependencies                              FROZEN
task_anchor / resume_cursor               FROZEN
Objective                                 FROZEN
Exact P31 and P32 file scope              FROZEN
Required changes                          FROZEN
Interfaces consumed                       FROZEN
Interfaces/evidence produced              FROZEN
15-operation coverage matrix contract     FROZEN
Restore RST-B01..RST-B12 linkage          FROZEN
No-mutation oracle                        FROZEN
Reference/Indexed oracle usage            FROZEN
RED tests                                 FROZEN
GREEN verification                        FROZEN
Full regression verification              FROZEN
Evidence artifacts                        FROZEN
Exact source binding                      FROZEN
Performance constraints                   FROZEN
Non-goals / lane boundaries               FROZEN
Exit criteria                             FROZEN
Blocked return behavior                   FROZEN
Durable materialization contract          FROZEN
Future P32 return contract                FROZEN
```

P31 completeness result:

```text
REVIEWED_PACKAGE
No P31 control-plane blocker identified from the reconciled repository state.
```

This is an implementation-control completeness statement only. It is **not**
CONTROL_REVIEW approval and **does not authorize P32**.

---

## 21. Future P32 completion report

A successful future execution must return, at minimum:

```yaml
P32_return:
  package_ref: <reviewed B10 P31 materialization commit>
  source_ref: <exact B10 source/test commit>
  materialized_ref: <reviewer-accessible evidence-only commit>

scope:
  created:
    - runtime/semantic/tests/g1_04_b_operation_matrix_test.cpp
    - runtime/semantic/tests/g1_04_b_no_mutation_test.cpp
  modified:
    - runtime/semantic/tests/CMakeLists.txt

verification:
  focused_matrix: <counts/results>
  focused_no_mutation: <counts/results>
  semantic_ctest: <counts/results>
  python_verification: <commands/results>
  hosted_exact_source_ci: <identity/result>
  diff_check: <result>

evidence:
  root: verification/evidence/gates/G1/<source_ref>/GT-G1-04-B/
  fifteen_operation_rows: <result>
  restore_rst_b01_b12: <result>
  reference_no_mutation: <result>
  indexed_no_mutation: <result>
  object_index_consistency: <result>
  evidence_family_bindings: <result>
  prepared_apply_plan_boundary: <result>

blockers: []
```

If the three immutable refs are not all available, the successful shape above is
invalid.

---

## 22. Stop condition after P31 materialization

After this docs-only file is committed and its exact `package_ref` is reported,
this occurrence stops at:

```text
B0-B9 = ACCEPTED_FOR_DOWNSTREAM

B10 P31 = MATERIALIZED / PENDING_CONTROL_REVIEW
B10 P32 = NOT_AUTHORIZED

GT-G1-04-C = DEFERRED / NOT_AUTHORIZED
GT-G1-05   = NOT_AUTHORIZED
```

No Codex execution, B10 source/test modification, B10 evidence generation, C
work, or GT-G1-05 work is authorized by P31 materialization.

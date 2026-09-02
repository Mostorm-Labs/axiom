# GT-G1-04-C C2 Independent Fixture Compiler + Reproducibility — P31 Task Package

> **Status:** P31 PACKAGE MATERIALIZATION / CONTROL ARTIFACT.
>
> This file packages only `GT-G1-04-C / C2 — Independent Fixture Compiler + Reproducibility`.
> It is **not** a P32 authorization. Do not execute the implementation until a later Aegis control review explicitly authorizes P32 against the immutable commit containing this package.
>
> C0 and C1 are inherited as closed downstream authority. This package does not reopen, rewrite, or re-audit them.

## 0. Task identity and purpose

```text
task_id = GT-G1-04-C/C2
task_name = Independent Fixture Compiler + Reproducibility
stage = P31 Task Packaging
purpose = transform accepted AUTHORITY_MANUAL case intent into deterministic DERIVED_GENERATED runnable input fixtures without borrowing production semantic correctness
```

C2 owns only the mechanical boundary:

```text
accepted C1 AUTHORITY_MANUAL root
        ↓
independent verification-only compiler
        ↓
DERIVED_GENERATED input fixtures + provenance
```

C2 does **not** execute production semantics, call an observer, compare providers, decide PASS/FAIL against runtime behavior, create a second expected-truth root, or start C3-C8.

---

## 1. Current Authority and accepted dependencies

C2 consumes these as resolved Current Authority inputs and does not reinterpret them:

1. `GT-G1-04-C P20 Verification Design Reconciliation v0.1` — Notion `3cc4c57a-590c-81ae-ab73-d75501c47169`.
2. `GT-G1-04-C P20 TerminalPhase Classification Addendum v0.1` — Notion `3cd4c57a-590c-8165-973f-ee31d93f1116`.
3. `GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31` — Notion `3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`.
4. Accepted C0 trust contracts and schemas already present in the repository.
5. Accepted C1 human-reviewed authoring root and blocking suite:
   - `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json`
   - `verification/corpus/semantic/v1/g1-04-c/authoring/expected.json`
   - `verification/corpus/semantic/v1/g1-04-c/suites/core.json`

Inherited C1 closure refs:

```yaml
package_ref: a9561da5bc781f148d12c85f90c34f860734a042
source_ref: 851c4a5ec03313bfe6d6258a9d0269f808ce374b
materialized_ref: e1b3e1dbc897a4b77385e7a01f8e348af2796610
```

Inherited C1 control state:

```text
C0 = ACCEPTED / CLOSED
C1 = ACCEPTED_FOR_DOWNSTREAM / CLOSED
C1 P36 = PASS / CLOSED
C1 final P34 = PASS
```

C1 expected truth remains exclusively `AUTHORITY_MANUAL`. Generated/runtime/reference/indexed/B-test output may not write, bless, repair, or replace it.

---

## 2. Task anchor and fresh repository reconciliation

```yaml
task_anchor:
  revision: e1b3e1dbc897a4b77385e7a01f8e348af2796610
  relation: ancestor
  branch_hint: codex/gt-g1-04-operation-apply
```

`Task Anchor != Execution Cursor`.

Fresh P31 repository-state check observed:

```text
repository = Mostorm-Labs/axiom
branch = codex/gt-g1-04-operation-apply
expected accepted C1 materialized ref = e1b3e1dbc897a4b77385e7a01f8e348af2796610
observed remote HEAD before C2 package materialization = e1b3e1dbc897a4b77385e7a01f8e348af2796610
classification = EXACT_ACCEPTED_BASELINE
```

Therefore C0/C1 closure is inherited directly. No descendant semantic delta required reconciliation before this P31 package.

A future P32 occurrence must record its actual `execution_start_ref` and prove the C2 task anchor is an ancestor. A legal package-only descendant is acceptable. Divergence, history rewrite, or unexpected semantic-scope change returns `BLOCKED_EXECUTION_DIVERGENCE`.

---

## 3. Fresh repository reality

At the accepted C1 materialization:

- `verification/fixture-author/` already contains prior independent verification-only compiler patterns, including `compile_g1_02r_leaf_golden.py` and its Python trust-boundary tests.
- `verification/fixture-author/compile_g1_04_c.py` does **not** exist.
- `verification/corpus/semantic/v1/g1-04-c/generated/` does **not** exist.
- C1 `cases.json`, `expected.json`, `core.json`, and `g1_04_c_authoring_root.test.mjs` already exist and are accepted upstream read-only inputs.
- C1 case records intentionally carry `inputRef = generated/inputs/<case-id>.json`; C1 explicitly left those targets unmaterialized for C2.
- The accepted C1 case schema contains case metadata/authority binding and the future `inputRef`; it does not add a second executable-input authoring file.

C2 therefore creates the missing independent compiler and derived fixture tree around the accepted C1 root. It must not add a competing `g1_04_c_authoring_v1.json` or similar semantic-authoring registry.

### C2-P31-D01 — stimulus construction is implementation, not expected authority

The compiler may contain deterministic verification-only stimulus constructors keyed by accepted case IDs and operation families. Those constructors are **not** a second correctness oracle and may not contain expected dispositions, terminal phases, semantic error categories, logical plan answers, or provider/runtime observations.

The allowed direction is:

```text
CaseIntent id + operationFamily + direct authority refs
        + published semantic schema/descriptor facts
        ↓
mechanical synthetic initial state + Operation stimulus
```

The forbidden direction is:

```text
expected disposition/error/plan answer
        ↓
choose or repair fixture until production returns that answer
```

If a case cannot be instantiated without making a material semantic choice that is not uniquely supported by its accepted case intent and Current Authority refs, C2 must return `BLOCKED_UPSTREAM`; it must not guess and must not modify C1 to hide the gap.

---

## 4. Exact future P32 repository scope

This section freezes the only implementation/source paths C2 may create after a separate P32 authorization.

### 4.1 Create

```text
verification/fixture-author/compile_g1_04_c.py
verification/tests/test_g1_04_c_fixture_compiler.py

verification/corpus/semantic/v1/g1-04-c/generated/manifest.json
verification/corpus/semantic/v1/g1-04-c/generated/inputs/<case-id>.json
verification/corpus/semantic/v1/g1-04-c/generated/provenance/<case-id>.json
```

`<case-id>` is the exact accepted ID set from C1 `authoring/cases.json`; the generated input count and per-case provenance count must equal the accepted case count. The current accepted C1 closure records 90/90 expected-truth cases; C2 does not re-author that inventory.

### 4.2 Modify

```text
None.
```

In particular C2 does **not** need to modify `verification/package.json`; focused Python tests are invoked explicitly and the existing verification workspace regression remains `npm run validate`.

### 4.3 Accepted upstream paths are read-only / forbidden to modify

```text
verification/schemas/semantic/g1-04-c-*.schema.json
verification/tools/g1_04_c_contract.mjs
verification/corpus/semantic/v1/g1-04-c/README.md
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/tests/g1_04_c_authoring_root.test.mjs
verification/tests/g1_04_c_schema.test.mjs
verification/tests/g1_04_c_authority_map.test.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
verification/package.json
verification/tools/validate_schemas.mjs
```

C2 also forbids all production/C3+ scope:

```text
runtime/semantic/**
runtime/semantic/tools/g1_04_c_observer.cpp
runtime/semantic/tools/g1_04_c_projection.*
verification/packages/semantic-conformance-cli/**
.github/workflows/g1-04-c-semantic-conformance.yml
```

And it explicitly forbids creating:

```text
verification/fixture-author/g1_04_c_authoring_v1.json
verification/corpus/semantic/v1/g1-04-c/generated/expected.json
verification/corpus/semantic/v1/g1-04-c/generated/**/expected*
any second human-authored expected/golden root
```

If correct C2 implementation requires changing an accepted C0/C1 path, stop with `BLOCKED_UPSTREAM` rather than widening scope.

---

## 5. Compiler input contract

`compile_g1_04_c.py` consumes only:

### Required C1 sources

```text
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
```

### Allowed mechanical metadata

Only where required to encode published semantic shapes, the compiler may read:

```text
published schemas
published descriptors / protobuf field tags
generic protobuf primitives
generic JSON/data encoding helpers
declarative registries that are already accepted published contracts
```

It may not read production test output, runtime observation output, C1 historical evidence as semantic input, or B test results as a recipe/oracle.

### Restricted use of `expected.json`

`expected.json` is read only for:

- exact case-ID one-to-one binding;
- `expectedRef` provenance binding;
- source-file and per-record content hashes;
- direct authority-ref provenance copied as metadata.

Fixture construction functions must not receive or branch on:

```text
disposition
terminalPhase
semanticErrorCategory
logicalPlanProjection
mutationExpected
provider output
PASS/FAIL result
```

The synthetic stimulus is constructed from accepted case intent / operation family / Current Authority facts, never from the desired implementation answer.

---

## 6. Generated input artifact contract

Every `generated/inputs/<case-id>.json` is a verification-only carrier with role:

```text
DERIVED_GENERATED
```

Top-level shape is frozen for C2 as:

```json
{
  "format": "axiom-g1-04-c-input-v1",
  "formatVersion": 1,
  "provenance": "DERIVED_GENERATED",
  "caseId": "<accepted case id>",
  "operationFamily": "<accepted V1 operation family>",
  "initialState": {
    "objects": [],
    "priorOperations": []
  },
  "operation": {},
  "executionVariants": []
}
```

Rules:

- `initialState.objects` is a deterministic renderer-/provider-neutral semantic state projection sufficient for later C3 construction of either Reference or Indexed ObjectStore.
- `initialState.priorOperations` is present only to express verification preconditions such as OperationId idempotency/collision; it is not History/Sync/tombstone state.
- `operation` is a deterministic verification JSON projection of the typed V1 Operation, using published semantic field names/types/descriptor facts. It is not a new Product ABI.
- `executionVariants` is verification-driver metadata only when a case requires the same semantic stimulus to be observed through multiple authority-defined ingress variants (for example Local/Replay/Remote). It must not contain expected outcomes.
- IDs, numeric values, object ordering, map/key ordering, and serialization must be stable across regenerations.
- No generated input may contain `disposition`, `terminalPhase`, `semanticErrorCategory`, `logicalPlanProjection`, `mutationExpected`, Reference/Indexed observed values, or PASS/FAIL verdicts.
- No runtime handle, pointer, container-layout detail, production issue enum, ObjectStore implementation detail, or observer output belongs in generated input.

The concrete nested Object/Operation field vocabulary must reuse published semantic contracts; C2 may not invent a competing Product semantic schema.

---

## 7. Per-case provenance contract

Every `generated/provenance/<case-id>.json` must be deterministic and minimally contain:

```text
format / formatVersion
provenance = DERIVED_GENERATED
caseId
operationFamily
sourceCaseRef = authoring/cases.json#<case-id>
sourceCaseRecordSha256
expectedRef = authoring/expected.json#<case-id>
expectedRecordSha256
caseAuthorityRuleRefs[]
expectedAuthorityRuleRefs[]
compiler.identity = g1-04-c-independent-fixture-compiler-v1
compiler.sourcePath = verification/fixture-author/compile_g1_04_c.py
compiler.sourceSha256
sourceFiles.cases.sha256
sourceFiles.expected.sha256
sourceFiles.coreSuite.sha256
generatedInput.path
generatedInput.sha256
generatedInput.bytes
```

The provenance file may carry hashes and refs to expected truth; it may not copy expected semantic answers into generated fixture content.

The compiler source SHA256 is the content identity used inside derived artifacts. Do not embed the eventual C2 `source_ref` Git commit in source-generated bytes, because that commit does not exist until the generated source tree is committed. The later durable evidence file binds the exact Git `source_ref` to the compiler/content hashes.

---

## 8. Global generated manifest contract

Create exactly:

```text
verification/corpus/semantic/v1/g1-04-c/generated/manifest.json
```

with role `DERIVED_GENERATED` and deterministic content including:

```text
format = axiom-g1-04-c-fixture-manifest-v1
formatVersion = 1
provenance = DERIVED_GENERATED
compiler identity + compiler source SHA256
cases.json / expected.json / core.json SHA256
caseCount
blockingCaseCount
entries[] sorted lexically by caseId
  - caseId
  - input path + SHA256 + byte count
  - provenance path + SHA256 + byte count
inventorySha256
```

`inventorySha256` is computed from the deterministic sorted entry inventory; the manifest does not attempt a self-hash.

No current wall-clock time, absolute filesystem path, host name, random UUID, environment-specific value, unordered map traversal, current branch name, or unbound Git HEAD may enter generated bytes.

---

## 9. Compiler trust boundary

### 9.1 Explicitly forbidden semantic dependencies

`compile_g1_04_c.py` must not import/link/call/invoke:

```text
OperationEngine
prepareApplyPlan
stateless validator
stateful validator
ReferenceObjectStore semantic behavior
IndexedObjectStore semantic behavior
production canonical planner logic
runtime semantic outcome logic
existing B test executables/results
future C observer/coordinator
```

### 9.2 Strong mechanical independence guard

For C2, the compiler should remain Python-stdlib-only unless an already-published generic descriptor/protobuf primitive is strictly necessary. It must not use dynamic loading or shell execution to reach production code.

The focused test must mechanically reject at least:

```text
imports from runtime / canvas semantic production modules
subprocess / os.system / os.popen production-binary invocation
ctypes / cffi / dlopen-style dynamic loading
importlib-based dynamic loading of production modules
forbidden source/path tokens:
  OperationEngine
  prepareApplyPlan
  stateful_validation
  stateless_validation
  ReferenceObjectStore
  IndexedObjectStore
  runtime/semantic
  canvas::semantic
```

If a generic protobuf package is used, its role must be limited to published wire primitives/descriptors and the boundary test must still prove no production semantic module/binary is reachable.

### 9.3 Minimal-source execution proof

The focused test must create a temporary minimal source root containing only:

```text
compile_g1_04_c.py
accepted C1 cases.json / expected.json / core.json
only the specifically required published schema/descriptor/declarative metadata
```

with **no `runtime/semantic/` tree and no production executable**, and prove generation succeeds there.

Success in that stripped root is mandatory evidence that production semantic correctness is not a hidden dependency.

---

## 10. No-write-to-authority-root contract

The compiler must resolve and validate its output target **before creating any output directory or file**.

Any output target equal to or nested under:

```text
verification/corpus/semantic/v1/g1-04-c/authoring/**
```

must be rejected fail-closed with non-zero exit / exception.

Focused test requirements:

1. hash/read the accepted `cases.json` and `expected.json` bytes;
2. attempt output to `authoring/` and `authoring/generated/`;
3. require failure before write;
4. assert authoring bytes and directory inventory are unchanged.

This is a mechanical test, not a code-review-only convention.

---

## 11. Deterministic regeneration contract

C2 must prove all three forms of reproducibility.

### 11.1 Two clean generations

```text
same accepted authoring root + same compiler
        ↓
temporary output A
        ↓
temporary output B
```

Require:

```text
file inventory A == file inventory B
AND every corresponding file is byte-for-byte identical
AND manifest A == manifest B byte-for-byte
AND every recomputed SHA256 equals recorded SHA256
```

Semantic equality alone is insufficient.

### 11.2 Temp-directory regeneration against committed artifacts

From a clean temporary output root:

```text
regenerate
→ compare to committed verification/corpus/semantic/v1/g1-04-c/generated/**
```

Require byte-for-byte equality for the complete generated tree. Validation must not first rewrite repository fixtures.

### 11.3 Deterministic source/provenance hashing

Recompute and verify:

- compiler source SHA256;
- cases/expected/core source SHA256;
- per-record case/expected SHA256;
- every generated input SHA256;
- every per-case provenance SHA256 referenced by manifest;
- manifest inventory SHA256.

Any mismatch is C2 failure; there is no bless/update path.

---

## 12. Focused C2 tests / oracle

Create `verification/tests/test_g1_04_c_fixture_compiler.py` with independent tests covering at minimum:

1. **basic positive generation** — all accepted case IDs generate exactly one input and one provenance artifact; all output roles are `DERIVED_GENERATED`.
2. **two-materialization byte identity** — A and B trees are byte-for-byte equal.
3. **committed-tree reproducibility** — clean temp generation equals committed `generated/**` byte-for-byte.
4. **hash integrity** — recompute all recorded source/artifact/inventory hashes.
5. **provenance traceability** — every case binds source case record, expected ref, authority refs, compiler identity/source hash, and generated artifact hash.
6. **authoring output rejection** — attempted `authoring/**` output fails before write; accepted C1 bytes remain unchanged.
7. **production-independence static guard** — forbidden imports/symbols/dynamic/binary invocation absent.
8. **minimal-source independence** — generation succeeds with production runtime tree absent.
9. **no generated oracle** — generated input/provenance/manifest tree contains no copied expected semantic outcome fields or PASS/FAIL/golden truth.
10. **C1 read-only proof** — exact accepted C0/C1 paths have no diff from the C2 task anchor in the C2 source commit.

Primary C2 oracle is therefore:

```text
accepted C1 source bytes + Current Authority refs
        ↓
pure mechanical construction
        ↓
content/hash/provenance reproducibility
```

Production semantic behavior is **not** a C2 oracle.

---

## 13. Later P32 TDD / execution sequence

This section describes future P32 work only. It does not authorize it.

### Task 0 — execution preflight, no edits

- record exact `execution_start_ref`;
- resolve this P31 package commit as `package_ref`;
- prove `e1b3e1dbc897a4b77385e7a01f8e348af2796610` is an ancestor of `execution_start_ref`;
- inspect only descendant delta;
- if divergence/history rewrite/unexpected semantic scope appears, return `BLOCKED_EXECUTION_DIVERGENCE`;
- confirm no unauthorized C2 compiler/generated/test files already exist; if interrupted/partial work exists, reconcile under P33 instead of overwriting blindly.

### Task 1 — RED: fixture compiler contract test

Create only:

```text
verification/tests/test_g1_04_c_fixture_compiler.py
```

First run must fail because `compile_g1_04_c.py` / generated outputs do not exist, not because of unrelated verification infrastructure.

### Task 2 — GREEN: independent compiler + deterministic generated tree

Create:

```text
verification/fixture-author/compile_g1_04_c.py
verification/corpus/semantic/v1/g1-04-c/generated/manifest.json
verification/corpus/semantic/v1/g1-04-c/generated/inputs/<case-id>.json
verification/corpus/semantic/v1/g1-04-c/generated/provenance/<case-id>.json
```

Implement only enough verification-only construction logic to satisfy this package. No observer/runtime call is allowed.

### Task 3 — focused C2 verification

Run from repository root:

```bash
python -m unittest verification.tests.test_g1_04_c_fixture_compiler
```

Expected: PASS.

Then explicitly regenerate to two clean temporary roots and against committed generated artifacts through the test/CLI contract; expected byte-for-byte PASS.

### Task 4 — inherited verification workspace regression

Run without changing existing scripts:

```bash
cd verification
npm run validate
```

Expected: PASS.

Then from repository root:

```bash
git diff --check
```

Expected: PASS.

No C++ build, semantic CTest, Reference/Indexed execution, or observer run is required for C2 because no production/runtime C++ path is authorized to change. If C2 touches runtime code such that a C++ build appears necessary, that is a scope violation, not a reason to widen the package.

### Task 5 — source commit and scope proof

Commit exactly the C2 compiler/test/generated-tree source files. Record that commit as `source_ref`.

Require exact changed-file proof from `execution_start_ref` to `source_ref` showing only the authorized C2 paths. Separately prove the accepted C0/C1 read-only paths are byte-identical to the C2 task anchor.

### Task 6 — durable C2 evidence

After `source_ref` exists, create under:

```text
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-FIXTURE-REPRO.json
```

This is the only C2 evidence file required by this package. It must contain at minimum:

```text
source_ref
package_ref
task_anchor revision + relation
execution_start_ref
compiler identity + compiler source SHA256
cases.json SHA256
expected.json SHA256
core.json SHA256
accepted authoring case count / generated input count
committed generated manifest path + SHA256
full generated inventory/hash summary
two-clean-generation byte equality = PASS
temp-vs-committed byte equality = PASS
no-authoring-write proof = PASS
production-independence static guard = PASS
minimal-source independence = PASS
implementation-derived expected count = 0
generated expected count = 0
runtime/reference/indexed output used as expected = false
focused C2 test = PASS
verification workspace regression = PASS
git diff check = PASS
accepted C0/C1 paths changed = false
```

Parse/recompute/corroborate the evidence against `source_ref`, commit only this evidence file, push the branch, and return that reviewer-resolvable evidence commit as `materialized_ref`.

Do not overwrite or edit the historical C1 evidence under the C1 source-ref directory.

If exact remote evidence cannot be materialized, return `BLOCKED_EVIDENCE`; local-only transcripts or SHAs are insufficient for P34.

---

## 14. Explicit non-goals

C2 does not start or implement:

```text
C3 Facts-Only Production Observer
runtime/semantic/tools/g1_04_c_observer.cpp
runtime/semantic/tools/g1_04_c_projection.*
OperationEngine::prepare() observation
Reference provider execution
Indexed provider execution
C4 coordinator / golden comparison
C5 runnable 15-operation execution corpus
C6 cross-cutting execution
C7 provider differential / final Gate aggregation
C8 CI workflow
Atomic Apply
SemanticGeneration
ChangeSet
CanonicalCommitStamp
GT-G1-05
```

C2 may generate input artifacts that those later slices consume. Generating a fixture is not production execution and is not proof of semantic conformance.

---

## 15. Performance / engineering constraints

C2 changes no production runtime and therefore has no runtime latency/frame/memory performance gate.

Engineering constraints are:

```text
network dependency during generation = 0
production semantic binary invocation = 0
production semantic import/link = 0
writes under authoring/** = 0
implementation-derived expected truth = 0
generated expected/golden truth = 0
nondeterministic generated bytes = 0
C0/C1 accepted-path modifications = 0
```

Generation must be finite and practical for the accepted C1 corpus, but correctness/reproducibility gates take precedence over micro-optimizing the compiler.

---

## 16. Fail-closed / blocked-return behavior

Return control instead of improvising under these conditions:

### `BLOCKED_EXECUTION_DIVERGENCE`

- task anchor is not an ancestor of future execution start;
- branch history was rewritten/diverged;
- a descendant introduces unexpected semantic scope that invalidates the package.

### `BLOCKED_UPSTREAM`

- C2 requires modifying accepted C0/C1 schemas/helper/README/authoring/suite/test contract;
- a mandatory case cannot be instantiated without inventing a material semantic stimulus not uniquely supported by accepted case intent + Current Authority refs;
- published schema/descriptor facts required for deterministic encoding are missing and would require a new upstream contract;
- C2 would need a second human-authoring source to proceed.

### `BLOCKED_AUTHORITY`

- fixture work exposes that human-reviewed expected truth itself is contradictory, missing, or no longer supported by Current Authority;
- expected truth would need to be repaired/blessed from runtime/reference/indexed output.

### `BLOCKED_SCOPE`

- correct implementation appears to require C3 observer, production semantic code, provider execution, coordinator/golden comparison, CI, or GT-G1-05 work.

### `BLOCKED_EVIDENCE`

- exact `source_ref` evidence cannot be durably materialized and pushed for independent review.

A reproducibility/hash/test failure inside authorized C2 code is an implementation failure to fix within C2; it is not permission to change authority or bless outputs.

---

## 17. P32 exit criteria frozen by this package

A future C2 P32 result is eligible for independent Gate review only when all of the following are true:

```text
compiler exists only at verification/fixture-author/compile_g1_04_c.py
AND generated tree exists only under g1-04-c/generated/**
AND every generated artifact is DERIVED_GENERATED
AND generated input count == accepted C1 case count
AND every case has deterministic input + provenance
AND every case binds source-case + expected-ref + authority refs + compiler identity + source hashes + generated hash
AND two clean regenerations are byte-for-byte identical
AND clean temp regeneration equals committed generated tree byte-for-byte
AND all recorded SHA256 values recompute exactly
AND attempted authoring/** output fails before write
AND accepted C1 cases.json / expected.json / core.json are unchanged
AND accepted C0 trust contracts are unchanged
AND compiler has no production semantic import/link/binary invocation
AND minimal-source generation succeeds with runtime/semantic absent
AND implementation-derived expected truth count == 0
AND generated expected/golden truth count == 0
AND focused C2 tests PASS
AND existing verification workspace regression PASS
AND git diff --check PASS
AND C3-C8 / GT-G1-05 implementation is absent
AND exact C2 source_ref is bound to reviewer-accessible C-FIXTURE-REPRO evidence at materialized_ref
```

Frozen C2 exit summary:

```text
clean regeneration = PASS
no-write-to-authority-root = PASS
generated artifacts = deterministic hashes
compiler independence boundary = PASS
C1 authoring truth unchanged = PASS
```

---

## 18. Current lifecycle state after P31 package materialization

This document itself does not authorize code execution.

Expected control state after its documentation-only commit is materialized:

```text
GT-G1-04-C / C0
= CLOSED / ACCEPTED

GT-G1-04-C / C1
= CLOSED / ACCEPTED_FOR_DOWNSTREAM

GT-G1-04-C / C2 P31
= PACKAGE MATERIALIZED
= REVIEWABLE

GT-G1-04-C / C2 P32
= NOT_AUTHORIZED
= NOT_STARTED

C3-C8
= NOT_AUTHORIZED

GT-G1-05
= NOT_AUTHORIZED
```

The immutable Git commit containing this file is the C2 P31 `package_ref`. The C2 task anchor remains `e1b3e1dbc897a4b77385e7a01f8e348af2796610` with relation `ancestor`.

A later P32 authorization, if separately approved, must carry:

```yaml
type: surface_handoff
stage: P32
stage_owner: aegis-implementation
from_surface: CONTROL_REASONING
to_surface: CODE_EXECUTION
preferred_executor: codex
reason: repository_heavy_execution
package_ref: <immutable commit containing this C2 P31 package>
task_anchor:
  revision: e1b3e1dbc897a4b77385e7a01f8e348af2796610
  relation: ancestor
resume_cursor: null
return_surface: CONTROL_REVIEW
```

This is a handoff template only. P31 does not issue that authorization.

# GT-G1-04-C C2 Independent Fixture Compiler Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Repair `GT-G1-04-C / C2 — Independent Fixture Compiler + Reproducibility` so every accepted C1 case has a deterministic generated stimulus that actually realizes its accepted case intent, including the newly promoted exact-f64 non-finite fixture-input carrier contract, while preserving the C2 production-independence/oracle boundary.

**Architecture:** Keep C2 as a Python-stdlib-only mechanical fixture compiler over the accepted C1 `AUTHORITY_MANUAL` case root. Repair only the existing compiler/tests/generated tree, add an explicit all-case realization audit that inspects generated stimuli without reading expected outcomes, and keep type-directed runtime reconstruction/production execution in future C3. The repair is based on the failed C2 materialization at `8bd7d9820503de366f2a541b0d3f16bbd4876c26`; it does not reopen C0/C1 or modify production semantics.

**Tech Stack:** Python 3 stdlib (`json`, `hashlib`, `struct`, `pathlib`, `unittest`), existing verification workspace validation (`npm run validate`), Git.

**Spec:** Current Authority set: Notion `3cc4c57a-590c-81ae-ab73-d75501c47169`, `3cd4c57a-590c-8165-973f-ee31d93f1116`, `3cd4c57a-590c-8163-b606-e5f4e1eb4c92`; P23 promotion record `3cd4c57a-590c-8154-bb20-ea7840aca73c`.

## Global Constraints

- This file is a **P31 Task Package Refresh / Supersession control artifact**, not P36 authorization.
- Superseded historical C2 package: `9e23eedce264b94e4435d1198ae69a5878cb1e6f` = `HISTORICAL_FOR_EXECUTION / REQUIRES_REFRESH`.
- Accepted C1 materialized anchor remains `e1b3e1dbc897a4b77385e7a01f8e348af2796610` with `relation: ancestor`.
- Failed C2 repair base is `8bd7d9820503de366f2a541b0d3f16bbd4876c26`; preserve its source/evidence as historical failed-occurrence evidence.
- C0/C1 accepted authoring truth is read-only. Never rewrite `authoring/cases.json`, `authoring/expected.json`, or `suites/core.json`.
- C2 may mechanically transport exact f64 bits but must not perform production observer/provider/runtime semantic execution. That remains C3 ownership.
- Expected truth remains `AUTHORITY_MANUAL`; generated/runtime/reference/indexed/B-test output may never create, bless, repair, or replace it.
- If any accepted case cannot be instantiated without a material semantic choice not uniquely supported by Current Authority, return `BLOCKED_UPSTREAM` with the exact case and missing contract. Do not guess.

---

## 0. P31 Refresh Identity and Supersession

```text
task_id = GT-G1-04-C/C2
task_name = Independent Fixture Compiler + Reproducibility
stage = P31 Task Package Refresh / Supersession
repair_target = prior C2 implementation defect + test defect
prior_package_ref = 9e23eedce264b94e4435d1198ae69a5878cb1e6f
prior_failed_source_ref = 78bfd50096ee42ee1f14724f13a9156d0a34d21b
prior_failed_materialized_ref = 8bd7d9820503de366f2a541b0d3f16bbd4876c26
```

This package supersedes the old C2 package **for future execution only**. The old package and failed evidence remain historical records and must not be deleted or overwritten.

The repair purpose is narrower than a redesign:

```text
accepted C1 case intent
        + Current semantic/verification Authority
        ↓
independent deterministic stimulus construction
        ↓
DERIVED_GENERATED fixture
        ↓
case-intent realization audit
```

It remains forbidden to use:

```text
expected outcome
production/reference/indexed output
PASS/FAIL
        ↓
choose or mutate fixture
```

---

## 1. Current Authority Basis

The refreshed package consumes the following Current Authority without reinterpretation:

1. `GT-G1-04-C P20 Verification Design Reconciliation v0.1` — Notion `3cc4c57a-590c-81ae-ab73-d75501c47169`.
2. `GT-G1-04-C P20 TerminalPhase Classification Addendum v0.1` — Notion `3cd4c57a-590c-8165-973f-ee31d93f1116`.
3. `GT-G1-04-C P20 Non-Finite Fixture Carrier Encoding Addendum v0.1` — Notion `3cd4c57a-590c-8163-b606-e5f4e1eb4c92`, promoted Current by P23.
4. `GT-G1-04-C Non-Finite Fixture Carrier P23 Authority Promotion v0.1 — PASS` — Notion `3cd4c57a-590c-8154-bb20-ea7840aca73c`.
5. `GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31` — Notion `3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`.
6. Accepted C0 schemas/contracts already present in the repository.
7. Accepted C1 human-reviewed root:
   - `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json`
   - `verification/corpus/semantic/v1/g1-04-c/authoring/expected.json`
   - `verification/corpus/semantic/v1/g1-04-c/suites/core.json`

Inherited C1 closure refs:

```yaml
package_ref: a9561da5bc781f148d12c85f90c34f860734a042
source_ref: 851c4a5ec03313bfe6d6258a9d0269f808ce374b
materialized_ref: e1b3e1dbc897a4b77385e7a01f8e348af2796610
```

Current C1 accepted case count remains exactly `90`.

---

## 2. Task Anchor, Repair Base, and Execution Reconciliation

```yaml
task_anchor:
  revision: e1b3e1dbc897a4b77385e7a01f8e348af2796610
  relation: ancestor
  branch_hint: codex/gt-g1-04-operation-apply

repair_base:
  revision: 8bd7d9820503de366f2a541b0d3f16bbd4876c26
  role: historical failed C2 materialization to repair, not semantic Authority
```

`Task Anchor != Execution Cursor`.

Before any future P36 edit, the executor must:

1. fresh-fetch branch `codex/gt-g1-04-operation-apply`;
2. record actual `repair_execution_start_ref`;
3. prove `task_anchor.revision` is an ancestor;
4. prove `repair_base.revision` is an ancestor of the accepted start or explicitly reconcile the descendant delta;
5. inspect only descendant changes after `repair_base` and preserve compatible documentation-only/package changes;
6. return `BLOCKED_EXECUTION_DIVERGENCE` on incompatible history rewrite, unrelated implementation scope, or contradictory semantic delta.

A package-only descendant is valid and must not be rejected solely because HEAD advanced.

---

## 3. Existing Defect Reality to Repair

At failed source `78bfd50096ee42ee1f14724f13a9156d0a34d21b`:

- compiler: `verification/fixture-author/compile_g1_04_c.py`;
- focused tests: `verification/tests/test_g1_04_c_fixture_compiler.py`;
- generated tree exists with 90 inputs + 90 provenance records + manifest;
- reproducibility/trust-boundary tests pass, but case-intent realization was not tested;
- `C1-TRANSFORM-NAN-INF` is incorrectly generated with an all-finite identity transform;
- `C1-PATCH-DUPLICATE-FIELD` is incorrectly generated with only one patch;
- the same token-driven construction pattern may leave other accepted cases semantically indistinguishable from their positive counterparts.

The repair must audit all 90 accepted cases. It is not sufficient to patch only the two discovered examples.

---

## 4. Exact Authorized Repair Source Scope

### Modify only

```text
verification/fixture-author/compile_g1_04_c.py
verification/tests/test_g1_04_c_fixture_compiler.py
verification/corpus/semantic/v1/g1-04-c/generated/manifest.json
verification/corpus/semantic/v1/g1-04-c/generated/inputs/<accepted-case-id>.json
verification/corpus/semantic/v1/g1-04-c/generated/provenance/<accepted-case-id>.json
```

No other source/config path is authorized.

### Evidence-only creation after a new source commit exists

```text
verification/evidence/gates/G1/<new-source-ref>/GT-G1-04-C/C-FIXTURE-REPRO.json
```

### Read-only / forbidden to modify

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
runtime/semantic/**
verification/packages/semantic-conformance-cli/**
.github/workflows/g1-04-c-semantic-conformance.yml
```

Explicitly forbidden creation:

```text
verification/fixture-author/g1_04_c_authoring_v1.json
verification/corpus/semantic/v1/g1-04-c/generated/expected.json
verification/corpus/semantic/v1/g1-04-c/generated/**/expected*
any second human-authored expected/golden root
```

If repair requires an accepted C0/C1 path change, return `BLOCKED_UPSTREAM`.

---

## 5. Non-Finite f64 Fixture Carrier Contract

For an accepted semantic `double`/f64 field, C fixture INPUT may mechanically use:

```text
f64:<16 lowercase IEEE-754 binary64 hex digits>
pattern = ^f64:[0-9a-f]{16}$
```

This is verification-only input representation, not Product JSON/wire/storage/sync ABI.

Frozen representative tokens:

```text
quiet NaN   = f64:7ff8000000000000
+Infinity   = f64:7ff0000000000000
-Infinity   = f64:fff0000000000000
-0 raw bits = f64:8000000000000000
```

### C2 mechanical proof boundary

C2 may use Python stdlib `struct` to prove a token decodes to the declared 64-bit bit pattern and round-trips back to the same bits. This is a carrier test only.

C2 must **not**:

- infer semantic type from the `f64:` prefix alone;
- invoke a production decoder;
- construct Reference/Indexed stores;
- invoke stateless/stateful validators;
- assert runtime `REJECTED` behavior.

Those typed runtime decoding/execution responsibilities remain C3.

### Frozen `C1-TRANSFORM-NAN-INF` stimulus

The generated transform must be exactly:

```json
[
  "f64:7ff8000000000000",
  0.0,
  0.0,
  1.0,
  "f64:7ff0000000000000",
  "f64:fff0000000000000"
]
```

in the accepted Transform2D order `a,b,c,d,tx,ty`.

### `C1-SIZE-NONFINITE`

The case intent requires an actual non-finite size component. Use one deterministic representative permitted by the promoted carrier rule; freeze repair output to:

```text
width = f64:7ff0000000000000
height = 24.0
```

This is a stimulus choice within an already-frozen “non-finite” invalid family, not expected-outcome Authority.

Bare JSON `NaN`, `Infinity`, or `-Infinity` tokens are forbidden.

---

## 6. Case-Intent Realization Audit Contract

Add an explicit audit mechanism to `verification/tests/test_g1_04_c_fixture_compiler.py`.

Use this interface:

```python
REALIZATION_RULES: dict[str, object]

def build_case_intent_audit(cases_path: Path, generated_root: Path) -> list[dict[str, object]]:
    ...
```

Requirements:

1. `set(REALIZATION_RULES) == set(case["id"] for case in cases.json)`; missing or extra rule fails.
2. Each rule reads only the accepted case record + generated input. It must not accept/read an expected record.
3. Each rule asserts the **distinguishing stimulus condition**, not a desired outcome.
4. Every audit entry returns at least:

```text
caseId
operationFamily
authorityRuleRefs
generatedInputPath
generatedInputSha256
assertions[]
result = PASS
```

5. The audit helper must fail/raise on the first unrealized condition; it must not emit PASS for an unresolved rule.
6. The audit output itself is evidence data, not a new golden truth root and not a checked-in file under `generated/**`.

### Mandatory explicit predicates

The rule set must include these concrete conditions at minimum:

```text
C1-TRANSFORM-FINITE
  → all six transform components are finite-carrier-compatible finite values; no non-finite tagged token.

C1-TRANSFORM-NEGATIVE-ZERO
  → tx is raw negative zero as the accepted pre-normalization stimulus (`-0.0` or exact raw-bit carrier preserving -0).

C1-TRANSFORM-NAN-INF
  → transform equals the exact frozen six-element stimulus from section 5.

C1-PATCH-DUPLICATE-FIELD
  → payload contains at least two patches with the same `(object_id, field_id)` tuple.

C1-PATCH-FIELD-ID
  → selected `field_id` is the deliberately unpublished/invalid ID used by the fixture constructor.

C1-SIZE-NONFINITE
  → width equals `f64:7ff0000000000000` and height remains finite.

C1-SIZE-NONPOSITIVE
  → at least one size dimension is <= 0.

C1-DELETE-MISSING-TARGET
  → delete target is absent from `initialState.objects`.

C1-DELETE-DUPLICATE-TARGET
  → `object_ids` contains the same target ID more than once.

C1-PLACEMENT-CYCLE
  → generated placement produces the explicit cycle relation encoded by the accepted fixture recipe, not the same relation as `C1-PLACEMENT-VALID`.

C1-IDEMPOTENT-EQUIVALENT
  → `priorOperations` contains the same OperationId and semantically identical generated payload as the incoming operation.

C1-ID-COLLISION
  → `priorOperations` contains the same OperationId but a deliberately different generated payload from the incoming operation.

C1-RESTORE-LOCAL-REPLAY-REMOTE
  → `executionVariants` is exactly `[local, replay, remote]` in deterministic order.

C1-INSERT-STAGED-PARENT / C1-INSERT-STAGED-CONNECTOR / C1-RESTORE-STAGED-PARENT-CHILD / C1-RESTORE-STAGED-CONNECTOR
  → the generated operation actually references records staged in the same operation as required by the case ID, rather than merely placing unrelated objects in initial state.

C1-INSERT-HIERARCHY-CYCLE
  → staged insert placements actually encode a cycle.

C1-INSERT-STICKY-CARDINALITY
  → generated staged records actually encode the Sticky direct-child cardinality stressor required by the accepted case intent.

C1-PATCH-BRANCH-TYPE
  → generated patch uses a value branch/type that is mechanically inconsistent with the selected published field type.

C1-PATCH-APPLICABILITY
  → generated target kind + selected field form the accepted non-applicable combination.

C1-SIZE-WRONG-KIND / C1-GEOMETRY-WRONG-KIND / C1-IMAGE-WRONG-KIND
  → initial target kind is actually incompatible with the operation family under the accepted published kind/capability metadata.

C1-GEOMETRY-STRUCTURAL
  → generated geometry contains the accepted structural invalidity and is not byte-equivalent to the valid geometry fixture.

C1-CONNECTOR-INVALID-END
  → generated connector content actually contains the accepted invalid endpoint form.
```

For every other accepted case ID, add a similarly explicit predicate derived from its case ID + direct Authority refs. If any such predicate cannot be written without inventing semantics, stop with `BLOCKED_UPSTREAM` and name the case.

### Anti-oracle guard for realization rules

Add a static test that rejects these tokens/flows in realization functions:

```text
disposition
terminalPhase
semanticErrorCategory
logicalPlanProjection
mutationExpected
PLAN_READY
ALREADY_APPLIED
REJECTED
ReferenceObjectStore
IndexedObjectStore
OperationEngine
prepareApplyPlan
```

The realization audit may hash/reference `expected.json` only through the existing provenance mechanism; it must not use expected values to choose/assert stimulus.

---

## 7. Compiler Repair Rules

Keep the existing deterministic IDs/order/serialization/provenance model unless a realization repair requires a local fixture-construction change.

Required repair behavior:

1. Extend `_payload` / `_initial_objects` / narrowly focused helpers so each accepted case has a distinct, Authority-supported stimulus.
2. Prefer named case-specific helper branches when generic token matching would make two semantically different cases indistinguishable.
3. Do not add production dependencies or shell execution to the compiler.
4. Keep `COMPILER_IDENTITY = "g1-04-c-independent-fixture-compiler-v1"` unless artifact schema/format changes. The promoted f64 carrier is an allowed value representation inside the existing verification input format, so no format-version bump is required by this package.
5. Generated input must still contain no expected outcomes, provider observations, or PASS/FAIL.
6. Provenance continues to bind case/expected source hashes and compiler hash without copying expected semantic answers into the generated input.

If a case requires published descriptor/registry metadata that is not already available to the current minimal-source compiler, do not silently import production code. Either use a published declarative metadata file already allowed by Current Authority or return `BLOCKED_UPSTREAM` if no accepted source exists.

---

## 8. Required Focused Tests — TDD

Preserve all existing 11 trust-boundary/reproducibility tests. Add at least the following tests with these names:

```text
test_nonfinite_f64_carrier_roundtrips_exact_bits
test_transform_nan_inf_fixture_realizes_current_authority
test_patch_duplicate_field_fixture_realizes_case_intent
test_size_nonfinite_fixture_realizes_case_intent
test_every_accepted_case_has_explicit_realization_rule
test_every_generated_fixture_satisfies_realization_rule
test_realization_audit_does_not_use_expected_outcome_or_production_semantics
```

### RED requirement

Before compiler repair, run the new focused suite and preserve a transcript showing at least:

```text
C1-TRANSFORM-NAN-INF realization test = FAIL
C1-PATCH-DUPLICATE-FIELD realization test = FAIL
```

The RED failure must be due to the known fixture defect, not missing infrastructure.

### GREEN command

From repository root:

```bash
python -m unittest verification.tests.test_g1_04_c_fixture_compiler
```

Expected after repair: all existing + new C2 tests PASS.

---

## 9. Reproducibility and Trust-Boundary Regression

All original C2 obligations remain mandatory:

```text
accepted case count = 90
generated input count = 90
generated provenance count = 90
clean generation A == generation B byte-for-byte
clean temp regeneration == committed generated tree byte-for-byte
manifest/inventory hashes recompute exactly
authoring output rejected before write
minimal-source root generation succeeds without runtime/semantic tree
production semantic dependency static guard PASS
implementation-derived expected = 0
generated expected truth = 0
C0/C1 accepted paths unchanged
```

The newly promoted f64 carrier must not weaken any of these checks.

---

## 10. P36 Repair Execution Sequence

A later, separate P36 authorization must instruct the executor to perform exactly this sequence.

### Task 0 — Fresh preflight

- [ ] Record `repair_execution_start_ref`.
- [ ] Prove task-anchor ancestry.
- [ ] Reconcile descendant delta from repair base.
- [ ] Verify only package/documentation descendants are present before edits.

### Task 1 — RED realization tests

- [ ] Modify only `verification/tests/test_g1_04_c_fixture_compiler.py`.
- [ ] Add non-finite carrier roundtrip + known-regression realization tests + all-case rule-coverage/audit tests.
- [ ] Run focused unittest and preserve RED proof for the known defects.

### Task 2 — Minimal compiler repair

- [ ] Modify only `verification/fixture-author/compile_g1_04_c.py`.
- [ ] Implement the frozen `C1-TRANSFORM-NAN-INF` carrier.
- [ ] Implement actual duplicate field stimulus.
- [ ] Repair all other unrealized cases discovered by the 90-case audit.
- [ ] Stop `BLOCKED_UPSTREAM` instead of inventing unsupported stimulus semantics.

### Task 3 — Regenerate derived tree

- [ ] Regenerate `generated/manifest.json`, all 90 `inputs/*.json`, all 90 `provenance/*.json`.
- [ ] Do not create expected/golden artifacts.

### Task 4 — Focused GREEN

- [ ] Run `python -m unittest verification.tests.test_g1_04_c_fixture_compiler`.
- [ ] Require all focused tests PASS.
- [ ] Run `build_case_intent_audit(...)`; require `accepted=90`, `audited=90`, `realized=90`, `unresolved=0`.

### Task 5 — Inherited workspace regression

- [ ] Run:

```bash
cd verification
npm run validate
```

- [ ] From repository root run:

```bash
git diff --check
```

- [ ] Verify all C0/C1 accepted files are byte-identical to task anchor.

### Task 6 — Source commit

- [ ] Commit exactly the authorized C2 repair source paths.
- [ ] Record the resulting `source_ref`.
- [ ] Confirm no evidence file is included in this source commit.

### Task 7 — Durable repair evidence

- [ ] Build `verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-FIXTURE-REPRO.json`.
- [ ] Include the full 90-case machine-readable realization audit from `build_case_intent_audit(...)`.
- [ ] Commit only the new evidence file.
- [ ] Push branch.
- [ ] Record `materialized_ref`; remote HEAD must resolve to it or to a legal descendant containing the same source/evidence pair.

Do not overwrite the failed evidence under `78bfd500...`.

---

## 11. New C-FIXTURE-REPRO Evidence Contract

The new repair evidence must bind at least:

```text
stage = P36
task = GT-G1-04-C/C2
occurrence = repair-2
superseded_package_ref = 9e23eedce264b94e4435d1198ae69a5878cb1e6f
package_ref = <this refreshed package commit>
task_anchor = e1b3e1dbc897a4b77385e7a01f8e348af2796610
repair_base = 8bd7d9820503de366f2a541b0d3f16bbd4876c26
repair_execution_start_ref
source_ref
compiler identity/hash
cases.json hash
expected.json hash
core.json hash
acceptedCaseCount = 90
generatedInputCount = 90
generatedProvenanceCount = 90
manifest path/hash
full generated inventory
cleanRegeneration = PASS
tempVsCommitted = PASS
deterministicHashes = PASS
noWriteToAuthorityRoot = PASS
productionSemanticIndependence = PASS
minimalSourceIndependence = PASS
implementationDerivedExpected = 0
generatedExpectedTruth = 0
c0C1ContractsChanged = false
focusedC2Tests = PASS
verificationWorkspaceRegression = PASS
gitDiffCheck = PASS
```

Add:

```yaml
caseIntentRealizationAudit:
  acceptedCaseCount: 90
  auditedCaseCount: 90
  realizedCaseCount: 90
  unresolvedCaseCount: 0
  implementationDerivedExpectedCount: 0
  generatedExpectedCount: 0
  entries:
    - caseId: <id>
      operationFamily: <family>
      authorityRuleRefs: [...]
      generatedInputPath: generated/inputs/<id>.json
      generatedInputSha256: <sha256>
      assertions: [<stimulus-only assertion descriptions>]
      result: PASS
```

Known regressions must appear explicitly as PASS entries:

```text
C1-TRANSFORM-NAN-INF
C1-PATCH-DUPLICATE-FIELD
C1-SIZE-NONFINITE
```

Evidence must not use expected disposition/terminal phase/runtime observation as the realization oracle.

---

## 12. Blocked Return Contract

Return immediately without widening scope when any of these occurs:

```text
BLOCKED_EXECUTION_DIVERGENCE
  anchor/repair-base ancestry cannot be reconciled or unexpected semantic implementation delta appears.

BLOCKED_UPSTREAM
  an accepted case needs a material stimulus/encoding/capability decision not uniquely supported by Current Authority;
  correct repair requires modifying accepted C0/C1 files;
  a second authoring/expected root would be required.

BLOCKED_AUTHORITY
  Current Authority refs conflict or cannot be resolved.

BLOCKED_SCOPE
  repair requires runtime/semantic, C3 observer/provider, workflow, package.json, schema, or other forbidden source changes.

BLOCKED_EVIDENCE
  exact source/evidence result cannot be durably materialized for independent review.
```

A blocker return must include the exact case/path/contract that caused the stop and must not create a fake PASS evidence artifact.

---

## 13. Explicit Non-Goals

Not authorized by this package:

- changing any C1 expected outcome or terminal phase;
- adding a 91st accepted C1 case;
- creating separate NaN/+Inf/-Inf semantic coverage cases;
- Product JSON/wire/protobuf/storage/sync/ABI changes;
- production stateless/stateful validation changes;
- Reference/Indexed provider execution;
- C3 type-directed runtime fixture decoder/observer;
- C4 coordinator;
- C5 runnable corpus/conformance execution;
- C6/C7/C8;
- Atomic Apply / GT-G1-05.

---

## 14. P31 Refresh Exit Criteria

This P31 refresh is complete only when:

```text
new package is durably committed on codex/gt-g1-04-operation-apply
new package explicitly supersedes 9e23eed...
Current Non-Finite Fixture Carrier Authority is referenced
repair_base 8bd7d982... is preserved
exact repair source scope is frozen
C2/C3 ownership split is frozen
90-case realization audit contract is frozen
known regression predicates are frozen
non-finite carrier exact bits are frozen
old evidence is preserved
P36 is still NOT AUTHORIZED
C3-C8 are still NOT AUTHORIZED
GT-G1-05 is still NOT AUTHORIZED
```

Future control flow after this package:

```text
P31 refresh materialization
→ separate C2 P36 repair-2 authorization
→ CODE_REVERIFY execution
→ durable new C-FIXTURE-REPRO evidence
→ P34 final re-review
```

Do not skip directly from this package to C3.

# GT-G1-04-C C0 P31 Corroboration Addendum

**Stage:** `GT-G1-04-C / P31 Task Packaging / C0 Authority Schema + Trust Boundary`.

**Base package:** `docs/superpowers/plans/2026-08-30-gt-g1-04-c-semantic-conformance.md@79826abbb8d52dc29edd0ecab498f1851d97174d`.

**Purpose:** Resolve two execution-readiness defects found during independent P31 corroboration without changing Current semantic Authority, C0 semantic scope, C1-C8 status, or P32 authorization state.

This addendum is authoritative over conflicting instructions in Sections 4 and 8 of the base package. All other base-package constraints remain unchanged.

## 1. Corroboration Findings

Repository ancestry and scope corroboration passed:

```text
task_anchor = ee2466099fb9e074eac5f558bf3e660c8792cae3
base_package = 79826abbb8d52dc29edd0ecab498f1851d97174d
relation = ancestor
base_package ahead_by anchor = 1
unrelated implementation changes = 0
```

Two P31 packaging defects were found:

### P31-C0-F01 — RED ordering was not mechanically guaranteed

The base plan created the full strict schema constraints in Task 2, then asked Task 3 to add authority/no-mutation negative tests and “verify RED”. Because Task 2 already requires `AUTHORITY_MANUAL`, non-empty `authorityRuleRefs`, and `mutationExpected=false`, those Task 3 tests could legitimately pass on first run. The same issue affected the `BLOCKED_OPEN` portion of Task 4 because the base expected-disposition enum already excluded it.

This is a task-packaging/TDD sequencing defect only. It does not change the C verification contract.

### P31-C0-F02 — closed-policy rejection was test-local rather than a reusable mechanical boundary

The base plan allowed the Connector/geometry CLOSED mapping to live only in a helper local to `g1_04_c_open_reconciliation.test.mjs`. Such a helper can prove the test author's intent, but future C authoring/coordinator tooling cannot reuse it, so it is weaker than C0's stated purpose of establishing a mechanical trust boundary.

Repository reality explains the issue: the existing lightweight `validateValue()` supports the strict JSON-shape keywords needed by C0, but not policy-aware `if/then` or equivalent conditional semantics. Refining C0 with one verification-only contract helper is therefore justified by repository reality and remains inside the authorized C0 verification scope.

## 2. Refined Exact Authorized File Scope

The base package Section 4.1 remains authorized, plus exactly one additional verification-only file:

```text
verification/tools/g1_04_c_contract.mjs
```

The full C0 implementation create set is therefore:

```text
verification/corpus/semantic/v1/g1-04-c/README.md

verification/schemas/semantic/g1-04-c-case.schema.json
verification/schemas/semantic/g1-04-c-expected.schema.json
verification/schemas/semantic/g1-04-c-observation.schema.json
verification/schemas/semantic/g1-04-c-result.schema.json
verification/schemas/semantic/g1-04-c-gate.schema.json

verification/tools/g1_04_c_contract.mjs

verification/tests/g1_04_c_schema.test.mjs
verification/tests/g1_04_c_authority_map.test.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
```

No existing shared verification file is authorized to change.

Still explicitly forbidden:

```text
verification/package.json
verification/tools/validate_schemas.mjs
verification/corpus/semantic/v1/corpus.json
verification/corpus/semantic/v1/fixture-manifest.json
runtime/semantic/**
.github/workflows/**
```

No C1 authoring file, fixture compiler, production observer, coordinator, production semantic file, or GT-G1-05 file is authorized.

## 3. Contract Helper Boundary

Create `verification/tools/g1_04_c_contract.mjs` as a verification-only cross-record/policy meta-contract. It is not Product ABI, wire schema, production validation, or a semantic-policy authority source.

It may export only the C0-level policy-status mechanism needed for the already-closed Current Authority facts, conceptually:

```js
export const CLOSED_CURRENT_POLICIES = new Set([
  "connector-target-delete",
  "geometry-point-like-elements-per-operation-aggregate",
]);

export function assertExpectedPolicyStatus(policyKey, expectedRecord) {
  if (CLOSED_CURRENT_POLICIES.has(policyKey) && expectedRecord.openPolicy === true) {
    throw new Error(`closed policy cannot be OPEN: ${policyKey}`);
  }
}
```

Required behavior:

- Connector target delete + `openPolicy: true` -> reject.
- Geometry aggregate + `openPolicy: true` -> reject.
- Those closed policies with `openPolicy` absent/false -> allowed by this policy-status check.
- Unknown/future policy key -> **not** auto-classified CLOSED. This helper must not invent a winner or policy status for unknown authority.
- The helper must not import production semantic code, B tests, ObjectStore code, observer output, or generated fixtures.
- The helper must not expose `--bless`, update-golden, capture-current-output, or any write path.

The two string keys above are verification contract labels for Current CLOSED decisions. They do not create Product semantic IDs.

## 4. Superseding TDD / Failing-First Sequence

This section supersedes the base package Task 1-4 ordering. Tasks 0, 5, and 6 from the base package remain applicable, with the changed-file allowlist extended by `verification/tools/g1_04_c_contract.mjs`.

### Task 1 — RED: write schema-role and authority-boundary tests before schemas exist

**Create first:**

```text
verification/tests/g1_04_c_schema.test.mjs
verification/tests/g1_04_c_authority_map.test.mjs
```

Both tests import existing `validateValue` from `verification/tools/validate_schemas.mjs` and load the future C0 semantic schema paths.

`g1_04_c_schema.test.mjs` must include:

- one minimal valid sample per record family;
- wrong `formatVersion` rejection;
- wrong provenance rejection;
- unknown top-level field rejection;
- observation cannot validate as expected;
- result cannot validate as case;
- gate cannot validate as expected.

`g1_04_c_authority_map.test.mjs` must include:

- valid CaseIntent with a non-empty Current Authority ref;
- missing/empty `authorityRuleRefs` rejection;
- blocking case without authority refs rejection;
- expected missing/empty authority refs rejection;
- `mutationExpected=true` rejection;
- expected provenance `DERIVED_GENERATED` rejection;
- expected provenance `IMPLEMENTATION_OBSERVATION` rejection;
- observation/result/gate cannot masquerade as authority-manual case/expected input.

Run before creating any C0 schema:

```bash
cd verification
node --test \
  tests/g1_04_c_schema.test.mjs \
  tests/g1_04_c_authority_map.test.mjs
```

Required RED observation:

```text
FAIL because verification/schemas/semantic/g1-04-c-*.schema.json does not yet exist
```

A different pre-existing infrastructure failure is not an acceptable RED signal; stop and classify it.

### Task 2 — GREEN: create README + five strict schemas

Create exactly the README and five schemas specified by the base package Sections 6-7.

Run:

```bash
cd verification
node --test \
  tests/g1_04_c_schema.test.mjs \
  tests/g1_04_c_authority_map.test.mjs
```

Required result: PASS.

This establishes strict record roles, manual authority provenance, authority refs, `mutationExpected=false`, and exclusion of `BLOCKED_OPEN` from the expected disposition enum.

### Task 3 — RED: write closed-policy reconciliation test before helper exists

Create:

```text
verification/tests/g1_04_c_open_reconciliation.test.mjs
```

The test must import the future:

```js
import { assertExpectedPolicyStatus } from "../tools/g1_04_c_contract.mjs";
```

Required cases:

1. `BLOCKED_OPEN` fails `g1-04-c-expected.schema.json` validation.
2. Connector target delete + `openPolicy:true` is rejected by `assertExpectedPolicyStatus`.
3. Geometry aggregate + `openPolicy:true` is rejected by `assertExpectedPolicyStatus`.
4. The same two policies with `openPolicy` absent/false pass the policy-status helper.
5. An unknown policy key is not silently promoted to CLOSED by the helper.

Run before creating the helper:

```bash
cd verification
node --test tests/g1_04_c_open_reconciliation.test.mjs
```

Required RED observation:

```text
FAIL because verification/tools/g1_04_c_contract.mjs does not yet exist
```

### Task 4 — GREEN: create the minimal reusable C0 policy-status helper

Create only:

```text
verification/tools/g1_04_c_contract.mjs
```

Implement only the behavior in Section 3 of this addendum.

Run:

```bash
cd verification
node --test tests/g1_04_c_open_reconciliation.test.mjs
```

Required result: PASS.

Then run the three-file focused C0 suite together:

```bash
cd verification
node --test \
  tests/g1_04_c_schema.test.mjs \
  tests/g1_04_c_authority_map.test.mjs \
  tests/g1_04_c_open_reconciliation.test.mjs
```

Required result: PASS.

## 5. Evidence Contract Refinement

The base evidence contract remains, with these additions to `C-AUTHORITY-MAP.json`:

```text
authorized C0 create set includes verification/tools/g1_04_c_contract.mjs
RED evidence records:
  - schema/authority tests failed before schemas existed
  - OPEN reconciliation test failed before g1_04_c_contract.mjs existed
GREEN evidence records:
  - schema/authority tests passed after schema creation
  - OPEN reconciliation passed after helper creation
```

`C-OPEN-RECONCILIATION.json` must identify `verification/tools/g1_04_c_contract.mjs` as the reusable C0 enforcement point for the two already-closed policy labels.

The P32 source-ref diff allowlist is the ten created implementation files listed in Section 2 of this addendum. The P31 plan files themselves predate `execution_start_ref` and are not implementation changes.

## 6. Corroborated P31 Exit Contract

After this addendum is materialized in Git and ancestry/scope are rechecked, the P31 package may be classified:

```text
GT-G1-04-C P31 / C0
= CORROBORATED
= READY_FOR_P32_AUTHORIZATION

package_ref
= immutable Git commit containing both the base package and this addendum

task_anchor
= ee2466099fb9e074eac5f558bf3e660c8792cae3
  relation = ancestor

authorized_scope
= C0 Authority Schema + Trust Boundary only

C1-C8
= NOT_PACKAGED / NOT_AUTHORIZED

P32
= NOT_EXECUTED
= requires explicit surface_handoff carrying package_ref

GT-G1-05
= NOT_AUTHORIZED
```

This addendum does not itself execute or authorize P32. It only makes the P31 package mechanically executable and reviewable.

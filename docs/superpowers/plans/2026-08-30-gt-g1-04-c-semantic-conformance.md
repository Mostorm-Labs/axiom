# GT-G1-04-C C0 Authority Schema + Trust Boundary — P31 Task Package

> **For agentic workers:** This file is a **P31 review package, not a P32 authorization**. Do not execute it until a separate Aegis P32 authorization carries this exact package commit as `package_ref`. After authorization, use `superpowers:test-driven-development` and `superpowers:executing-plans` or `superpowers:subagent-driven-development` for coding mechanics.

**Goal:** Establish the GT-G1-04-C verification-only authority/trust-domain schemas and negative/meta-contract tests so later C tooling cannot confuse human-reviewed expected truth, generated artifacts, implementation observations, conformance results, or Gate evidence.

**Architecture:** C0 creates only verification-layer schemas, a corpus trust-boundary README, and schema/meta-tests. It does not create the human-reviewed C corpus, generated fixtures, production observers, comparison coordinators, production semantic changes, or any apply/post-state behavior. The schemas lock provenance and record-role boundaries; cross-record rules that the existing lightweight validator cannot express are exercised explicitly as meta-contract tests.

**Tech Stack:** Existing `verification/` Node ESM workspace, Node `node:test`, existing `verification/tools/validate_schemas.mjs::validateValue`, JSON Schema Draft 2020-12, Markdown.

**Spec / Current C Verification Authority:** Notion page `3cc4c57a-590c-81ae-ab73-d75501c47169` — `GT-G1-04-C P20 Verification Design Reconciliation v0.1`, promoted by P23.

**P30 Implementation-Plan Authority:** Notion page `3cc4c57a-590c-81c4-9e7b-d404c3fdba4b` — `GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31`.

**P23 Promotion Record:** Notion page `3cc4c57a-590c-817b-bcc6-e545279a4722`.

**Package identity:** The immutable Git commit containing this file is the P31 `package_ref`. The control plane must resolve and return that commit after materialization; this document does not self-invent a SHA.

## Global Constraints

- Authorized stage/slice is exactly `GT-G1-04-C / P31 / C0 Authority Schema + Trust Boundary`.
- `task_anchor.revision = ee2466099fb9e074eac5f558bf3e660c8792cae3`, `task_anchor.relation = ancestor`, `branch_hint = codex/gt-g1-04-operation-apply`.
- `Task Anchor != Execution Cursor`; a later legal descendant is reconciled, not rejected merely because HEAD advanced.
- C ends at `Operation -> Normalize/Canonicalize -> Stateless Validation -> OperationId Idempotency -> Stateful Validation -> PreparedApplyPlan`.
- C0 must remain independently testable without a production observer, ObjectStore execution, C++ build, or mutation/apply code.
- Production output, Reference provider output, Indexed provider output, existing B tests/results, and generated fixture output are never expected truth.
- Every manual expected record is authority-derived. Every blocking case must have Current Authority references; C0 will require authority refs on all manual case/expected records, which is compatible with the stronger oracle rule and avoids a conditional hole in the lightweight validator.
- `semanticErrorCategory` remains an opaque verification string in C0. C0 must not enumerate B-internal `StatefulIssue` values or invent a Product ABI error taxonomy.
- `mutationExpected` is invariantly `false` for GT-G1-04-C expected records.
- Connector target delete is CLOSED as deterministic Cascade Connector Delete. `BLOCKED_OPEN` is not a valid C expected disposition.
- Geometry point-like aggregate accounting is CLOSED at `2,000,000`; C0 must reject stale OPEN classification for that policy but must not author the N-1/N/N+1 corpus itself.
- No `--bless`, `--update-golden`, accept-current-output, capture-to-expected, or equivalent path is authorized.
- The future single human-reviewed source is exactly `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json` plus `authoring/expected.json`; C0 documents those paths but does not create them.
- Do not create or search for a `B11` implementation package. `RST-B11` remains only a Restore semantic/conformance test identifier.
- No Atomic Apply, `SemanticGeneration`, `ChangeSet`, `CanonicalCommitStamp`, local publication/no-echo, final applied-state oracle, or GT-G1-05 work.
- No production semantic source/header changes.

---

# 1. P31 Package Status and Authorization Boundary

```text
GT-G1-04-C P31 / C0
= PACKAGE MATERIALIZATION TARGET
= REVIEWABLE CONTROL ARTIFACT

C1-C8
= NOT_PACKAGED / NOT_AUTHORIZED

P32
= NOT_AUTHORIZED BY THIS FILE

GT-G1-05
= NOT_AUTHORIZED
```

This package becomes `READY_FOR_REVIEW` only after the control plane materializes it as an immutable reviewer-resolvable Git commit and verifies the package commit is a legal descendant of the task anchor with no unrelated repository changes.

A later P32 authorization must carry:

```yaml
type: surface_handoff
stage: P32
stage_owner: aegis-implementation
from_surface: CONTROL_REASONING
to_surface: CODE_EXECUTION
preferred_executor: codex
package_ref: <the immutable commit containing this file>
task_anchor:
  revision: ee2466099fb9e074eac5f558bf3e660c8792cae3
  relation: ancestor
resume_cursor: null
return_surface: CONTROL_REVIEW
```

The above is a handoff contract template only. This P31 occurrence does not issue the handoff.

# 2. Current Authority Inputs

C0 consumes these as already resolved and must not redesign them:

1. `GT-G1-04-C P20 Verification Design Reconciliation v0.1` — page `3cc4c57a-590c-81ae-ab73-d75501c47169` — Current C Verification Authority.
2. `GT-G1-04-C P23 Authority Supersession + Promotion v0.1 — PASS` — page `3cc4c57a-590c-817b-bcc6-e545279a4722` — promotion establishing P20 as Current Authority.
3. `GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31` — page `3cc4c57a-590c-81c4-9e7b-d404c3fdba4b` — this occurrence's implementation-plan authority.
4. Current GT-G1-04 semantic freeze, referenced by P20 as page `3c94c57a-590c-8153-955e-dfbb4b6b7da3`.
5. Current geometry aggregate closure, referenced by P20 as page `3c94c57a-590c-81e1-aea7-eae7ea8a8c88`.
6. Current Restore authority, referenced by P20 as page `3ca4c57a-590c-8150-b3bd-cb1d51eb0b83`.
7. Current hierarchy capability/cardinality authority, referenced by P20 as page `3cb4c57a-590c-815a-b8fa-cd785a837da7`.

C0 does not author semantic cases from items 4-7. They are listed so the trust-boundary contract remains grounded and so stale OPEN meta-tests do not invent policy status.

# 3. Repository Reality and Dependency Assumptions

The accepted repository baseline already provides:

- `verification/package.json` as the existing Node ESM verification workspace.
- `verification/tools/validate_schemas.mjs`, which exports `validateValue(schema, value)` and supports the C0 keywords needed here: `$ref` to local schema fragments, `const`, `enum`, `type`, `minLength`, `pattern`, `minItems`, `uniqueItems`, `items`, `required`, and `additionalProperties`.
- `verification/tests/schema_meta.test.mjs` as the established `node:test` pattern.
- `verification/schemas/platform/` as an existing schema family using `urn:auditoryworks:axiom:verification:*:v1` IDs and strict top-level objects.
- `verification/corpus/semantic/v1/` as the existing semantic corpus root.

C0 must reuse these boundaries and must **not**:

- add AJV or another JSON-schema engine;
- modify the platform schema inventory in `verification/tools/validate_schemas.mjs` merely to register semantic C schemas;
- create a second verification workspace;
- modify `verification/package.json` merely to make C0 executable. C0 has explicit direct test commands; workspace integration belongs to a separately authorized later slice if needed.

Because the current lightweight validator does not implement cross-document or policy-aware conditional validation, C0 uses two layers deliberately:

```text
JSON schema
  -> strict record shape, record role, provenance, enum/type constraints

C0 meta-contract tests
  -> cross-record authority binding and known-closed OPEN reconciliation
```

That is a verification-layer implementation detail, not a new semantic rule.

# 4. Exact Authorized Repository Scope

## 4.1 Create

```text
verification/corpus/semantic/v1/g1-04-c/README.md

verification/schemas/semantic/g1-04-c-case.schema.json
verification/schemas/semantic/g1-04-c-expected.schema.json
verification/schemas/semantic/g1-04-c-observation.schema.json
verification/schemas/semantic/g1-04-c-result.schema.json
verification/schemas/semantic/g1-04-c-gate.schema.json

verification/tests/g1_04_c_schema.test.mjs
verification/tests/g1_04_c_authority_map.test.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
```

## 4.2 Evidence created only after authorized P32 implementation/tests

Let `source_ref` be the commit containing only the authorized C0 README/schema/test implementation, before evidence materialization. Create:

```text
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-AUTHORITY-MAP.json
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-OPEN-RECONCILIATION.json
```

Then commit those exact evidence artifacts on the remote branch. That later evidence commit is the P32 `materialized_ref` returned to review.

## 4.3 Modify

None outside the created C0 files and the later evidence paths above.

In particular, C0 does not modify:

```text
verification/package.json
verification/tools/validate_schemas.mjs
verification/corpus/semantic/v1/corpus.json
verification/corpus/semantic/v1/fixture-manifest.json
runtime/semantic/**
.github/workflows/**
```

If implementation discovers one of those modifications is required to make the C0 contract correct, stop and return for scope review instead of silently widening the package.

# 5. Explicit Non-Goals

C0 does not create or populate:

```text
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/corpus/semantic/v1/g1-04-c/generated/**
verification/fixture-author/compile_g1_04_c.py
runtime/semantic/tools/g1_04_c_observer.cpp
runtime/semantic/tools/g1_04_c_projection.*
verification/packages/semantic-conformance-cli/**
.github/workflows/g1-04-c-semantic-conformance.yml
```

Those belong to C1, C2, C3, C4/C7, and C8 respectively.

C0 also does not:

- enumerate the 15-operation corpus;
- create golden vectors;
- define production wire formats;
- define Product ABI types;
- define production error numbers/codes;
- compare Reference vs Indexed behavior;
- invoke `OperationEngine::prepare()`;
- inspect or mutate canonical ObjectStore state;
- run Atomic Apply;
- create final C Gate PASS/FAIL evidence.

# 6. Trust-Domain Record Contracts

All five schemas use:

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "$id": "urn:auditoryworks:axiom:verification:<record-family>:v1",
  "type": "object",
  "additionalProperties": false
}
```

All schemas have a `format` constant, `formatVersion: 1`, and a provenance constant. These constants are verification-only and do not become Product ABI.

## 6.1 CaseIntent — `g1-04-c-case.schema.json`

Required fields:

```text
format              = axiom-g1-04-c-case-v1
formatVersion       = 1
provenance          = AUTHORITY_MANUAL
id                  = non-empty string
operationFamily     = non-empty string
authorityRuleRefs[] = unique non-empty strings, minItems 1
inputRef            = non-empty string
expectedRef         = non-empty string
blocking            = boolean
```

C0 deliberately does not enumerate `operationFamily`; C1 owns case coverage and may only use Current semantic authority.

Representative schema shape:

```json
{
  "required": [
    "format", "formatVersion", "provenance", "id", "operationFamily",
    "authorityRuleRefs", "inputRef", "expectedRef", "blocking"
  ],
  "properties": {
    "format": { "const": "axiom-g1-04-c-case-v1" },
    "formatVersion": { "const": 1 },
    "provenance": { "const": "AUTHORITY_MANUAL" },
    "id": { "type": "string", "minLength": 1 },
    "operationFamily": { "type": "string", "minLength": 1 },
    "authorityRuleRefs": {
      "type": "array",
      "minItems": 1,
      "uniqueItems": true,
      "items": { "type": "string", "minLength": 1 }
    },
    "inputRef": { "type": "string", "minLength": 1 },
    "expectedRef": { "type": "string", "minLength": 1 },
    "blocking": { "type": "boolean" }
  }
}
```

## 6.2 CExpectedOutcome — `g1-04-c-expected.schema.json`

Required trust fields:

```text
format              = axiom-g1-04-c-expected-v1
formatVersion       = 1
provenance          = AUTHORITY_MANUAL
caseId              = non-empty string
authorityRuleRefs[] = unique non-empty strings, minItems 1
mutationExpected    = false
```

Semantic fields:

```text
disposition? = PLAN_READY | ALREADY_APPLIED | REJECTED
terminalPhase? = NORMALIZE | STATELESS_VALIDATE | IDEMPOTENCY | STATEFUL_VALIDATE | PREPARE
semanticErrorCategory? = non-empty string, only when authority supports it
logicalPlanProjection? = object, exact projection shape belongs to later C3/C4 packages
openPolicy? = boolean
```

`disposition` and `terminalPhase` are schema-optional in C0 so a future genuinely OPEN authority item is not forced to invent a winner. C0 current closed-case meta-tests require normal expected records to carry them. Later C4 owns the complete OPEN observation-only comparison behavior.

Hard invariants:

- `mutationExpected` is encoded as `{ "const": false }`.
- `provenance` is encoded as `{ "const": "AUTHORITY_MANUAL" }`.
- `BLOCKED_OPEN` is not a disposition enum member.
- `semanticErrorCategory` is not an enum in C0.

## 6.3 ImplementationObservation — `g1-04-c-observation.schema.json`

Required fields:

```text
format                 = axiom-g1-04-c-observation-v1
formatVersion          = 1
provenance             = IMPLEMENTATION_OBSERVATION
caseId                 = non-empty string
provider               = reference | indexed
observedDisposition    = PLAN_READY | ALREADY_APPLIED | REJECTED
observedTerminalPhase  = NORMALIZE | STATELESS_VALIDATE | IDEMPOTENCY | STATEFUL_VALIDATE | PREPARE
beforeProjection       = object
afterProjection        = object
```

Optional:

```text
observedErrorCategory?
observedPlanProjection?
```

The schema must not admit `authorityRuleRefs`, `mutationExpected`, `expected`, `pass`, `verdict`, or another field that would let an observation masquerade as expected truth or coordinator output.

## 6.4 ConformanceResult boundary — `g1-04-c-result.schema.json`

C0 freezes only trust-domain envelope identity, not C4's final comparison taxonomy.

Required fields:

```text
format          = axiom-g1-04-c-result-v1
formatVersion   = 1
provenance      = CONFORMANCE_RESULT
caseId          = non-empty string
status          = non-empty string
expectedRef     = non-empty string
observationRefs = unique non-empty strings, minItems 1
```

Optional:

```text
diagnostics[] = strings
```

C4 may later narrow `status` or add comparison detail through its own reviewed package, but it may not weaken provenance or allow a result to become authority-manual input.

## 6.5 GateEvidence boundary — `g1-04-c-gate.schema.json`

C0 freezes only trust-domain envelope identity, not C7/C8's final Gate taxonomy.

Required fields:

```text
format        = axiom-g1-04-c-gate-v1
formatVersion = 1
provenance    = GATE_EVIDENCE
gateId        = non-empty string
status        = non-empty string
resultRefs[]  = unique non-empty strings, minItems 1
```

Optional:

```text
authorityRefs[] = unique non-empty strings
sourceRef?      = lowercase 40-char git SHA string
```

This record is evidence, never Current Authority.

# 7. README Contract

Create `verification/corpus/semantic/v1/g1-04-c/README.md` with these normative C0 statements:

1. C scope ends at `PreparedApplyPlan`; no mutation/apply/post-state semantics.
2. Trust path is exactly:

```text
Current Semantic Authority
  -> AUTHORITY_MANUAL
  -> independent mechanical generation (future C2, DERIVED_GENERATED)
  -> implementation observation
  -> coordinator result
  -> gate evidence
```

3. `AUTHORITY_MANUAL`, `DERIVED_GENERATED`, `IMPLEMENTATION_OBSERVATION`, `CONFORMANCE_RESULT`, and `GATE_EVIDENCE` are disjoint trust roles.
4. Future single manual authoring root is exactly `authoring/cases.json` and `authoring/expected.json`; those files do not exist as part of C0.
5. Production/reference/indexed outputs can never write or update authoring expected truth.
6. No bless/update-golden/accept-current-output mechanism is permitted.
7. Every C expected record has `mutationExpected=false`; later observation must prove before/after canonical projections equal.
8. Closed policy notice: Connector target delete and geometry aggregate are CLOSED; stale OPEN treatment is invalid.
9. B tests/results are coverage references only, never oracle sources.
10. C0 schemas are verification contracts, not Product ABI or wire-format commitments.

# 8. TDD / Failing-First Implementation Sequence

A later authorized P32 executes these steps in order. Each task is small enough to stop/review independently; none expands C0 scope.

## Task 0 — Execution preflight, no edits

**Files:** none.

- [ ] Resolve current branch HEAD and record it as `execution_start_ref`.
- [ ] Verify `ee2466099fb9e074eac5f558bf3e660c8792cae3` is an ancestor of `execution_start_ref`.
- [ ] If HEAD is a legal descendant, inspect only the descendant delta for C0 scope impact. Do not replay B0-B10 or P20/P21/P23/P30.
- [ ] If ancestry cannot be established, history is rewritten, or descendant semantic changes invalidate the package assumptions, return `BLOCKED_EXECUTION_DIVERGENCE` and stop.
- [ ] Verify this package commit is resolvable and is the exact P32 `package_ref`.

## Task 1 — RED: trust-domain schema inventory and strict record roles

**Files:**
- Create: `verification/tests/g1_04_c_schema.test.mjs`
- Later create in GREEN: all five C0 schema files and README.

**Consumes:** existing `validateValue` from `verification/tools/validate_schemas.mjs`.

**Produces:** schema tests proving the five record roles are strict and non-interchangeable.

- [ ] Write `g1_04_c_schema.test.mjs` first. The test must load the five exact schema paths and define minimal valid samples for each record family.
- [ ] Include failing assertions that initially fail because the schema files do not exist.
- [ ] Include positive validation for one minimal valid record per schema.
- [ ] Include negative validation for wrong `formatVersion`, wrong provenance, unknown top-level field, observation passed to expected schema, result passed to case schema, and gate passed to expected schema.
- [ ] Run:

```bash
cd verification
node --test tests/g1_04_c_schema.test.mjs
```

Expected before GREEN: **FAIL because C0 semantic schemas do not exist.**

## Task 2 — GREEN: create README and five minimal strict schemas

**Files:**
- Create: `verification/corpus/semantic/v1/g1-04-c/README.md`
- Create: `verification/schemas/semantic/g1-04-c-case.schema.json`
- Create: `verification/schemas/semantic/g1-04-c-expected.schema.json`
- Create: `verification/schemas/semantic/g1-04-c-observation.schema.json`
- Create: `verification/schemas/semantic/g1-04-c-result.schema.json`
- Create: `verification/schemas/semantic/g1-04-c-gate.schema.json`

- [ ] Implement only the fields/contracts in Sections 6-7.
- [ ] Use strict top-level objects and unique `urn:auditoryworks:axiom:verification:*:v1` `$id` values.
- [ ] Do not use external `$ref`, `if/then`, or another keyword unsupported by the existing lightweight validator for a rule that C0 tests depend on.
- [ ] Run:

```bash
cd verification
node --test tests/g1_04_c_schema.test.mjs
```

Expected: **PASS**.

- [ ] Commit the completed Task 1/2 red-green unit with a C0-only message; do not include production files.

## Task 3 — RED/GREEN: authority-manual and no-mutation contract

**Files:**
- Create: `verification/tests/g1_04_c_authority_map.test.mjs`
- Modify only if needed: case/expected schema files from Task 2.

**Required cases:**

1. valid manual CaseIntent with one authority ref passes;
2. CaseIntent with empty/missing `authorityRuleRefs` fails;
3. blocking manual case without authority refs fails;
4. expected record with empty/missing `authorityRuleRefs` fails;
5. expected record with `mutationExpected=true` fails;
6. expected record with provenance `DERIVED_GENERATED` fails;
7. expected record with provenance `IMPLEMENTATION_OBSERVATION` fails;
8. ImplementationObservation cannot validate as expected truth;
9. ConformanceResult/GateEvidence cannot validate as CaseIntent or CExpectedOutcome.

Representative negative checks:

```js
assert.throws(() => validateValue(expectedSchema, {
  ...validExpected,
  mutationExpected: true,
}), /const mismatch/);

assert.throws(() => validateValue(expectedSchema, {
  ...validExpected,
  provenance: "IMPLEMENTATION_OBSERVATION",
}), /const mismatch/);
```

- [ ] Write the test before any schema adjustment needed for these constraints.
- [ ] Run it and verify RED for at least the missing constraint being introduced.
- [ ] Make the smallest schema correction.
- [ ] Run:

```bash
cd verification
node --test tests/g1_04_c_authority_map.test.mjs
```

Expected: **PASS**.

## Task 4 — RED/GREEN: closed-policy OPEN reconciliation

**Files:**
- Create: `verification/tests/g1_04_c_open_reconciliation.test.mjs`
- Modify only if needed: `g1-04-c-expected.schema.json`.

C0 must not build a corpus or semantic winner table here. This test is a meta-contract linter for already-closed Current Authority facts.

Test at least:

1. `disposition: "BLOCKED_OPEN"` fails schema validation because it is not a valid C disposition;
2. a representative Connector target-delete record marked `openPolicy: true` is rejected by the C0 closed-policy meta rule;
3. a representative geometry aggregate record marked `openPolicy: true` is rejected by the C0 closed-policy meta rule;
4. closed-policy records with `openPolicy` absent/false and a valid Current Authority ref are accepted by the meta rule;
5. the meta rule does not claim to resolve arbitrary future policy names; unknown policy status must not be auto-promoted to CLOSED by convenience.

The test-local closed-policy keys are verification test labels only, not new semantic IDs:

```js
const CLOSED_CURRENT_POLICIES = new Set([
  "connector-target-delete",
  "geometry-point-like-elements-per-operation-aggregate",
]);
```

A helper local to this meta-test may reject `openPolicy === true` for those two keys. Do not create a shared coordinator module in C0; reusable corpus/coordinator validation belongs to later slices.

- [ ] Write the failing meta-test first.
- [ ] Run:

```bash
cd verification
node --test tests/g1_04_c_open_reconciliation.test.mjs
```

- [ ] Add only the minimal expected-schema constraint needed to reject `BLOCKED_OPEN`; keep the Current-Authority policy mapping in the meta-test.
- [ ] Re-run and require PASS.

## Task 5 — C0 focused verification and regression check

Run exactly:

```bash
cd verification
node --test \
  tests/g1_04_c_schema.test.mjs \
  tests/g1_04_c_authority_map.test.mjs \
  tests/g1_04_c_open_reconciliation.test.mjs
```

Expected: all C0 tests PASS.

Then run the existing verification workspace regression suite without editing its script:

```bash
cd verification
npm run validate
```

Expected: existing verification validation remains PASS.

Then from repository root:

```bash
git diff --check
```

Expected: PASS.

No C++ build or semantic CTest is required for C0 because no C++/production file is authorized to change. If C0 implementation touches production C++ such that a C++ build becomes necessary, that is a scope violation, not a reason to add a build step.

## Task 6 — Scope diff and evidence materialization

- [ ] Commit the C0 README/schema/test implementation. Record that commit as `source_ref`.
- [ ] Verify the exact changed-file set from the P32 execution start to `source_ref` is limited to the C0 authorized create paths in Section 4.1.
- [ ] Create `C-AUTHORITY-MAP.json` under the exact source-ref evidence directory. It must record:
  - `source_ref`;
  - P31 `package_ref`;
  - task anchor revision/relation;
  - Current Authority/P30 page IDs from Section 2;
  - exact C0 schema/test file list;
  - provenance role constants;
  - focused test command and result;
  - existing `npm run validate` result;
  - confirmation `production_semantic_files_changed = false`.
- [ ] Create `C-OPEN-RECONCILIATION.json` recording Current policy status checks for Connector target delete and geometry aggregate, with test names/results and authority refs. It must not contain a generated semantic winner or implementation observation.
- [ ] Validate both evidence JSON files parse and accurately match the repository/test facts.
- [ ] Commit only the evidence files. Push the branch.
- [ ] Return the remote reviewer-resolvable evidence commit as `materialized_ref`.

If the exact result cannot be pushed/materialized for independent review, return `BLOCKED_EVIDENCE`; a local commit, worktree path, or chat transcript is not sufficient.

# 9. Verification Oracle for C0

C0's oracle is **contract structure**, not production behavior.

Primary oracle inputs:

```text
P20 Current C Verification Authority
+ P30 C0 slice contract
+ this reviewed P31 package
```

C0 PASS means only:

- trust roles are mechanically distinguishable;
- manual case/expected records cannot be implementation-derived by provenance;
- expected records cannot claim mutation;
- observation/result/gate records cannot masquerade as manual authority input;
- stale `BLOCKED_OPEN` cannot be a C expected disposition;
- known closed Connector/geometry policies cannot be tagged OPEN by the C0 meta-contract;
- C0 runs without production observer/application logic.

C0 PASS does **not** prove any of the 15 Operations conform. That begins only after later authorized slices.

# 10. Required Evidence Contract

A later P32 result is not review-ready unless all of the following exist at a reviewer-resolvable remote ref:

1. exact C0 source/schema/test commit (`source_ref`);
2. exact evidence commit (`materialized_ref`);
3. `C-AUTHORITY-MAP.json` under the source-ref evidence path;
4. `C-OPEN-RECONCILIATION.json` under the same path;
5. focused C0 Node test PASS;
6. existing `verification/npm run validate` PASS;
7. `git diff --check` PASS;
8. changed-file proof showing no production semantic files, C1 corpus, fixture compiler, observer, coordinator, CI workflow, or GT-G1-05 files changed.

Existing B tests/results may be mentioned only as regression/coverage context and must not appear as expected-oracle provenance.

# 11. Exit Criteria

C0 implementation may later be returned for P34 review only when:

```text
five C0 schemas exist and are strict
AND C corpus README exists
AND C0 schema/meta tests pass
AND existing verification regression suite passes
AND mutationExpected=true is rejected
AND manual expected provenance != AUTHORITY_MANUAL is rejected
AND observation/result/gate masquerading is rejected
AND BLOCKED_OPEN disposition is rejected
AND closed Connector/geometry OPEN tagging is rejected
AND no production semantic file changed
AND no C1-C8 implementation was added
AND exact evidence is durably materialized remotely
```

The expected control return after **this P31 packaging occurrence**, before any P32 execution, is:

```text
GT-G1-04-C P31 / C0
= READY_FOR_REVIEW

package_ref
= immutable Git commit containing this package

task_anchor
= ee2466099fb9e074eac5f558bf3e660c8792cae3
  relation = ancestor

authorized_scope
= C0 Authority Schema + Trust Boundary only

C1-C8
= NOT_PACKAGED / NOT_AUTHORIZED

P32
= NOT_AUTHORIZED

GT-G1-05
= NOT_AUTHORIZED
```

# 12. Fail-Closed / Scope-Change Triggers

Stop and return to Aegis control/governance handling if any of the following is discovered:

- a blocking expected field cannot be defined from Current Authority without inventing semantic truth;
- Current authorities materially contradict one another;
- C0 would need to enumerate or promote B-internal `StatefulIssue` values as C semantic truth;
- C0 would need to author `cases.json`, `expected.json`, suite membership, or golden vectors;
- C0 would need to invoke or modify production validator/planner/ObjectStore code;
- C0 would need a production observer or coordinator to pass its own tests;
- C0 would need Atomic Apply/post-state semantics;
- the accepted task anchor is not an ancestor of the execution start or repository history has unexpectedly diverged;
- a required C0 rule cannot be implemented without modifying out-of-scope shared verification infrastructure;
- evidence cannot be durably materialized for independent review.

Use `BLOCKED_EXECUTION_DIVERGENCE` for invalid repository ancestry/history, `BLOCKED_EVIDENCE` for materialization failure, or route the discovered earlier authority defect back through Aegis governance. Do not repair upstream truth inside C0.

# 13. Self-Review Checklist for P31

Before treating this package as reviewable, the control plane must verify:

- [x] Scope is C0 only; C1-C8 are explicitly excluded.
- [x] Current Authority, P23, P30, and task anchor are named.
- [x] Exact files are enumerated.
- [x] Single authoring-root decision is preserved and C1 files are not created.
- [x] C ends before mutation/apply.
- [x] Manual/generation/observation/result/gate trust roles are disjoint.
- [x] Production output/reference/indexed output cannot be expected truth.
- [x] No bless/update-golden path is authorized.
- [x] Connector target delete and geometry aggregate are treated as CLOSED.
- [x] TDD failing-first steps are executable with repository-existing tools.
- [x] C0 is independently testable without a production observer.
- [x] Evidence-materialization obligation and fail-closed behavior are explicit.
- [x] No P32 authorization or Codex execution is issued by this package.

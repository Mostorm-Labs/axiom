# GT-G1-04-C C1 Human-Reviewed Authoring Root — P31 Task Package

> **For agentic workers:** This file is a **P31 review package, not a P32 authorization**. Do not execute it until a separate Aegis P32 surface handoff carries the immutable commit containing this file as `package_ref`. After authorization, use `superpowers:test-driven-development` plus `superpowers:executing-plans` or `superpowers:subagent-driven-development` for coding mechanics.

**Goal:** Establish the single GT-G1-04-C human-reviewed `AUTHORITY_MANUAL` authoring root at the already-frozen `cases.json` / `expected.json` locations, with mechanically reviewable authority traceability and suite membership, before any independent fixture compiler, production observer, coordinator, or runnable C corpus exists.

**Architecture:** C1 materializes only declarative human-reviewed semantic intent and expected truth plus a verification-only root-integrity test. It consumes the C0 schemas and trust helper exactly as accepted, and it does not create generated inputs, observe production behavior, compare providers, or implement the C5/C6 runnable corpus. The authoring records may describe the Current-Authority-required intent families, but `inputRef` remains a logical future generated-artifact reference until C2; authoring truth must never be produced from implementation output.

**Tech Stack:** Existing `verification/` Node ESM workspace, Node `node:test`, existing `verification/tools/validate_schemas.mjs::validateValue`, accepted C0 JSON schemas, accepted `verification/tools/g1_04_c_contract.mjs`, JSON authoring records, Markdown control documentation.

**Spec / Current C Verification Authority:** Notion page `3cc4c57a-590c-81ae-ab73-d75501c47169` — `GT-G1-04-C P20 Verification Design Reconciliation v0.1`, promoted by P23.

**P23 Promotion Record:** Notion page `3cc4c57a-590c-817b-bcc6-e545279a4722`.

**P30 Implementation-Plan Authority:** Notion page `3cc4c57a-590c-81c4-9e7b-d404c3fdba4b` — `GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31`.

**Accepted upstream Gate:** Notion page `3cd4c57a-590c-8114-917e-d40243e668cc` — `GT-G1-04-C P34 C0 Gate Review v0.1 — PASS_WITH_FINDINGS / ACCEPTED_FOR_DOWNSTREAM`.

**Package identity:** The immutable Git commit containing this file is the C1 P31 `package_ref`. This document does not self-invent its own SHA; the control plane resolves the commit after materialization.

## Global Constraints

- Authorized stage/slice is exactly `GT-G1-04-C / C1 Human-Reviewed Authoring Root / P31 Task Packaging`.
- `task_anchor.revision = 5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539`, `task_anchor.relation = ancestor`, `branch_hint = codex/gt-g1-04-operation-apply`.
- The C1 anchor is the accepted C0 materialized state, not the historical C0 package ref and not the historical P30 anchor.
- `Task Anchor != Execution Cursor`; P32 records its actual `execution_start_ref`. A legal descendant is reconciled, not rejected merely because HEAD advanced.
- GT-G1-04-C ends at `Operation -> Normalize/Canonicalize -> Stateless Validation -> OperationId Idempotency -> Stateful Validation -> PreparedApplyPlan`.
- Atomic Apply, `SemanticGeneration`, `ChangeSet`, `CanonicalCommitStamp`, local publication/no-echo, post-apply state, final applied-state oracle, and all GT-G1-05 work are forbidden.
- Production output, Reference provider output, Indexed provider output, existing B tests/results, generated fixtures, and future observer output are never expected truth.
- The single human-reviewed authoring root is exactly:
  - `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json`
  - `verification/corpus/semantic/v1/g1-04-c/authoring/expected.json`
- C1 must preserve `provenance = AUTHORITY_MANUAL` and `mutationExpected = false` exactly as required by the accepted C0 schemas.
- Every blocking CaseIntent and every expected record must carry non-empty **direct, concrete Current Authority references**. Text-only labels such as `authority_basis = "current semantic authority"` are insufficient for new C1 records/evidence.
- Prefer a concrete Notion page UUID/URL in every `authorityRuleRefs` item; where the source exposes a stable rule/requirement ID such as `C-V02`, `RST-Bxx`, or a stable section name, append it to the page reference. Do not create a new semantic ID namespace merely for C1.
- `semanticErrorCategory` may be authored only where Current Authority freezes the category; B-internal `StatefulIssue` values are not automatically C contract.
- `logicalPlanProjection` may be authored only for authority-defined logical facts. Do not invent C3 projection shape, pointer/handle/container details, or implementation-specific data.
- Connector target delete is CLOSED as deterministic Cascade Connector Delete. Geometry point-like aggregate accounting is CLOSED at `2,000,000`; `1,999,999` and `2,000,000` pass, `2,000,001` is `GEOMETRY_LIMIT_EXCEEDED`, checked overflow is `INTEGER_OVERFLOW`. Neither may be authored as OPEN.
- Restore is current-state based; idempotency precedes restore-existence checks; staged graph validation is complete; Local/Replay/Remote share semantic behavior; no hidden tombstone/history/sync metadata may be required.
- Hierarchy remains Root→any, Group→any, Sticky→RichText only, Sticky direct RichText cardinality `0..1`, other kinds non-parent, empty Sticky legal.
- No `--bless`, `--update-golden`, accept-current-output, capture-production-output-to-expected, or equivalent mechanism is authorized.
- C1 may author semantic intent/expected records required by Current Authority, but it **does not implement the C5 runnable 15-operation corpus or C6 execution coverage**. No generated input payloads, fixture compiler, observer, coordinator, provider run, or conformance result is part of C1.
- No production semantic/ObjectStore file may change.
- C2-C8 and GT-G1-05 remain not authorized.

---

# 1. P31 Status and Authorization Boundary

```text
GT-G1-04-C C0
= PASS_WITH_FINDINGS
= CLOSED
= ACCEPTED_FOR_DOWNSTREAM

GT-G1-04-C C1 / P31
= PACKAGE MATERIALIZATION TARGET
= REVIEWABLE CONTROL ARTIFACT

C1 / P32
= NOT_AUTHORIZED BY THIS FILE

C2-C8
= NOT_PACKAGED / NOT_AUTHORIZED

GT-G1-05
= NOT_AUTHORIZED
```

A later P32 authorization must carry this package's immutable commit SHA and the accepted C0 baseline:

```yaml
type: surface_handoff
stage: P32
stage_owner: aegis-implementation
from_surface: CONTROL_REASONING
to_surface: CODE_EXECUTION
preferred_executor: codex
reason: repository_heavy_execution
package_ref: <immutable commit containing this C1 P31 package>
task_anchor:
  revision: 5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539
  relation: ancestor
resume_cursor: null
return_surface: CONTROL_REVIEW
```

This is a handoff template only. P31 does not issue P32 authorization.

# 2. Current Authority Inputs

C1 consumes these as resolved inputs and must not redesign them:

1. `GT-G1-04-C P20 Verification Design Reconciliation v0.1` — page `3cc4c57a-590c-81ae-ab73-d75501c47169` — single Current C Verification Authority.
2. `GT-G1-04-C P23 Authority Supersession + Promotion v0.1 — PASS` — page `3cc4c57a-590c-817b-bcc6-e545279a4722` — promotion establishing P20 as Current Authority.
3. `GT-G1-04-C P30 Implementation Plan v0.1 — READY_FOR_P31` — page `3cc4c57a-590c-81c4-9e7b-d404c3fdba4b` — C0-C8 DAG and C1 purpose/exit authority.
4. `GT-G1-04-C P34 C0 Gate Review v0.1 — PASS_WITH_FINDINGS` — page `3cd4c57a-590c-8114-917e-d40243e668cc` — accepts C0 downstream and records non-blocking traceability finding `P34-C0-F01`.
5. `G1-04-A Semantic Authority Closure Gate v0.1` — page `3c94c57a-590c-8153-955e-dfbb4b6b7da3` — Current GT-G1-04 semantic freeze used by C expected intent.
6. Current A-lane structural semantics — page `3c94c57a-590c-81cf-97eb-f6c89fce563e`.
7. Current Restore authority — page `3ca4c57a-590c-8150-b3bd-cb1d51eb0b83`.
8. Current hierarchy capability/cardinality authority — page `3cb4c57a-590c-815a-b8fa-cd785a837da7`.
9. Current geometry aggregate accounting closure — page `3c94c57a-590c-81e1-aea7-eae7ea8a8c88`.
10. V1 semantic surface / 15-operation release-candidate lock — page `3c54c57a-590c-8136-89c3-f2b7cdb87625`.

P20 is the C verification authority that decides how these semantic authorities may become expected truth. Supporting older verification pages remain supporting inputs only where consistent with P20/P23; they are not parallel Current C Authorities.

## Authority-reference rule for C1

Every authoring record must contain concrete references that a reviewer can resolve without following the P31 package transitively. At minimum:

- every record cites P20 directly for its C verification expectation (for example `C-V02`, `C-V03`, `C-V04`, `C-V05`, `C-V06`, or another applicable stable P20 requirement); and
- every record cites the semantic Current Authority page(s) that determine the operation/state rule represented by the case.

Acceptable examples are concrete strings such as:

```text
notion:3cc4c57a-590c-81ae-ab73-d75501c47169#C-V02
notion:3c94c57a-590c-8153-955e-dfbb4b6b7da3
notion:3ca4c57a-590c-8150-b3bd-cb1d51eb0b83#RST-B04
```

The `notion:` prefix is only a verification traceability convention inside the authoring files; it does not create Product ABI or wire semantics. A full Notion URL containing the same page UUID is also acceptable. A text-only sentence with no concrete page UUID/URL is not acceptable.

# 3. Fresh Repository Reality / Trusted Baseline

P31 fresh-state reconciliation observed:

```text
repository = Mostorm-Labs/axiom
branch = codex/gt-g1-04-operation-apply
accepted_C0_materialized_ref = 5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539
observed_HEAD_before_C1_package = 5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539
classification = EXACT_ACCEPTED_BASELINE
```

No descendant delta exists to reconcile before packaging.

The accepted baseline already contains and C1 must consume, not replace:

```text
verification/schemas/semantic/g1-04-c-case.schema.json
verification/schemas/semantic/g1-04-c-expected.schema.json
verification/schemas/semantic/g1-04-c-observation.schema.json
verification/schemas/semantic/g1-04-c-result.schema.json
verification/schemas/semantic/g1-04-c-gate.schema.json
verification/tools/g1_04_c_contract.mjs
verification/corpus/semantic/v1/g1-04-c/README.md
verification/tests/g1_04_c_schema.test.mjs
verification/tests/g1_04_c_authority_map.test.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
```

C0's case schema requires `AUTHORITY_MANUAL`, non-empty `authorityRuleRefs`, `inputRef`, `expectedRef`, and `blocking`. C0's expected schema requires `AUTHORITY_MANUAL`, non-empty `authorityRuleRefs`, and `mutationExpected=false`, with only `PLAN_READY | ALREADY_APPLIED | REJECTED` dispositions and pre-apply terminal phases.

The accepted C0 README freezes the authoring-root paths and forbids implementation/reference/generated output from writing expected truth.

# 4. Exact Authorized Repository Scope

## 4.1 Create during later authorized P32

```text
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/tests/g1_04_c_authoring_root.test.mjs
```

The test file is a C1 verification-only integrity test. It does not modify C0 schemas/helper contracts or production code.

## 4.2 Modify

None.

In particular, do not modify:

```text
verification/schemas/semantic/g1-04-c-*.schema.json
verification/tools/g1_04_c_contract.mjs
verification/corpus/semantic/v1/g1-04-c/README.md
verification/tests/g1_04_c_schema.test.mjs
verification/tests/g1_04_c_authority_map.test.mjs
verification/tests/g1_04_c_open_reconciliation.test.mjs
verification/package.json
verification/tools/validate_schemas.mjs
verification/corpus/semantic/v1/corpus.json
verification/corpus/semantic/v1/fixture-manifest.json
runtime/semantic/**
.github/workflows/**
```

If correct C1 implementation requires modifying one of those paths, stop and return for scope/Authority review instead of widening the package.

## 4.3 Evidence created only after later P32 source completion

Let `source_ref` be the commit containing exactly the four C1 source/test files above on top of an accepted execution start. Create under:

```text
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/
```

exactly:

```text
C-AUTHORITY-MAP.json
C-CORPUS-MANIFEST.json
C-EXPECTED-PROVENANCE.json
C-OPEN-RECONCILIATION.json
```

These evidence files are not authoring truth and must use evidence/provenance roles appropriate to verification evidence. The evidence commit containing them becomes the later P32 `materialized_ref` after push.

# 5. Explicit Non-Goals

C1 does not create or modify:

```text
verification/corpus/semantic/v1/g1-04-c/generated/**
verification/fixture-author/compile_g1_04_c.py
runtime/semantic/tools/g1_04_c_observer.cpp
runtime/semantic/tools/g1_04_c_projection.hpp
runtime/semantic/tools/g1_04_c_projection.cpp
verification/packages/semantic-conformance-cli/**
.github/workflows/g1-04-c-semantic-conformance.yml
```

C1 also does not:

- compile `inputRef` targets;
- serialize protobuf/runtime operations;
- invoke `OperationEngine::prepare()`;
- read Reference/Indexed ObjectStore output;
- compare expected vs implementation observations;
- prove provider differential;
- prove canonical before/after no-mutation by executing production code;
- generate a final C Gate verdict;
- add production instrumentation;
- create Atomic Apply/post-state evidence;
- start C2-C8 execution;
- claim that the C5 runnable 15-operation core corpus or C6 cross-cutting execution slice is complete merely because human-authored semantic records exist.

Existing B tests may be read later as coverage-location hints only. They may not supply expected values, error categories, plan facts, case IDs, or authority refs.

# 6. C1 Authoring File Contract

C1 uses the accepted C0 record schemas directly. It does **not** create another case/expected schema.

## 6.1 `authoring/cases.json`

File representation is a JSON array of CaseIntent records. Every element validates independently against:

```text
verification/schemas/semantic/g1-04-c-case.schema.json
```

Each record must have:

```text
format              = axiom-g1-04-c-case-v1
formatVersion       = 1
provenance          = AUTHORITY_MANUAL
id                  = stable unique non-empty string
operationFamily     = exact V1 operation-family name
authorityRuleRefs[] = concrete direct Current Authority refs
inputRef            = generated/inputs/<case-id>.json
expectedRef         = authoring/expected.json#<case-id>
blocking            = boolean defined by Current C authority
```

`inputRef` is a logical future C2 target in C1. C1 must not create that file or derive it from production behavior.

The exact V1 operation-family vocabulary for C1 is the accepted 15-family surface:

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

Do not invent aliases such as `CreateObject`, `MoveObject`, or product UI action names.

## 6.2 `authoring/expected.json`

File representation is a JSON array of CExpectedOutcome records. Every element validates independently against:

```text
verification/schemas/semantic/g1-04-c-expected.schema.json
```

Each authored expected record must correspond one-to-one with a CaseIntent `id` and must contain only Current-Authority-supported expected truth:

```text
format              = axiom-g1-04-c-expected-v1
formatVersion       = 1
provenance          = AUTHORITY_MANUAL
caseId              = matching CaseIntent.id
authorityRuleRefs[] = concrete direct Current Authority refs
mutationExpected    = false

disposition?         = PLAN_READY | ALREADY_APPLIED | REJECTED
terminalPhase?       = NORMALIZE | STATELESS_VALIDATE | IDEMPOTENCY | STATEFUL_VALIDATE | PREPARE
semanticErrorCategory? = only when Current Authority freezes it
logicalPlanProjection? = only authority-defined logical facts
openPolicy?             = true only if Current Authority is genuinely OPEN
```

For currently CLOSED Connector-delete and geometry-aggregate policies, `openPolicy:true` is forbidden. Do not use `BLOCKED_OPEN`.

## 6.3 Human-reviewed intent catalog boundary

C1 is the layer that writes **human-reviewed semantic intent and expected truth**. P32 must transcribe the mandatory intent families from P20/P30 into these files from Current Authority, not from code/tests.

This authoring activity does not authorize C5/C6 runtime execution. The distinction is:

```text
C1
= manual semantic intent / expected truth records
= logical future input refs only
= no generated runnable fixture
= no observer/coordinator execution

C5/C6 later
= runnable operation-family/cross-cutting corpus materialization and execution coverage
= generated inputs + observations + comparison evidence
```

At C1 authoring time, include the Current-Authority-required intent families described by P20 Section 7 / P30 C1, including:

- positive `PLAN_READY` intent for each of the 15 operation families;
- operation-specific blocking negative intents required by P20's Mandatory 15-Operation Intent Coverage table;
- Restore current-state eligibility, existing-ID rejection, staged parent/child, staged target+Connector, idempotency-before-existence, Local/Replay/Remote semantic parity, and no-tombstone dependency intents;
- Connector target delete deterministic cascade-closure expected intent, never OPEN;
- geometry aggregate `1,999,999`, `2,000,000`, `2,000,001`, and checked-overflow intents using Current Authority outcomes;
- hierarchy capability/cardinality intents for Root, Group, Sticky and non-parent kinds;
- same OperationId + equivalent payload => `ALREADY_APPLIED`; same OperationId + different payload => rejection before stateful validation.

For every intent family above, derive the expected fields from the cited Current Authority. If the applicable authority does not freeze an error category or logical projection field, omit that field rather than filling it from implementation reality.

If any mandatory intent in P20/P30 cannot be represented without inventing semantic truth, C1 is `BLOCKED_AUTHORITY`; stop and route upstream. Do not weaken the mandatory set to make the package pass.

## 6.4 `suites/core.json`

Create one verification-only membership file:

```json
{
  "format": "axiom-g1-04-c-core-suite-v1",
  "formatVersion": 1,
  "suiteId": "GT-G1-04-C-CORE",
  "caseIds": ["<all C1 blocking CaseIntent ids in deterministic lexical order>"]
}
```

Rules:

- `caseIds` contains every C1 `blocking:true` case exactly once;
- `caseIds` contains no non-existent case;
- order is deterministic lexical order;
- suite membership is not expected truth and must not duplicate expected fields;
- C5/C6 may later add authorized cases through their own reviewed packages; C1 does not pre-authorize future additions.

# 7. C0 Contracts Consumed, Not Rewritten

C1 must use the C0 contracts exactly as materialized at the accepted baseline:

```text
g1-04-c-case.schema.json
  -> manual case role, direct authority-ref field, input/expected references, blocking bit

g1-04-c-expected.schema.json
  -> manual expected role, no mutation, valid pre-apply disposition/phase envelope

g1-04-c-observation.schema.json
  -> remains unused for production observation in C1

g1-04-c-result.schema.json
  -> remains unused for coordinator result in C1

g1-04-c-gate.schema.json
  -> remains unused for Gate result in C1

g1_04_c_contract.mjs
  -> accepted CLOSED-policy guard; may be imported by C1 integrity tests but not modified

g1-04-c/README.md
  -> accepted trust-path and no-bless contract; may not be rewritten in C1
```

C1's integrity test may add stricter **authoring-root cross-record checks** without changing C0 schemas: uniqueness, one-to-one case/expected binding, direct-authority-ref form, suite completeness, and forbidden implementation-derived references.

# 8. TDD / Failing-First Sequence for Later P32

## Task 0 — Execution preflight, no edits

**Files:** none.

- [ ] Resolve branch HEAD and record it as `execution_start_ref`.
- [ ] Verify `5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539` is an ancestor of `execution_start_ref`.
- [ ] Resolve this package commit and verify it equals the P32 `package_ref`.
- [ ] If the package commit itself is branch HEAD, this is a legal descendant of the C1 task anchor; do not reject it because it differs from the historical C0 HEAD.
- [ ] Inspect only descendant changes after the anchor/package baseline. If unrelated semantic changes invalidate C1 assumptions, return `BLOCKED_EXECUTION_DIVERGENCE`.
- [ ] Confirm the two authoring files and C1 suite/test files do not already exist from an unauthorized prior attempt. If they do, reconcile under P33 rules rather than overwriting them blindly.

## Task 1 — RED: create authoring-root integrity test before authoring files

**Files:**
- Create: `verification/tests/g1_04_c_authoring_root.test.mjs`

**Consumes:**
- `verification/tools/validate_schemas.mjs::validateValue`
- accepted C0 case/expected schemas
- accepted `verification/tools/g1_04_c_contract.mjs` for CLOSED-policy checks where applicable

**Produces:** a C1-only test that validates the real authoring root, not synthetic in-test samples.

The test must use this structure:

```js
import assert from "node:assert/strict";
import { readFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import { validateValue } from "../tools/validate_schemas.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..");

async function readJson(relativePath) {
  return JSON.parse(await readFile(resolve(root, relativePath), "utf8"));
}

async function schema(name) {
  return readJson(`schemas/semantic/g1-04-c-${name}.schema.json`);
}

const CURRENT_AUTHORITY_PAGE_IDS = new Set([
  "3cc4c57a-590c-81ae-ab73-d75501c47169",
  "3c94c57a-590c-8153-955e-dfbb4b6b7da3",
  "3c94c57a-590c-81cf-97eb-f6c89fce563e",
  "3ca4c57a-590c-8150-b3bd-cb1d51eb0b83",
  "3cb4c57a-590c-815a-b8fa-cd785a837da7",
  "3c94c57a-590c-81e1-aea7-eae7ea8a8c88",
  "3c54c57a-590c-8136-89c3-f2b7cdb87625",
]);

function isDirectCurrentAuthorityRef(ref) {
  return [...CURRENT_AUTHORITY_PAGE_IDS].some((id) =>
    ref.includes(id) || ref.includes(id.replaceAll("-", ""))
  );
}
```

Required test assertions:

1. `cases.json` exists, parses as a non-empty array, and every item validates against the accepted C0 case schema.
2. `expected.json` exists, parses as a non-empty array, and every item validates against the accepted C0 expected schema.
3. Case IDs are unique; expected `caseId`s are unique; the ID sets are exactly equal.
4. `expectedRef` is exactly `authoring/expected.json#<case-id>` for each case.
5. `inputRef` is exactly `generated/inputs/<case-id>.json`; the test does **not** require the generated file to exist in C1.
6. Every `authorityRuleRefs` item is a direct Current Authority ref by concrete page UUID/URL; no record relies only on free-text authority basis.
7. No authority ref or authoring field points at `runtime/semantic/`, B test output, `IMPLEMENTATION_OBSERVATION`, generated fixture output, or an evidence file as expected truth.
8. Every expected record has `mutationExpected === false` and `provenance === "AUTHORITY_MANUAL"` (also enforced by schema).
9. The 15 exact operation-family names listed in Section 6.1 each have at least one positive blocking `PLAN_READY` / `PREPARE` authoring record.
10. Mandatory closed Connector-delete / geometry intents are not authored OPEN; use the accepted C0 CLOSED-policy helper or an equivalent test call without modifying that helper.
11. `suites/core.json` exists; its `caseIds` are lexically sorted, unique, reference existing cases only, and equal exactly to the IDs of all `blocking:true` cases.
12. No authoring file contains an implementation-observation provenance constant, provider field, runtime result, PASS/FAIL verdict, or `source_ref`/`materialized_ref` as semantic truth.

Before creating any authoring/suite file, run:

```bash
cd verification
node --test tests/g1_04_c_authoring_root.test.mjs
```

Required RED signal:

```text
FAIL because verification/corpus/semantic/v1/g1-04-c/authoring/cases.json (or expected.json) does not exist
```

A failure caused by an unrelated pre-existing verification infrastructure problem is not an acceptable RED; stop and classify it.

Commit the RED test alone after confirming the intended failure.

## Task 2 — GREEN: hand-author the C1 authority-manual root

**Files:**
- Create: `verification/corpus/semantic/v1/g1-04-c/authoring/cases.json`
- Create: `verification/corpus/semantic/v1/g1-04-c/authoring/expected.json`
- Create: `verification/corpus/semantic/v1/g1-04-c/suites/core.json`

**Interfaces:**
- Consumes: P20/P30 mandatory intent definitions and the semantic Current Authority pages in Section 2.
- Produces: stable `AUTHORITY_MANUAL` CaseIntent/Expected records and deterministic blocking suite membership for C2+ to consume.

- [ ] Read P20 Section 7 and the referenced semantic Current Authority pages before authoring each intent family. Do not consult current B/runtime result values to choose expected truth.
- [ ] Hand-author `cases.json` as a JSON array. Use the exact operation-family vocabulary in Section 6.1.
- [ ] Hand-author `expected.json` as a JSON array, one record per case.
- [ ] Give every case/expected record concrete direct authority refs as specified in Section 2.
- [ ] For positive operation-family cases, author `disposition:"PLAN_READY"` and `terminalPhase:"PREPARE"` only when supported by P20/semantic authority.
- [ ] For idempotent-equivalent cases, author `ALREADY_APPLIED` at `IDEMPOTENCY`; for ID collision, author rejection before stateful checks, using only authority-supported category detail.
- [ ] For Restore, Connector cascade, geometry boundaries, hierarchy/cardinality and operation-specific negative cases, copy the semantic outcome from Current Authority; omit any unsupported `semanticErrorCategory` or `logicalPlanProjection` field.
- [ ] Do not create the files referenced by `inputRef`; C2 owns generation.
- [ ] Create `suites/core.json` containing exactly all C1 blocking case IDs in lexical order.
- [ ] Run:

```bash
cd verification
node --test tests/g1_04_c_authoring_root.test.mjs
```

Expected: **PASS**.

If this cannot pass without changing C0 schemas/helper or inventing semantic truth, stop. Do not weaken the test.

## Task 3 — Focused C1 verification + inherited regression

Run exactly:

```bash
cd verification
node --test \
  tests/g1_04_c_schema.test.mjs \
  tests/g1_04_c_authority_map.test.mjs \
  tests/g1_04_c_open_reconciliation.test.mjs \
  tests/g1_04_c_authoring_root.test.mjs
```

Expected: all C0 + C1 focused tests PASS.

Then run the complete existing verification workspace regression without changing its scripts:

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

No C++ build, semantic CTest, observer execution, provider run, or hosted C workflow is required for C1 because no C++/production/CI file is authorized to change. If C1 touches production C++ such that such a build becomes necessary, that is a scope violation, not a reason to widen the package.

## Task 4 — Source ref, scope proof, and durable evidence materialization

- [ ] Commit exactly the C1 integrity test plus the three authoring/suite files. Record the resulting commit as `source_ref`.
- [ ] Verify the exact changed-file set from the accepted P32 execution start to `source_ref` is limited to the four authorized C1 create paths in Section 4.1.
- [ ] Create `C-AUTHORITY-MAP.json` under `verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/` with:
  - `source_ref` and C1 `package_ref`;
  - C1 task anchor revision/relation;
  - P20/P23/P30/C0-P34 page IDs;
  - every distinct concrete semantic Current Authority page ref used by authoring records;
  - case count / expected count / blocking count;
  - per-case direct `authorityRuleRefs` mapping;
  - explicit `production_semantic_files_changed = false`;
  - focused C0+C1 test result and workspace regression result.
- [ ] Create `C-CORPUS-MANIFEST.json` with deterministic case IDs, operation families, blocking flags, `inputRef`, `expectedRef`, and suite membership. This manifest is evidence only; it must not supply expected values.
- [ ] Create `C-EXPECTED-PROVENANCE.json` proving every expected record has `AUTHORITY_MANUAL`, concrete authority refs, and `mutationExpected=false`; record `implementation_derived_expected_count = 0`, `generated_expected_count = 0`, and `runtime_capture_used = false`.
- [ ] Create `C-OPEN-RECONCILIATION.json` for any authored Connector-delete / geometry closed-policy records. Unlike the C0 finding, include the exact concrete Notion page refs directly in the evidence record, not text-only `authority_basis`.
- [ ] Parse/validate the evidence JSON and corroborate counts against the actual authoring files.
- [ ] Commit only these evidence files and push the branch.
- [ ] Return the reviewer-resolvable remote evidence commit as `materialized_ref`.

If the exact result cannot be materialized remotely, return `BLOCKED_EVIDENCE`; local commits/test transcripts are insufficient.

# 9. Focused Oracle for C1

C1's correctness oracle is **human-reviewed Current Authority plus C0 trust contracts**, not production behavior.

Primary oracle chain:

```text
Current semantic authority
+ P20 Current C Verification Authority
+ P30 C1 slice
+ accepted C0 schemas/trust helper
        ↓
AUTHORITY_MANUAL cases/expected
        ↓
C1 authoring-root integrity checks
```

C1 PASS may prove:

- the single authoring root exists at the frozen paths;
- every record is `AUTHORITY_MANUAL` and schema-valid;
- every blocking case/expected has direct concrete Current Authority refs;
- case/expected/suite identity is internally complete and deterministic;
- expected truth is not generated or captured from production/reference/indexed output;
- closed Connector/geometry policies are not stale OPEN;
- no C0 trust contract or production semantic file was rewritten.

C1 PASS does **not** prove:

- generated fixtures are reproducible (C2);
- production `prepare()` observations match expected (C3/C4+);
- the runnable 15-operation core corpus passes (C5);
- cross-cutting execution/no-mutation cases pass production observation (C6);
- Reference/Indexed parity or Gate aggregation passes (C7);
- exact-source C CI is green (C8);
- Atomic Apply or final semantic state is correct (GT-G1-05+).

# 10. Required Regression / Performance Constraints

C1 has no runtime performance target because it changes no runtime code. Its engineering constraints are:

```text
production semantic files changed = 0
C0 contract files modified = 0
generated authoring truth = 0
implementation-derived expected truth = 0
blocking cases without direct authority refs = 0
case/expected identity mismatch = 0
suite dangling/duplicate membership = 0
stale OPEN on closed Connector/geometry policies = 0
```

Existing verification workspace behavior must remain green.

# 11. `source_ref` / `materialized_ref` Model for Later P32

The later P32 return uses two immutable refs:

```text
source_ref
= Git commit containing only C1 source/test changes:
  authoring/cases.json
  authoring/expected.json
  suites/core.json
  g1_04_c_authoring_root.test.mjs

materialized_ref
= later reviewer-resolvable remote Git commit containing evidence for exactly source_ref
```

Rules:

- `source_ref` must be a descendant of the accepted P32 execution start, which itself must satisfy the C1 task-anchor ancestry contract.
- `materialized_ref` must be a descendant of `source_ref` and may add only the package-defined evidence files.
- evidence files must identify `source_ref` exactly; evidence for another source commit is invalid.
- local-only SHA/worktree state is insufficient for review.
- the P32 result must return both refs explicitly.

# 12. Fail-Closed Triggers

Stop C1 and return upstream rather than improvising if any of the following occurs:

- C1 requires inventing semantic truth not present in Current Authority;
- Current Authority documents materially contradict each other for a mandatory authoring intent;
- a required expected value can be obtained only by running production/reference/indexed implementation;
- C1 would need C2 fixture generation to decide whether its own expected outcome is correct;
- C1 would require changing production semantic/ObjectStore code;
- C1 would require Atomic Apply/post-state semantics;
- C1 would require weakening or rewriting an accepted C0 schema/helper/trust rule;
- C1 would require creating a second authoring root or competing schema;
- C1 would require generated/runtime/implementation output to write `cases.json` or `expected.json`;
- C1 packaging/execution attempts to claim C5/C6 runnable coverage complete without their separately authorized slices;
- repository ancestry from `5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539` cannot be established;
- unrelated descendant changes invalidate the trusted baseline;
- exact evidence cannot be durably materialized for independent review.

Authority conflict/missing truth => route to the earliest untrusted layer through Aegis. Do not repair Authority inside P31/P32.

# 13. Exit Criteria

A later C1 P32 result may be returned for P34 review only when all of these are true:

```text
single authoring root exists at the two frozen paths
AND cases.json is a non-empty array of C0-valid AUTHORITY_MANUAL CaseIntent records
AND expected.json is a non-empty array of C0-valid AUTHORITY_MANUAL Expected records
AND every case has exactly one matching expected record
AND all 15 V1 operation families have Current-Authority-required positive authoring intent
AND all mandatory P20/P30 C1 intent families are represented in manual authoring truth
AND every blocking record has direct concrete Current Authority refs
AND mutationExpected=false for every expected record
AND no expected truth came from implementation/generated/runtime output
AND core.json deterministically contains exactly all blocking case IDs
AND closed Connector/geometry policies are not OPEN
AND C0 + C1 focused tests pass
AND verification npm run validate passes
AND git diff --check passes
AND C0 contract files are unchanged
AND production semantic files are unchanged
AND C2-C8 / GT-G1-05 implementation is absent
AND exact C1 source_ref is durably evidenced at materialized_ref
```

# 14. Expected Control Return After This P31 Packaging Occurrence

Before any P32 execution, the correct control state is:

```text
GT-G1-04-C C0
= PASS_WITH_FINDINGS
= CLOSED
= ACCEPTED_FOR_DOWNSTREAM

GT-G1-04-C C1 / P31
= PACKAGE MATERIALIZED
= CORROBORATED / REVIEWABLE
= READY_FOR_P32_AUTHORIZATION
  only after package-ref ancestry/scope corroboration passes

C1 / P32
= NOT_STARTED

C2-C8
= NOT_AUTHORIZED

GT-G1-05
= NOT_AUTHORIZED
```

The new C1 `package_ref` is the immutable Git commit containing this file. It must not reuse `cefa97ff175de2dc7082039b653ea54300d456c9` or any C0 source/materialized ref.

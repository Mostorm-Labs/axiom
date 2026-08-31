# GT-G1-04-C C1 P31 Corroboration Addendum

**Stage:** `GT-G1-04-C / C1 Human-Reviewed Authoring Root / P31 Task Packaging`.

**Base package:** `docs/superpowers/plans/2026-08-31-gt-g1-04-c1-human-reviewed-authoring-root-p31.md@54d35aa3ad5120a2b95cfa54f9d141447d124dd9`.

**Purpose:** Corroborate repository ancestry/scope and repair execution-readiness defects in the base C1 P31 package without changing Current semantic Authority, C1 semantic scope, C0 accepted contracts, C2-C8 authorization, or GT-G1-05 status.

This addendum is authoritative over conflicting or weaker instructions in the base package. All base-package constraints not explicitly refined here remain unchanged.

## 1. Repository / ancestry corroboration

Fresh corroboration establishes:

```text
repository = Mostorm-Labs/axiom
branch = codex/gt-g1-04-operation-apply
C1 task_anchor = 5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539
base_package_ref = 54d35aa3ad5120a2b95cfa54f9d141447d124dd9
relation = ancestor
base_package ahead_by anchor = 1
base_package behind_by anchor = 0
merge_base = task_anchor
base_package delta = exactly one added P31 plan file
unrelated implementation changes = 0
```

At corroboration time, remote branch HEAD equals the base package ref exactly.

Therefore the package history is a legal fast-forward from the accepted C0 materialized baseline. C0 is not reopened.

## 2. Corroboration findings

### P31-C1-F01 — mandatory C1 intent coverage was delegated back to P20 at execution time

The base package correctly states that C1 must author the Current-Authority-required positive/negative intent families, but Task 2 tells the executor to read P20 Section 7 and reconstruct the per-operation mandatory set while coding.

That is weaker than the P31 control objective: resolved decisions should be compressed into the approved package so the code surface does not rediscover lifecycle scope or case obligations. It also creates a risk that C1 silently authors only the 15 positive cases while operation-specific negatives drift into C5/C6.

**Classification:** P31 packaging completeness defect. No Authority ambiguity exists; P20 already freezes the mandatory table.

**Repair:** Section 4 below embeds the exact P20 mandatory 15-operation intent table and makes it the C1 execution checklist.

### P31-C1-F02 — authority-ref integrity test was weaker than the package's own authority-reference contract

The base package says every C1 authoring record must cite:

1. P20 directly for the C verification requirement; and
2. at least one concrete semantic Current Authority source that determines the semantic rule.

But the draft test only checked that every ref matched any ID in one fixed set. A record containing only P20, or only one high-level semantic page, could pass that test despite violating the stronger contract. The fixed set would also risk rejecting a more-specific Current semantic leaf page that is preferable for direct traceability.

**Classification:** mechanical traceability test defect only.

**Repair:** Section 5 requires a two-part check: P20 is mandatory on every case/expected record, and at least one additional concrete semantic authority UUID/URL is mandatory. Specific Current leaf Authority refs are allowed and preferred; the later evidence map must resolve every distinct semantic ref to the Current G1-04 authority closure.

### P31-C1-F03 — `expectedRef` fragment semantics were implicit

The base package specifies:

```text
authoring/expected.json#<case-id>
```

while `expected.json` is a JSON array. Without an explicit resolver contract, a future tool could mistakenly treat the fragment as RFC 6901 JSON Pointer or an array index.

**Classification:** verification-only reference-contract ambiguity; not semantic Authority.

**Repair:** Section 6 freezes this as an opaque C verification logical fragment resolved by matching `CExpectedOutcome.caseId`, not by JSON Pointer/array position.

### P31-C1-F04 — coverage evidence needed an explicit mandatory-requirement matrix

The base package requires `C-AUTHORITY-MAP.json`, but did not make the 15-operation mandatory-family checklist a required evidence projection. A reviewer could therefore see valid records and counts without a direct proof that every P20 mandatory family was covered.

**Classification:** P31 evidence-completeness defect.

**Repair:** Section 7 requires `C-AUTHORITY-MAP.json` to contain a requirement-coverage matrix grounded in Section 4.

## 3. Authority status after corroboration

No earlier untrusted layer was found.

```text
P20 Current C Verification Authority
= unchanged

P23 promotion
= unchanged

P30 C0-C8 DAG
= unchanged

C0 P34 PASS_WITH_FINDINGS / ACCEPTED_FOR_DOWNSTREAM
= unchanged

C1 semantic scope
= unchanged

C2-C8
= NOT_AUTHORIZED

GT-G1-05
= NOT_AUTHORIZED
```

The following C0 contracts remain consumed exactly and are not modified by this repair:

```text
verification/schemas/semantic/g1-04-c-case.schema.json
verification/schemas/semantic/g1-04-c-expected.schema.json
verification/schemas/semantic/g1-04-c-observation.schema.json
verification/schemas/semantic/g1-04-c-result.schema.json
verification/schemas/semantic/g1-04-c-gate.schema.json
verification/tools/g1_04_c_contract.mjs
verification/corpus/semantic/v1/g1-04-c/README.md
```

## 4. Superseding exact C1 mandatory intent checklist

This section supersedes any instruction that asks the P32 executor to rediscover the C1 coverage set from P20. P20 remains Authority for the expected truth; this table is the P31 execution projection of its already-frozen Section 7 mandatory coverage.

For every row below, C1 must author the listed human-reviewed CaseIntent/CExpectedOutcome families. These are **manual semantic intent/expected records only** in C1. Their runnable generated inputs and production execution remain later slices.

| Operation family | Mandatory C1 intent families |
| --- | --- |
| `InsertObjects` | valid staged insert; staged parent; staged Connector target; duplicate/existing ID; invalid staged hierarchy/cycle; parent-child capability / Sticky cardinality |
| `DeleteObjects` | valid delete; subtree closure; deterministic Connector cascade fixed point; missing/invalid target; duplicate target structural rejection where applicable |
| `RestoreObjects` | current-state eligibility; existing-ID rejection; staged parent+child; staged target+Connector; absent required ref; OperationId-before-existence ordering; Local/Replay/Remote semantic parity; no tombstone dependency; authority-defined `RST-B01..RST-B12` intents are semantic inputs, never B-result oracles |
| `SetPlacements` | valid move; full resulting-cycle rejection; invalid parent; Group/Sticky/other parent capability; Sticky second-RichText cardinality; OrderKey validity |
| `SetTransforms` | valid finite canonical transform; `-0` normalization; NaN/Inf rejection |
| `PatchProperties` | valid Set/Clear; FieldId existence; value branch/type; kind applicability; presence/default semantics; duplicate `(ObjectId, FieldId)` rejection |
| `SetObjectSize` | Shape/Image/Sticky valid size; wrong kind; non-finite/non-positive size; applicable hard limits |
| `SetVectorPathGeometry` | valid replacement; wrong kind; structural invariant; exact aggregate geometry boundary at N-1/N/N+1; checked overflow |
| `SetImageContent` | valid Image content; wrong kind; ResourceId/content presence; intrinsic/sourceRect/contentMode/local-size whole-content validity; runtime resource availability is not a semantic correctness oracle/precondition |
| `AddStroke` | valid Vector/Dab stroke; wrong kind/content; invalid StrokeRecord; new-ID/existing-ID behavior |
| `SplitStrokes` | valid source→replacement plan; source missing; replacement structural invalidity; replacement ID collision; whole split plan atomic closure at the pre-apply planning level |
| `AddEraseMasks` | valid object-local mask; mask ID uniqueness; geometry validity; target capability; duplicate/existing mask rejection |
| `RemoveEraseMasks` | valid removal; object/mask missing; duplicate mask IDs; whole-operation rejection |
| `EditRichText` | valid ordered steps; stable refs/positions; UTF-8/scalar/style invariants; one invalid step causes whole-delta rejection |
| `SetConnectorContent` | valid Free/Attached endpoints; target existence and V1 Connectable capability; anchor presence/applicability; routing validity; invalid one-end causes whole-content rejection |

In addition to the operation rows, C1 must author the P20 cross-cutting manual intent families already named by the base package:

```text
same OperationId + equivalent payload
  -> ALREADY_APPLIED at IDEMPOTENCY

same OperationId + different payload
  -> REJECTED before STATEFUL_VALIDATE

Connector target delete
  -> CLOSED deterministic cascade behavior; never OPEN

geometry point-like aggregate
  1,999,999 -> PASS/PLAN_READY when all other semantics are valid
  2,000,000 -> PASS/PLAN_READY when all other semantics are valid
  2,000,001 -> GEOMETRY_LIMIT_EXCEEDED where Current Authority freezes that category
  checked arithmetic overflow -> INTEGER_OVERFLOW where Current Authority freezes that category

hierarchy
  Root -> any V1
  Group -> any V1
  Sticky -> RichText only
  Sticky direct RichText child cardinality = 0..1
  other kinds cannot be non-root parents
  empty Sticky is legal

all C expected records
  mutationExpected = false
```

Rules for interpreting this checklist:

- One case may satisfy more than one mandatory family only when the cited Current Authority makes that overlap explicit and the case remains reviewable; do not collapse distinct semantic boundaries merely to reduce case count.
- Coverage completeness is obligation-based, not count-based.
- Do not invent `semanticErrorCategory` just to make a row more specific. Omit unsupported detail.
- Do not infer `logicalPlanProjection` from current implementation output. Author only authority-defined logical facts.
- C1 authoring of this checklist does **not** claim C5/C6 runnable coverage complete.

## 5. Superseding authority-reference integrity contract

Every `CaseIntent.authorityRuleRefs` and every matching `CExpectedOutcome.authorityRuleRefs` must satisfy all of the following:

1. contains a direct concrete P20 reference containing page UUID `3cc4c57a-590c-81ae-ab73-d75501c47169` (or the dashless equivalent / full Notion URL);
2. contains at least one additional direct concrete semantic Current Authority reference, distinct from P20;
3. every ref is a concrete Notion UUID/URL-style reference, optionally suffixed with a stable rule/section ID;
4. no ref points to production code, B test output, generated fixtures, implementation observations, result evidence, or a textual `authority_basis` substitute;
5. where a more-specific Current semantic leaf page exists, it is allowed and preferred over citing only a high-level closure page;
6. later `C-AUTHORITY-MAP.json` must list every distinct semantic page ref and record how it resolves into the Current G1-04 semantic authority closure.

The C1 integrity test must mechanically require the two-part structure. Conceptually:

```js
const P20_PAGE_ID = "3cc4c57a-590c-81ae-ab73-d75501c47169";
const P20_PAGE_ID_COMPACT = P20_PAGE_ID.replaceAll("-", "");

function containsPage(ref, pageId) {
  return ref.includes(pageId) || ref.includes(pageId.replaceAll("-", ""));
}

function hasConcreteNotionUuid(ref) {
  return /[0-9a-f]{8}-?[0-9a-f]{4}-?[0-9a-f]{4}-?[0-9a-f]{4}-?[0-9a-f]{12}/i.test(ref);
}

function assertAuthorityBinding(record) {
  assert(record.authorityRuleRefs.some((ref) =>
    ref.includes(P20_PAGE_ID) || ref.includes(P20_PAGE_ID_COMPACT)
  ));

  const semanticRefs = record.authorityRuleRefs.filter((ref) =>
    hasConcreteNotionUuid(ref) && !containsPage(ref, P20_PAGE_ID)
  );
  assert(semanticRefs.length >= 1);
}
```

The test must not use a closed whitelist that rejects more-specific Current semantic leaf pages. Current-status corroboration of the concrete semantic refs is an evidence/review responsibility recorded in `C-AUTHORITY-MAP.json`.

## 6. Superseding `expectedRef` resolution contract

C1 keeps the base-package representation:

```text
expectedRef = authoring/expected.json#<case-id>
```

but freezes its meaning as follows:

- it is a verification-only opaque logical fragment identifier;
- the fragment after `#` is exactly the CaseIntent `id` / CExpectedOutcome `caseId`;
- consumers resolve it by loading `authoring/expected.json` and selecting the **unique** record whose `caseId` equals the fragment;
- it is **not** RFC 6901 JSON Pointer;
- it is **not** an array index;
- reordering the expected array must not change reference meaning;
- zero matches or multiple matches is an integrity failure.

The C1 integrity test must assert exact equality:

```js
caseRecord.expectedRef === `authoring/expected.json#${caseRecord.id}`
```

and separately assert unique one-to-one `id` / `caseId` binding.

Future C2/C4 tooling may implement this resolver but may not reinterpret the fragment without a separately reviewed contract change.

## 7. Superseding coverage evidence contract

`C-AUTHORITY-MAP.json` is required to contain a reviewer-checkable `requirement_coverage` projection in addition to the fields already required by the base package.

For every operation row in Section 4 and every cross-cutting family, record:

```json
{
  "requirement": "P20:C1:<stable-human-readable-key>",
  "operation_family": "InsertObjects",
  "intent_family": "staged-parent",
  "case_ids": ["<one-or-more-authoring-case-ids>"],
  "authority_rule_refs": ["<concrete-P20-ref>", "<concrete-semantic-ref>"],
  "blocking": true
}
```

The literal `P20:C1:*` values are evidence labels, not Product semantic IDs and not new semantic Authority.

Evidence rules:

- every Section 4 mandatory family has at least one mapped case;
- every mapped case exists in `cases.json`;
- every mapped case has a matching expected record;
- every mapped record carries the listed direct authority refs;
- no requirement is marked covered merely because a similarly named B test exists;
- no expected value is copied from implementation output;
- reviewer can trace each coverage row directly to P20 plus semantic Current Authority.

`C-CORPUS-MANIFEST.json` remains identity/membership evidence and must not duplicate expected semantic truth.

## 8. Refined TDD / execution sequence

The base package Task 0 remains unchanged.

### Task 1 — RED authoring-root integrity test

Create only:

```text
verification/tests/g1_04_c_authoring_root.test.mjs
```

Before authoring files exist, require the intended RED caused by missing C1 authoring root.

In addition to the base-package assertions, the RED/GREEN test must enforce:

- every case/expected record contains P20 plus at least one additional concrete semantic authority ref as Section 5 defines;
- 15 positive blocking `PLAN_READY/PREPARE` operation families exist;
- exact one-to-one case/expected binding and opaque `expectedRef` semantics in Section 6;
- core suite identity/deterministic membership;
- closed Connector/geometry policy status using the accepted C0 helper;
- no implementation/generated/evidence provenance is allowed to become authoring truth.

The test does not need to encode every semantic negative expected value; semantic coverage completeness is proved by human-authored records plus the required Section 7 evidence matrix and later P34 review.

### Task 2 — GREEN authoring

Do **not** rediscover the operation-family checklist. Author from Section 4 of this addendum, consulting the cited Current Authority only to resolve exact expected fields and the most direct semantic page/rule references.

If Section 4 and Current Authority conflict, Current Authority wins and execution stops with `BLOCKED_AUTHORITY` rather than silently editing the package or choosing a behavior.

Create only the three authoring/suite files already authorized by the base package.

### Task 3 — verification

Unchanged from base package:

```bash
cd verification
node --test \
  tests/g1_04_c_schema.test.mjs \
  tests/g1_04_c_authority_map.test.mjs \
  tests/g1_04_c_open_reconciliation.test.mjs \
  tests/g1_04_c_authoring_root.test.mjs

npm run validate

cd ..
git diff --check
```

### Task 4 — source/evidence materialization

Unchanged except that `C-AUTHORITY-MAP.json` must satisfy Section 7 and every distinct semantic authority reference used by the authoring root must be explicitly resolved/corroborated as Current Authority input.

## 9. Exact authorized source scope after repair

No source-scope widening occurred. Later P32 may still create exactly:

```text
verification/corpus/semantic/v1/g1-04-c/authoring/cases.json
verification/corpus/semantic/v1/g1-04-c/authoring/expected.json
verification/corpus/semantic/v1/g1-04-c/suites/core.json
verification/tests/g1_04_c_authoring_root.test.mjs
```

No existing source file is authorized to change.

Evidence scope also remains exactly the base package's four C1 evidence files:

```text
C-AUTHORITY-MAP.json
C-CORPUS-MANIFEST.json
C-EXPECTED-PROVENANCE.json
C-OPEN-RECONCILIATION.json
```

No production semantic, ObjectStore, C0 contract, package script, C2 compiler, C3 observer, C4 coordinator, C5/C6 runnable implementation, C7 gate, C8 CI, or GT-G1-05 file is authorized.

## 10. P31 corroboration exit criteria

This repaired package is P31-reviewable only if the addendum commit itself is a legal descendant of the accepted C0 anchor and the combined P31 delta remains documentation-only.

Required control result:

```text
C1 task_anchor
= 5f0f061ec7db1d0490941cf3f4e8dbd9eb26b539
  relation = ancestor

base package
= 54d35aa3ad5120a2b95cfa54f9d141447d124dd9

final C1 package_ref
= immutable Git commit containing this corroboration addendum

combined task_anchor -> package_ref delta
= P31 documentation only

C1 P31
= CORROBORATED / REVIEWABLE
= READY_FOR_P32_AUTHORIZATION

C1 P32
= NOT_STARTED

C2-C8
= NOT_AUTHORIZED

GT-G1-05
= NOT_AUTHORIZED
```

This addendum does not itself authorize CODE_EXECUTION. A separate explicit P32 surface handoff must carry the final repaired `package_ref` and the unchanged task anchor.

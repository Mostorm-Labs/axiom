# GT-G1-04-B Stateful Validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Every package is RED → GREEN → evidence-gated and ends at the `PreparedApplyPlan` boundary.

**Goal:** Add the stateful, read-only validation and deterministic `PreparedApplyPlan` layer for all fifteen V1 Operations, including OperationId idempotency, resulting-state checks, restore eligibility, deletion closure, and Reference/Indexed differential evidence.

**Architecture:** B consumes a normalized, structurally valid typed `Operation` and a read-only state view. A shared planning context provides ObjectStore lookup, applied-operation lookup, and an in-memory staged overlay; focused validators resolve operation-specific state rules into one immutable `PreparedApplyPlan`. The plan contains no mutation callback, generation, ChangeSet, commit stamp, history write, or publication hook, so GT-G1-05 remains the only commit owner.

**Tech Stack:** C++20, existing `runtime/semantic` static library, GoogleTest/CTest, existing `ReferenceObjectStore` as the independent state oracle, `IndexedObjectStore` as the production-oriented implementation, YAML registries under `schema/axiom/v1`, and source-bound JSON evidence under `verification/evidence/gates/G1/<commit>/GT-G1-04-B/`.

**Spec:** `docs/notion/authority/04-semantic-schema/02-operation-model/operation-payload-validation-v0.1.md`, `docs/notion/authority/04-semantic-schema/02-operation-model/operation-structural-semantics-v1-closure-v0.1.md`, `docs/notion/authority/04-semantic-schema/02-operation-model/restoreobjects-identity-state-tombstone-eligibility-v1-authority-closure-v0.1.md`, `docs/notion/authority/04-semantic-schema/01-object-schema/connector-anchor-contract-v1-release-v0.1.md`, `docs/notion/authority/04-semantic-schema/01-object-schema/objectkind-version-registry-v1-release-v0.1.md`, `docs/notion/authority/04-semantic-schema/01-object-schema/field-registry-v1-release-v0.1.md`, `docs/notion/authority/04-semantic-schema/00-overview/g1-04-a-semantic-authority-closure-gate-v0.1.md`, `docs/notion/authority/07-runtime-data-flow/07-03-operation-semantic-document-v0.1.md`, `docs/notion/authority/07-runtime-data-flow/07-08-open-restore-catchup-v0.1.md`, `docs/notion/authority/07-runtime-data-flow/07-09-special-flows-v0.1.md`, `docs/notion/authority/07-runtime-data-flow/07-11-generation-changeset-apply-batch-v0.1.md`, and the current G1 package at `docs/superpowers/plans/2026-08-26-g1-semantic-kernel-codex-package.md`.

## Global Constraints

- B starts only from a normalized, structurally valid typed `Operation`; it does not repeat A0–A3 wire, envelope, normalization, collection-order, leaf, or hard-limit semantics.
- The authoritative order is Decode/typed view → Normalize → Envelope → Payload → OperationId Idempotency Gate → Reference/Kind/Invariant Validation → Prepare ApplyPlan → Atomic Apply.
- Every operation is whole-operation atomic: a failed target or reference produces no observable canonical mutation.
- B may read `ObjectStore` and build an in-memory staged view; it may not call `internal::ObjectStoreMutator` or any canonical write seam.
- B ends at `PreparedApplyPlan`; Atomic ObjectStore mutation, SemanticGeneration, ChangeSet, CanonicalCommitStamp, History, DataBridge, Outbox, local echo, and publication are GT-G1-05 or later.
- `ReferenceObjectStore` is the correctness-first oracle. `IndexedObjectStore` must produce the same B decision and plan projection without requiring a full ObjectId scan for indexed lookup paths.
- OperationId idempotency is checked before Restore identity/state validation and before all other stateful validation. Equal canonical payloads return `AlreadyApplied`; a different canonical payload for the same ID is a collision/protocol-corruption result.
- Restore eligibility is current canonical ObjectStore state plus complete staged resulting-state validation. No tombstone ledger, deletion epoch, deletion generation, history proof, sync metadata, server cursor, or revision is introduced.
- Snapshot bootstrap remains a Loading-only privileged path; `RestoreObjects` remains a normal OperationEngine path.
- ApplySource (`LocalUndo`, `Replay`, `Remote`, or local) does not alter Restore semantic validation for the same base state and canonical Operation.
- ObjectKind/version, Field Registry applicability, connector connectability, StablePort actual-target applicability, parent/reference existence, and resulting-state rules must be read from released authority. If a required rule is not published, stop with `MISSING_CONTRACT` rather than inventing one.
- B4 StablePort authority is READY: StablePort identity is `(target ObjectKindId, target kindVersion, portId)`; Shape v1, Image v1, and Sticky v1 are connectable; fixed V1 ports are `1=TOP`, `2=RIGHT`, `3=BOTTOM`, and `4=LEFT`; `0` and `5..u32::MAX` are invalid. Every attached StablePort is revalidated against the actual staged target kind/version, with no fallback to AutoPerimeter.
- No B task creates a final independent C golden authority, blesses expected outcomes from production output, or starts GT-G1-05.
- Existing untracked `pocs/shared_engine/reports/poc01/android-visual-smoke/` is preserved, not staged, modified, or removed.

## Repository Reality and Planned File Map

Existing A/GT-G1-03 files consumed by B:

- `runtime/semantic/include/canvas/semantic/operation.hpp`
- `runtime/semantic/include/canvas/semantic/operation_payload.hpp`
- `runtime/semantic/include/canvas/semantic/operation_id.hpp`
- `runtime/semantic/include/canvas/semantic/object_record.hpp`
- `runtime/semantic/include/canvas/semantic/object_content.hpp`
- `runtime/semantic/include/canvas/semantic/erase_mask.hpp`
- `runtime/semantic/include/canvas/semantic/property_value.hpp`
- `runtime/semantic/include/canvas/semantic/validator.hpp`
- `runtime/semantic/include/canvas/semantic/normalizer.hpp`
- `runtime/semantic/include/canvas/semantic/object_store.hpp`
- `runtime/semantic/include/canvas/semantic/reference_object_store.hpp`
- `runtime/semantic/include/canvas/semantic/indexed_object_store.hpp`
- `runtime/semantic/src/object_store_mutator.hpp` (B must not call it)
- `runtime/semantic/src/object_index.hpp` (read-only derived hierarchy acceleration)
- `runtime/semantic/CMakeLists.txt`
- `runtime/semantic/tests/CMakeLists.txt`
- `docs/notion/authority/04-semantic-schema/02-operation-model/restoreobjects-identity-state-tombstone-eligibility-v1-authority-closure-v0.1.md`

No `OperationEngine`, `AppliedOperationView`, `PreparedApplyPlan`, stateful validator, or B idempotency implementation currently exists. The following files are therefore planned creations, and each creation is tied to a package below:

- `runtime/semantic/include/canvas/semantic/stateful_validation.hpp`
- `runtime/semantic/include/canvas/semantic/operation_fingerprint.hpp`
- `runtime/semantic/include/canvas/semantic/idempotency.hpp`
- `runtime/semantic/include/canvas/semantic/staged_object_view.hpp`
- `runtime/semantic/include/canvas/semantic/hierarchy_validation.hpp`
- `runtime/semantic/include/canvas/semantic/connector_validation.hpp`
- `runtime/semantic/include/canvas/semantic/delete_closure.hpp`
- `runtime/semantic/include/canvas/semantic/restore_planner.hpp`
- `runtime/semantic/include/canvas/semantic/operation_state_validator.hpp`
- `runtime/semantic/include/canvas/semantic/apply_plan.hpp`
- `runtime/semantic/include/canvas/semantic/operation_engine.hpp`
- matching `.cpp` files under `runtime/semantic/src/`
- focused GoogleTest files under `runtime/semantic/tests/`

The plan permits adding a narrowly scoped helper only when its exact path and ownership are recorded in the package that needs it. It does not permit guessed edits to unrelated modules.

## Plan DAG

```text
B0 read-only context + staged overlay + fingerprint seam
 ├─ B1 OperationId idempotency gate
 ├─ B2 shared state/kind/version/field applicability
 │    ├─ B3 staged hierarchy and cycle validation
 │    └─ B4 connector/connectability/StablePort validation
 ├─ B5 delete subtree + connector fixed-point closure
 └─ B6 RestoreObjects staged eligibility
B2+B3+B4+B5+B6 ──> B7 remaining operation-specific validators
B1+B7 ──> B8 PreparedApplyPlan composition + OperationEngine facade
B8 ──> B9 Reference/Indexed decision and plan differential
B8+B9 ──> B10 complete 15-operation matrix + no-mutation gate
```

The decomposition keeps shared state resolution separate from operation-specific policy, because the repository has one common `ObjectRecord`/`ObjectStore` model but no existing stateful validator. Delete and restore are separate packages because they have opposite identity predicates and distinct staged-closure obligations. Differential testing follows composition rather than duplicating every package's implementation. B10 is the gate package, not a second validator implementation.

---

## Task Package B0 — Read-only Stateful Context and Staged View

**Purpose:** Establish the only stateful inputs B may consume and an in-memory staged resulting-state view that never mutates either ObjectStore implementation.

**Authority references:** Operation Payload + Validation Rules §validation pipeline; Operation Semantic Document §Validation/Prepare; Module Design Closure Axiom Semantic Core ownership; ObjectStore and ObjectIndex comments declaring parent/cycle validation as Operation/Plan responsibility.

**Dependencies:** GT-G1-03 PASS; existing `ObjectStore` API; A-lane `Operation`, `ObjectRecord`, and structural validator output.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/stateful_validation.hpp` and `runtime/semantic/src/stateful_validation.cpp` for `StatefulValidationContext`, `StatefulIssue`, `StatefulResult`, and read-only lookup adapters.
- Create `runtime/semantic/include/canvas/semantic/staged_object_view.hpp` and `runtime/semantic/src/staged_object_view.cpp` for an overlay containing existing records plus planned creates/replacements/deletes, with deterministic ObjectId enumeration.
- Create `runtime/semantic/include/canvas/semantic/operation_fingerprint.hpp` and `runtime/semantic/src/operation_fingerprint.cpp` for an encoding-neutral canonical fingerprint plus typed semantic-equality helper for a normalized typed Operation; it must not expose generated protobuf types or change wire schema.
- Modify `runtime/semantic/CMakeLists.txt` to compile the three new sources.
- Test (Create) `runtime/semantic/tests/stateful_context_test.cpp` and `runtime/semantic/tests/staged_object_view_test.cpp`.

**Interfaces consumed:** `const ObjectStore&`; normalized `Operation`; `OperationId`; `ObjectRecord`; existing deterministic `allObjects()`/`children()` behavior.

**Interfaces produced:**

- `using OperationFingerprint = std::vector<std::uint8_t>; OperationFingerprint fingerprintOperation(const Operation& normalized);`
- `bool canonicalPayloadEqual(const Operation& lhs, const Operation& rhs) noexcept;` comparing the normalized typed semantic projection (document/version fields and payload) and excluding OperationId, source metadata, ApplySource, and transport metadata.
- `struct AppliedOperationEntry { Operation canonical_operation; std::optional<OperationFingerprint> fingerprint; };`
- `class AppliedOperationView { virtual ~AppliedOperationView(); virtual std::optional<AppliedOperationEntry> find(const OperationId&) const = 0; }` (the B0 read-only seam; B1 supplies classification logic over it; the canonical operation is required for typed exact comparison)
- `struct StatefulValidationContext { const ObjectStore& objects; const AppliedOperationView& applied_operations; };`
- `class StagedObjectView { contains(id); find(id); allObjects(); children(parent); stageCreate(record); stageReplace(record); stageDelete(id); projection(); }` where all returned collections are ObjectId-deterministic.
- `enum class StatefulIssue { kNone, kObjectMissing, kObjectAlreadyExists, kInvalidKindVersion, kInvalidApplicability, kInvalidReference, kHierarchyCycle, kConnectorInvalid, kMaskStateInvalid, kTextStateInvalid, kOperationIdCollision };`

**RED tests and expected failure:** Add tests that compile against the named interfaces and assert (1) a staged create is visible only in the overlay, (2) a staged delete is absent only in the overlay, (3) parent/children order is deterministic, and (4) equal normalized Operations have equal fingerprints while a changed field does not. Before implementation the tests fail because the new headers/types and sources do not exist.

**Implementation obligations:** Keep the base ObjectStore const; make staged operations local value copies; reject a duplicate staged identity; never call `ObjectStoreMutator`; make fingerprint output independent of input insertion order because A normalization has already produced canonical ordering.

**GREEN verification:** Run the two focused tests and the existing reference/indexed store tests; inspect a mutation counter or before/after projection proving the base store is unchanged.

**Oracle/differential:** Build the same staged trace over Reference and Indexed stores and compare `projection()` and `children()` output. This is context infrastructure evidence, not C golden authority.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B0-context.json` with source SHA, test names, base projection before/after, and fingerprint vectors.

**Non-goals:** No idempotency classification, operation policy, canonical mutation, generation, ChangeSet, history, snapshot restore, or C expected outcomes.

**Exit criteria:** Context and overlay APIs compile; focused tests pass; no base-store mutation is observed; public headers have no Scene/Skia/Arc/Data/Host dependency.

**Block conditions:** `AUTHORITY_CONFLICT`, `MISSING_CONTRACT`, `WIRE_SCHEMA_CHANGE_REQUIRED`, or a need to change protobuf fields stops the package.

**Commit boundary:** One commit: `feat(g1): add read-only stateful validation context`.

**Performance constraints:** Single-ID reads use `find`/`contains`; overlay projection sorts only affected values and does not scan the base store for indexed lookups.

**Required final report:** Report created files, focused test output, base-store before/after projection, Reference/Indexed comparison, and any authority block classification.

---

## Task Package B1 — OperationId Idempotency Gate

**Purpose:** Classify a normalized OperationId before stateful identity/reference checks and make equal replay a non-plan disposition.

**Authority references:** Operation Payload + Validation Rules §Idempotence; Operation Semantic Document §Idempotency; Open/Restore/Catch-up §re-drive; Special Flows §duplicate OperationId.

**Dependencies:** B0 fingerprint/context; A normalization and structural validation.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/idempotency.hpp` and `runtime/semantic/src/idempotency.cpp`.
- Test (Create) `runtime/semantic/tests/idempotency_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** `OperationId`, `OperationFingerprint`, `AppliedOperationView` from B0, normalized `Operation`.

**Interfaces produced:**

- `enum class IdempotencyDisposition { kNew, kAlreadyApplied, kCollision };`
- `struct IdempotencyResult { IdempotencyDisposition disposition; };`
- `IdempotencyResult classifyOperation(const Operation&, const AppliedOperationView&);`

**RED tests and expected failure:** Add `NewIdIsNotFound`, `EquivalentPayloadIsAlreadyApplied`, `DifferentPayloadIsCollision`, and `RestoreEquivalentReplayStopsBeforeCollisionCheck`. They initially fail to compile because `idempotency.hpp` and the classifier are absent.

**Implementation obligations:** Look up the exact OperationId, then compare the normalized canonical semantic payload with `canonicalPayloadEqual` using typed exact equality. A stored fingerprint/digest may accelerate the decision: a hash mismatch is `kCollision`, while a hash match MUST still perform typed exact equality. Never use raw transport bytes, protobuf incidental serialization ordering, source metadata, ApplySource, or transport metadata as the correctness authority. Return `kAlreadyApplied` only after typed equality; return `kCollision` for typed inequality; return `kAlreadyApplied` without reading ObjectStore; and do not insert into the applied-operation view or mutate any state.

**GREEN verification:** Focused idempotency tests pass; an instrumented ObjectStore confirms equivalent replay performs zero lookups; the result is identical for LocalUndo, Replay, and Remote source labels because source metadata is outside this API.

**Oracle/differential:** Compare classifier output against a small independent map fixture, not against the classifier's own storage implementation.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-IDEM.json` containing new/equivalent/collision observations and the proof that equivalent Restore replay precedes existence checking.

**Non-goals:** No dedupe persistence, transport dedupe, revision/cursor, tombstone ledger, mutation, generation, ChangeSet, or history.

**Exit criteria:** Gate ordering is executable and tested; equal replay has no state lookup; collision is explicit; no new OperationId record is created by B.

**Block conditions:** Missing canonical fingerprint contract, any required wire/schema change, or an attempt to make equal replay depend on tombstone/history metadata produces `MISSING_CONTRACT` or `WIRE_SCHEMA_CHANGE_REQUIRED` and stops.

**Commit boundary:** One commit: `feat(g1): add operation idempotency classification`.

**Performance constraints:** Classification performs one applied-operation lookup and fingerprint comparison; it never enumerates ObjectStore records.

**Required final report:** Report new/equivalent/collision observations, equivalent-replay lookup count, and the exact source/test SHA.

---

## Task Package B2 — Shared Object State, Kind/Version, and Field Applicability

**Purpose:** Resolve actual ObjectStore state and released ObjectKind/kindVersion/field applicability for all operation validators without duplicating A structural rules.

**Authority references:** ObjectKind Version Registry V1; Field Registry V1; Object Content field table; Operation Payload Validation §cross-object validation; G1-04-A boundary §A/B.

**Dependencies:** B0 context/overlay; GT-G1-03 stores; A structural validator; registry YAML files `schema/axiom/v1/registry/object_kind_registry_v1.yaml` and `schema/axiom/v1/registry/field_registry_v1.yaml`.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/operation_state_validator.hpp` and `runtime/semantic/src/operation_state_validator.cpp` with shared `findRequired`, `requireAbsent`, `requireKindVersion`, and `requireFieldApplicable` helpers.
- Test (Create) `runtime/semantic/tests/stateful_kind_applicability_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** `StatefulValidationContext`, `StagedObjectView`, `ObjectStore`, `ObjectRecord`, `ObjectKind`, `PropertyPatch`, registry snapshots.

**Interfaces produced:**

- `enum class StateRule { kCreateAbsent, kEditExisting, kPlacementTarget, kTransformTarget, kSizeTarget, kVectorPathTarget, kImageTarget, kStrokeTarget, kRichTextTarget, kConnectorTarget };`
- `StatefulResult requireExisting(const StagedObjectView&, ObjectId);`
- `StatefulResult requireAbsent(const StagedObjectView&, ObjectId);`
- `StatefulResult requireKindVersion(const ObjectRecord&, ObjectKind, std::uint32_t);`
- `StatefulResult requirePropertyApplicability(const ObjectRecord&, std::uint32_t field_id, const PropertyValue* value);`
- `StatefulResult validateRecordStateForOperation(const ObjectRecord&, StateRule rule);`

**RED tests and expected failure:** Add tests for missing targets, duplicate create identities, wrong actual ObjectKind, unsupported kindVersion, applicable and inapplicable FieldIds, and a structurally valid property value on the wrong target kind. They fail initially because the stateful helper header/functions do not exist.

**Implementation obligations:** Use actual records, not payload-declared kind, as the authority for edits; require exact released kind/version pairs; distinguish missing from collision; apply Field Registry value type and applicable-kinds rules after A has checked tagged value structure; return one deterministic first state failure; keep resource/blob availability outside this package unless an explicit current authority makes it semantic.

**GREEN verification:** Focused tests pass against both store implementations; a lookup counter demonstrates the helper uses `find`/`contains` and does not mutate or enumerate the whole store for single-target checks.

**Oracle/differential:** ReferenceObjectStore decisions are the oracle; IndexedObjectStore must match decision and failure path for identical records.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-STATE.json` with actual-kind/version and field applicability cases.

**Non-goals:** No hierarchy closure, connector graph, delete cascade, restore tombstone policy, plan composition, or stable C error category mapping.

**Exit criteria:** Shared helpers cover existence/non-existence, actual kind/version, and FieldId applicability; positive/negative focused cases pass; no mutation seam is referenced.

**Block conditions:** Registry drift, an unpublished applicability rule, or a requirement to change the ObjectRecord wire shape stops with `AUTHORITY_CONFLICT` or `MISSING_CONTRACT`.

**Commit boundary:** One commit: `feat(g1): add shared state and applicability validation`.

**Performance constraints:** Single-target checks use indexed `find`; batch checks are bounded by payload and staged references.

**Required final report:** Report actual target kind/version, FieldId applicability outcomes, lookup instrumentation, and Reference/Indexed parity.

---

## Task Package B3 — Staged Hierarchy, Parent References, and Cycle Validation

**Purpose:** Validate parent existence and resulting hierarchy for batches that create or move objects, including cycles formed only after staging.

**Authority references:** Operation Semantic Document §resulting-state validation; Operation Structural Semantics §A/B boundary; OrderKey authority; Special Flows §delete subtree.

**Dependencies:** B0 staged view; B2 shared state helpers; normalized InsertObjects, RestoreObjects, SetPlacements, AddStroke, SplitStrokes payloads.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/hierarchy_validation.hpp` and `runtime/semantic/src/hierarchy_validation.cpp`.
- Test (Create) `runtime/semantic/tests/hierarchy_stateful_validation_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** `StagedObjectView`, `Placement`, `ObjectId`, vectors of candidate records/placement edits.

**Interfaces produced:**

- `struct HierarchyEdit { ObjectId object_id; Placement placement; };`
- `StatefulResult validateStagedHierarchy(const StagedObjectView&, std::span<const HierarchyEdit>);`
- `std::vector<ObjectId> resolveDescendants(const StagedObjectView&, ObjectId);`

**B3-P31-01 contract (frozen for P32):** `HierarchyEdit` represents the complete proposed `Placement` for one visible/staged object. The proposed parent and sibling order are both taken from `HierarchyEdit::placement`: parent authority is `edit.placement.parent_id` and sibling-order authority is `edit.placement.order_key`. There is no second parent source, parent override, expected-parent field, old-parent field, or implicit normalization rule.

`validateStagedHierarchy()` owns only staged visibility/existence, parent visibility/existence, resulting parent topology, cycle rejection, and deterministic traversal/diagnostics. It does not own `ObjectKind` parent-capability semantics. Do not infer a parent-capability matrix, child-role semantics, or a Sticky primary-RichText-child role; if a later operation requires such a rule, stop with `MISSING_CONTRACT`.

`resolveDescendants()` keeps the existing API and returns strict descendants only: it excludes the root `ObjectId`, returns IDs in deterministic `ObjectId` order, assumes the caller has established root existence, observes resulting-state visibility through `StagedObjectView`, and excludes staged-deleted objects.

Conceptually, `validateStagedHierarchy()` builds one private resulting parent projection from the base/staged visible hierarchy plus all edit placements, then validates parent existence and cycles against that complete projection. It must not mutate canonical state or validate edits through per-edit partial mutation.

**RED tests and expected failure:** Add valid parent, absent parent, self-parent, two-node staged cycle, three-node staged cycle, parent+child same InsertObjects batch, and SetPlacements cycle tests. They fail because the hierarchy validator is not yet defined.

**Implementation obligations:** Apply every complete `HierarchyEdit::placement` to one private resulting-state overlay before traversing; treat records in the same batch as available only when staged; reject absent parent and every cycle; preserve deterministic `ObjectId` ordering for diagnostics and returned descendants independently of unordered-map iteration, ObjectStore insertion history, platform, or Reference versus Indexed implementation; do not mutate canonical state; do not infer product Page roots, synthetic Document roots, ObjectKind parent-capability semantics, child roles, or Sticky primary-RichText-child roles.

**GREEN verification:** Focused tests pass for both stores and for batches whose input order is permuted after normalization; before/after base projections are equal.

**Oracle/differential:** Compare the resulting parent map and cycle decision against an independent test-only DFS model.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-HIER.json` containing staged parent maps, cycle witnesses, and no-mutation projections.

**Non-goals:** No connector cascade, semantic generation, renderer bounds, spatial index, or atomic commit.

**Exit criteria:** Parent/reference and staged cycle rules are deterministic and reusable by insert, restore, placement, split, and delete packages.

**Block conditions:** Any request to introduce a synthetic root or to accept an unstated orphan policy stops with `MISSING_CONTRACT`.

**Commit boundary:** One commit: `feat(g1): validate staged hierarchy state`.

**Performance constraints:** Descendant traversal uses `children(parent)` and cycle detection is bounded by the affected staged graph.

**Required final report:** Report staged parent map, cycle witnesses, descendant ordering, and base-store no-mutation proof.

---

## Task Package B4 — Connector References, Connectability, and StablePort Applicability

**Purpose:** Validate Connector target existence, released connectable ObjectKinds, endpoint references, and actual-target StablePort applicability before a plan is produced.

**Authority references:** Connector Referential Integrity + Anchor Contract V1; current Connector V1 StablePort closure; Semantic Leaf Structural Validation Closure V1 §Connector; ObjectKind registry; fixed V1 StablePort registry. B4 StablePort Authority is READY and requires no new semantic authority.

**Dependencies:** B0 staged view; B2 actual kind/version helpers; B3 staged hierarchy for target records; A static connector validator.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/connector_validation.hpp` and `runtime/semantic/src/connector_validation.cpp`.
- Test (Create) `runtime/semantic/tests/connector_stateful_validation_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** `ConnectorContent`, `AttachedEndpoint`, `AnchorRef`, `StagedObjectView`, `ObjectRecord`, released connectable-kind table, static A validation result.

**Interfaces produced:**

- `StatefulResult validateConnectorReferences(const StagedObjectView&, const ConnectorContent&);`
- `bool isConnectableObjectKind(ObjectKind, std::uint32_t kind_version) noexcept;`
- `StatefulResult validateStablePortForTarget(const ObjectRecord&, const StablePortAnchor&);`

**B4 StatefulIssue mapping (frozen for P32):**

- An `AttachedEndpoint` whose target is absent or staged-deleted returns `StatefulIssue::kInvalidReference`.
- An `AttachedEndpoint` whose target exists but whose actual `ObjectKind` is unknown, or whose actual `kindVersion` is unsupported/non-V1, returns `StatefulIssue::kInvalidKindVersion`. In particular, an actual `Shape` with `kindVersion = 2` is not a connector-invalid decision.
- An existing target with a released V1 kind/version that is not connectable returns `StatefulIssue::kConnectorInvalid`. The V1 non-connectable set is `VectorPath`, `RichText`, `VectorStroke`, `DabStroke`, `Connector`, and `Group`.
- A structurally A-valid `StablePortAnchor` that is not applicable to the actual released target kind/version returns `StatefulIssue::kConnectorInvalid`.
- A `FreePoint` endpoint after A structural validation returns `StatefulIssue::kNone`.
- An attached endpoint targeting Shape v1, Image v1, or Sticky v1 returns `StatefulIssue::kNone` when its AutoPerimeter or applicable StablePort anchor is valid.

Unsupported actual kind/version must not be mapped to `StatefulIssue::kConnectorInvalid`; B4 may reuse B2 actual-kind/version semantics.

**A/B structural boundary (frozen):** B4 consumes an A-normalized, A-stateless-structurally-valid `ConnectorContent`. The following failures remain owned by A and are not redefined or re-owned as B-CONN cases: zero `targetObjectId`; invalid endpoint variant or structural shape; invalid AutoPerimeter hint; non-finite or out-of-normalized-range hint; `hint == center`; `StablePort` `portId == 0`; `StablePort` `portId >= 5`; and invalid routing discriminant. `validateStablePortForTarget()` has the normative precondition that its `StablePortAnchor` already passed A structural validation. B4 does not turn the A `portId` 0/5..u32::MAX rejection into a second stateful rule.

**Deterministic endpoint failure precedence:** For structurally valid `ConnectorContent` (`start`, `end`, `routing`), validate `start` first and return its first B4 failure; validate `end` only when `start` succeeds. This `start-before-end` rule is B-local diagnostic precedence only, not a new C golden authority, wire error ordering, or protocol-visible numeric error guarantee.

**Connectable V1 matrix:**

| Actual target kind/version | Connectable | B4 outcome when attached |
| --- | --- | --- |
| Shape v1 | yes | Continue to anchor applicability; valid anchor is `kNone` |
| Image v1 | yes | Continue to anchor applicability; valid anchor is `kNone` |
| Sticky v1 | yes | Continue to anchor applicability; valid anchor is `kNone` |
| VectorPath v1 | no | `kConnectorInvalid` |
| RichText v1 | no | `kConnectorInvalid` |
| VectorStroke v1 | no | `kConnectorInvalid` |
| DabStroke v1 | no | `kConnectorInvalid` |
| Connector v1 | no | `kConnectorInvalid` |
| Group v1 | no | `kConnectorInvalid` |

No future `Frame`, `Card`, `Table`, or custom-object capability is introduced. Connector-to-Connector attachment is `kConnectorInvalid`.

**StablePort V1 matrix:** StablePort semantic identity is `(target ObjectKindId, target kindVersion, portId)`. The fixed V1 registry is `1 = TOP`, `2 = RIGHT`, `3 = BOTTOM`, `4 = LEFT`. Shape v1, Image v1, and Sticky v1 accept each of ports `1`, `2`, `3`, and `4`; all other actual target kind/version combinations are not applicable. B4 must not fallback to AutoPerimeter, renumber, normalize, clamp, or derive ports from renderer geometry.

**RED tests and expected failure:** Add named tests for every semantic category below; they initially fail because the stateful connector interfaces do not exist.

- `FreeFreeIsValid`.
- `ShapeV1AutoPerimeterIsValid`, `ImageV1AutoPerimeterIsValid`, and `StickyV1AutoPerimeterIsValid`.
- `ShapeV1Ports1Through4AreValid`, `ImageV1Ports1Through4AreValid`, and `StickyV1Ports1Through4AreValid`.
- `MissingAttachedTargetReturnsInvalidReference` and `StagedDeletedTargetReturnsInvalidReference`.
- `UnsupportedTargetKindVersionReturnsInvalidKindVersion`, including Shape `kindVersion = 2` and any safe actual kind/version mismatch fixture.
- A table-driven test covering VectorPath, RichText, VectorStroke, DabStroke, Connector, and Group V1 targets, each returning `kConnectorInvalid`.
- A same-batch test where a newly staged Shape/Image/Sticky target and a Connector referencing it return `kNone`.
- `StartInvalidReferenceWinsOverEndNonConnectable` and `StartNonConnectableWinsOverEndInvalidReference`, proving `start-before-end`.
- StablePort not-applicable-to-actual-target returns `kConnectorInvalid`; A-only `portId` 0 and `portId >= 5` rejection remains existing A static coverage and is not duplicated as a B-CONN normative case.

**Implementation obligations:** Reuse A for endpoint shape, routing discriminant, and finite-value checks; resolve each attached target through the staged view; evaluate actual staged kind/version before connectability and StablePort applicability; enforce only the released connectable table above; preserve free endpoints; and never store routed paths.

**GREEN verification:** Focused tests pass for all named categories, including same-batch staged visibility and start-before-end determinism. Rejection and success paths leave the base `ObjectStore`, the base `StagedObjectView`, and any `ObjectIndex` unchanged.

**Oracle/differential and no-mutation:** `ReferenceObjectStore` and `IndexedObjectStore` must produce identical decisions for `kNone`, `kInvalidReference`, `kInvalidKindVersion`, and `kConnectorInvalid`, including actual target kind/version and StablePort identity. B4 evaluation must not mutate either store projection, the staged-view base, or the index.

**Lookup/performance contract:** Each `AttachedEndpoint` performs at most one staged target lookup. Evidence must distinguish source inspection from instrumented runtime measurement: only a counter-based test may be reported as a runtime measurement. B4 never calls `allObjects()`, renderer geometry, route computation, Skia, Scene, or SpatialIndex.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-CONN.json` must record `sourceCommit`, `testedCommit`, branch, authorization record, plan commit, each endpoint case, target ObjectId, actual target ObjectKind/kindVersion, connectability decision, anchor type, StablePort identity (target kind, target kindVersion, portId), StatefulIssue, both start/end precedence cases, staged target visibility, Reference/Indexed parity, lookup measurements, no-mutation proof, and the A/B structural-boundary statement. It may reference A regression evidence for `portId` 0 and `>= 5`, but must not claim ownership of those A rejections.

**Non-goals:** No route computation, renderer hit testing, geometry-derived anchor mutation, C golden expected outcome, or connector persistence.

**Exit criteria:** B4 StablePort Authority is READY; all attached references are checked against actual staged state and released connectability; fixed V1 StablePort identities are authority-bound; free endpoints remain valid.

**Block conditions:** Connector authority/repository disagreement or a request to persist a route yields `AUTHORITY_CONFLICT` or `MISSING_CONTRACT`.

**Commit boundary:** One commit: `feat(g1): validate stateful connector references`.

**Performance constraints:** Each attached endpoint performs one staged target lookup and never invokes renderer geometry or routing.

**Required final report:** Report endpoint target IDs, actual kinds/versions, StablePort decisions, authority status, and differential output.

---

## Task Package B5 — Delete Subtree and Connector Fixed-Point Closure

**Purpose:** Resolve DeleteObjects into the complete hierarchy descendant set plus the fixed-point set of Connectors attached to any deleted target.

**Authority references:** Operation Semantic Document §Cascade Connector Delete; Special Flows §delete subtree; ObjectStore children contract; ChangeSet authority only for the later consumer, not implemented here.

**Dependencies:** B0 staged view; B2 existence helpers; B3 descendants; B4 connector endpoint parsing/reference helper.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/delete_closure.hpp` and `runtime/semantic/src/delete_closure.cpp`.
- Test (Create) `runtime/semantic/tests/delete_closure_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** DeleteObjects target IDs, `StagedObjectView`, `ConnectorContent`, deterministic children lookup.

**Interfaces produced:**

- `struct DeleteClosure { std::vector<ObjectId> requested_delete_ids; std::vector<ObjectId> resolved_hierarchy_closure; std::vector<ObjectId> resolved_connector_cascade_closure; std::vector<ObjectId> final_delete_set; };`
- `StatefulResult resolveDeleteClosure(const StagedObjectView&, std::span<const ObjectId>, DeleteClosure* out);`

**RED tests and expected failure:** Add named tests for (1) a direct target referenced by a Connector, (2) a descendant target referenced by a Connector, (3) multiple Connectors referencing one deleted target, (4) one Connector whose two endpoints both hit the delete set and is deduplicated once, (5) repeated closure evaluation producing the same final set, and (6) input-order permutation producing the same closure. Do not construct Connector-to-Connector attachment chains: V1 Connectable kinds are Shape v1, Image v1, and Sticky v1, so Connector is not a legal Connector target. They fail because closure interfaces are absent.

**Implementation obligations:** Start with requested existing IDs; add all descendants; repeatedly scan only the necessary connector relation until no new Connector ID is added; return unique ObjectIds sorted by unsigned identity bytes; reject a missing requested target; do not include unrelated free/free connectors; do not mutate the store.

**GREEN verification:** Closure tests pass, fixed point is stable on a second run, and the base ObjectStore/index projection is unchanged.

**Oracle/differential:** Compare against an independent graph traversal fixture and require Reference/Indexed equality. Instrument Indexed lookup to ensure hierarchy traversal uses its index rather than an ObjectId full scan.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-DELETE.json` with deterministic requested IDs, hierarchy closure, connector cascade closure (including additions per iteration), and final delete set.

**Non-goals:** No actual erase, ChangeSet deleted flags, history before-images, generation, or publication.

**Exit criteria:** Delete plan input is a deterministic complete closure and supports whole-op rejection when any requested target is missing.

**Block conditions:** Ambiguous connector cascade relation or any need to commit deletion stops with `MISSING_CONTRACT` or `GT_G1_05_REQUIRED`.

**Commit boundary:** One commit: `feat(g1): resolve delete subtree connector closure`.

**Performance constraints:** Hierarchy traversal uses the children index; connector closure iterates until a stable fixed point and emits sorted unique IDs.

**Required final report:** Report the deterministic `requested_delete_ids`, `resolved_hierarchy_closure`, `resolved_connector_cascade_closure`, and `final_delete_set` projections, connector additions by fixed-point iteration, and Reference/Indexed store parity.

---

## Task Package B6 — RestoreObjects Identity-State and Staged Resulting-State Planner

**Purpose:** Implement the restore-specific state predicate and all twelve RST-B cases without introducing tombstone or history semantics.

**Authority references:** `docs/notion/authority/04-semantic-schema/02-operation-model/restoreobjects-identity-state-tombstone-eligibility-v1-authority-closure-v0.1.md`; Operation Semantic Document §Idempotency/Prepare; Open/Restore/Catch-up §snapshot versus replay; ObjectKind, Connector, and hierarchy authorities.

**Dependencies:** B0 context/fingerprint; B1 idempotency; B2 kind/existence; B3 hierarchy; B4 connector validation; the current repository RestoreObjects authority mirror.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/restore_planner.hpp` and `runtime/semantic/src/restore_planner.cpp`.
- Test (Create) `runtime/semantic/tests/restore_stateful_validation_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** `RestoreObjectsOp`, `StatefulValidationContext`, `AppliedOperationView`, `StagedObjectView`, shared kind/reference/hierarchy/connector validators.

**Interfaces produced:**

- `StatefulResult validateRestoreObjects(const Operation&, const RestoreObjectsOp&, const StatefulValidationContext&, RestorePlanInputs* out);`
- `struct RestorePlanInputs { std::vector<ObjectRecord> creates; std::vector<ObjectId> staged_connector_targets; };`

**RED tests and expected failure:** Implement named tests `RST-B01` through `RST-B12` before the planner exists:

| Case | Required assertion |
| --- | --- |
| RST-B01 | All candidate IDs absent and records valid produces restore plan inputs. |
| RST-B02 | Existing same record returns `ObjectAlreadyExists`; no overwrite. |
| RST-B03 | Existing different record returns `ObjectAlreadyExists`; no upsert. |
| RST-B04 | Parent and child in one payload succeed when staged hierarchy is valid. |
| RST-B05 | Child with absent parent returns `InvalidReference`/hierarchy failure. |
| RST-B06 | Target and cascaded Connector restored together succeed when staged state is valid. |
| RST-B07 | Connector alone with absent target returns `InvalidReference`. |
| RST-B08 | Same OperationId/equivalent payload returns `AlreadyApplied` before existence lookup. |
| RST-B09 | New OperationId after prior restore returns `ObjectAlreadyExists`. |
| RST-B10 | One collision in a multi-record restore rejects the whole operation. |
| RST-B11 | LocalUndo, Replay, and Remote produce the same result for equal base state and operation. |
| RST-B12 | Snapshot without deleted-ID ledger accepts an absent-ID tail restore when staged state is valid. |

These tests initially fail because the restore planner and the B idempotency/state interfaces are absent.

**Implementation obligations:** Apply B1 first; for a new ID require every candidate ObjectId to be absent in the current/apply-base ObjectStore first, then stage all candidate records and validate the complete resulting staged state; allow same-payload target+Connector restoration; reject Connector-only restoration; never compare against or create tombstone metadata; treat same-record collision exactly like different-record collision; preserve whole-op atomicity and deterministic create ordering.

**GREEN verification:** All RST-B01..B12 pass for Reference and Indexed stores; operation-source parameterization proves equal results; an instrumented applied-operation view proves RST-B08 performs no existence lookup.

**Oracle/differential:** Independent fixture state contains only ObjectStore records and applied-operation fingerprints. There is deliberately no tombstone ledger in the oracle.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-RESTORE.json` with all twelve case IDs, source labels, collision decisions, staged graph projection, and no-mutation proof.

**Non-goals:** Snapshot bootstrap implementation, deletion metadata, history proof, sync metadata, Atomic Apply, generation, ChangeSet, or C final golden corpus.

**Exit criteria:** RST-B01..B12 are green; restore semantics are source-independent and state/tombstone-free; all candidate collisions reject the complete operation.

**Block conditions:** Notion/repository disagreement or any request to infer tombstone eligibility beyond the current authority stops with `MISSING_CONTRACT` or `AUTHORITY_CONFLICT`.

**Commit boundary:** One commit: `feat(g1): plan restore identity and staged-state validation`.

**Performance constraints:** Each candidate identity is checked once against current/staged state; no tombstone scan or unrelated full-store scan is permitted.

**Required final report:** Report RST-B01..RST-B12 individually, source-label parity, collision/no-lookup ordering, authority mirror path/hash, and any block condition.

---

## Task Package B7 — Remaining Operation-Specific Stateful Validators

**Purpose:** Add state-dependent rules for the thirteen non-delete/non-restore families while reusing B2–B4 primitives and preserving operation-specific reviewability.

**Authority references:** Operation Payload Validation; ObjectKind and Field registries; RichText wire/font authority; Brush/Stroke and EraseMask authorities; Image/Object Content authorities; Connector authority for SetConnectorContent.

**Dependencies:** B2 shared state; B3 hierarchy where placement is affected; B4 connector validation; B0 staged view. B5 and B6 are consumed for shared closure/planner composition but are not reimplemented.

**Files (Create / Modify / Test):**

- Modify `runtime/semantic/include/canvas/semantic/operation_state_validator.hpp` and `runtime/semantic/src/operation_state_validator.cpp` to add explicit per-operation state validation entry points.
- Test (Create) `runtime/semantic/tests/operation_state_validator_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** Existing fifteen payload structs; B2/B3/B4 APIs; current target records and staged records.

**Interfaces produced:**

- `StatefulResult validateInsertObjectsState(...)`
- `StatefulResult validateSetPlacementsState(...)`
- `StatefulResult validateSetTransformsState(...)`
- `StatefulResult validatePatchPropertiesState(...)`
- `StatefulResult validateSetObjectSizeState(...)`
- `StatefulResult validateSetVectorPathGeometryState(...)`
- `StatefulResult validateSetImageContentState(...)`
- `StatefulResult validateAddStrokeState(...)`
- `StatefulResult validateSplitStrokesState(...)`
- `StatefulResult validateAddEraseMasksState(...)`
- `StatefulResult validateRemoveEraseMasksState(...)`
- `StatefulResult validateEditRichTextState(...)`
- `StatefulResult validateSetConnectorContentState(...)`

Each function consumes a const state view and writes only operation-specific plan inputs into an output value; none receives a mutator.

**RED tests and expected failure:** Add one positive and one state-negative test for each operation listed in the interface block. Required state cases include duplicate insert identity, missing placement/transform/size/content target, wrong target kind, inapplicable FieldId, missing stroke source, replacement collision, current mask missing/already present, missing RichText paragraph or out-of-range scalar range, and SetConnectorContent attached-target/connectability failure. Tests initially fail because these entry points do not exist.

**Implementation obligations:** Keep each family explicit: Insert creates only absent records and invokes B3/B4 as needed; SetPlacements validates target/parent and staged cycle; SetTransforms and SetObjectSize validate actual target kind; PatchProperties checks actual Field Registry applicability and current bag state; SetVectorPathGeometry targets VectorPath v1; SetImageContent targets Image v1 without treating blob availability as ObjectStore state; AddStroke creates an absent stroke record; SplitStrokes requires the source stroke and absent replacement IDs, validates staged replacement graph, and resolves source replacement semantics into plan inputs; Add/RemoveEraseMasks target stroke kinds and check mask identity against current/staged masks; EditRichText validates current paragraph identity and scalar ranges against staged text; SetConnectorContent validates actual Connector v1 plus B4 endpoint references.

**GREEN verification:** Run the focused operation-state test; run existing A structural tests to ensure no rule moved backward; assert each rejection leaves both store projections unchanged.

**Oracle/differential:** Independent state fixtures define actual records and paragraph/mask/stroke identities. Reference and Indexed decisions must match for every case.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-OP15-stateful.json` with operation name, case ID, actual target kind/version, decision, and state projection.

**Non-goals:** No final C conformance authority, no wire decoder changes, no semantic commit, no generation/ChangeSet/history, and no renderer/resource cache policy.

**Exit criteria:** All fifteen operations have explicit state validators across B2/B3/B4/B5/B6/B7; operation-specific applicability is reviewable by name; all focused positives/negatives pass.

**Block conditions:** A required operation-specific state rule absent from current authority, any proto change, or any need to publish an outcome golden stops with `MISSING_CONTRACT`, `WIRE_SCHEMA_CHANGE_REQUIRED`, or `CROSS_LANE_SCOPE_REQUIRED`.

**Commit boundary:** One commit: `feat(g1): add operation-specific state validation`.

**Performance constraints:** Single-target operations use indexed lookup; batch work scales with affected payload records and required staged references.

**Required final report:** Report one stateful positive/negative result per operation, actual-target applicability, regression output, and any missing authority rule.

---

## Task Package B8 — Deterministic PreparedApplyPlan and OperationEngine Planning Facade

**Purpose:** Compose B1 and B2–B7 into one deterministic read-only planning entry point that stops exactly before Atomic Apply.

**Authority references:** Operation Semantic Document §pipeline, §Prepare Apply Plan, §Atomicity; Module Design Closure Axiom Semantic Core; G1 package Wave 3 ApplyPlan boundary.

**Dependencies:** B1 idempotency; B2 shared state; B3 hierarchy; B4 connector; B5 delete; B6 restore; B7 operation validators.

**Files (Create / Modify / Test):**

- Create `runtime/semantic/include/canvas/semantic/apply_plan.hpp` and `runtime/semantic/src/apply_plan.cpp`.
- Create `runtime/semantic/include/canvas/semantic/operation_engine.hpp` and `runtime/semantic/src/operation_engine.cpp`.
- Test (Create) `runtime/semantic/tests/apply_plan_test.cpp` and `runtime/semantic/tests/operation_engine_boundary_test.cpp`.
- Modify `runtime/semantic/CMakeLists.txt` and `runtime/semantic/tests/CMakeLists.txt`.

**Interfaces consumed:** Normalized `Operation`; `StatefulValidationContext`; B1 classifier; B2–B7 state/planner outputs.

**Interfaces produced:**

- `enum class PrepareDisposition { kPrepared, kAlreadyApplied, kRejected };`
- `struct PreparedApplyPlan { Operation operation; PrepareDisposition disposition; std::vector<ObjectRecord> creates; std::vector<ObjectRecord> replacements; std::vector<ObjectId> requested_delete_ids; std::vector<ObjectId> resolved_hierarchy_closure; std::vector<ObjectId> resolved_connector_cascade_closure; std::vector<ObjectId> final_delete_set; std::vector<ObjectId> touched_objects; std::vector<std::uint32_t> touched_fields; };`
- `struct PrepareResult { PrepareDisposition disposition; StatefulResult error; std::optional<PreparedApplyPlan> plan; };`
- `PrepareResult prepareApplyPlan(const Operation&, const StatefulValidationContext&);`
- `class OperationEngine final { public: PrepareResult prepare(const Operation&, const StatefulValidationContext&) const; };`

`PreparedApplyPlan` must own values, use deterministic ObjectId/FieldId ordering, and contain no `SemanticGeneration`, `ChangeSet`, `CanonicalCommitStamp`, history, callback, store pointer, renderer object, or publication token.

**RED tests and expected failure:** Add plan tests for deterministic equivalent input, whole-op rejection with no partial plan, AlreadyApplied with no plan, collision with no plan, delete closure materialization, restore create materialization, and a compile-time boundary test that `PreparedApplyPlan` has no mutation API. Add a test that calls `OperationEngine::prepare` for each operation family. They initially fail because the plan and facade do not exist.

**Implementation obligations:** Enforce exact stage order: classify idempotency first; on `AlreadyApplied` stop; on collision stop; then invoke stateful operation validation; compose one immutable plan; never call `ObjectStoreMutator`; sort all output collections deterministically; reject if any required sub-validator fails; make repeated planning against unchanged state byte/projection equivalent.

**GREEN verification:** Focused plan/boundary tests pass; a spy ObjectStore verifies no mutation method is reachable; repeated calls produce equal plan projections; existing A tests and store tests remain green.

**Oracle/differential:** Compare plan projection against an independent plan recorder containing only expected IDs/records/fields. Do not treat the production plan serializer as a golden authority.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-PLAN.json` with disposition, deterministic projection digest, stage trace, explicit requested/hierarchy/connector-cascade/final-delete partitions, and proof that no commit-side field exists.

**Non-goals:** Atomic Apply, SemanticDocument ownership, revision/generation, ChangeSet, CanonicalCommitStamp, History, DataBridge, Outbox, local echo, or GT-G1-05 integration.

**Exit criteria:** A caller can obtain only `PreparedApplyPlan`, `AlreadyApplied`, or a rejection; plan output is deterministic and complete enough for a later commit owner; no canonical state changes.

**Block conditions:** Any plan field requiring generation/ChangeSet/history, mutation callback, or publication dependency stops with `GT_G1_05_REQUIRED` or `CROSS_LANE_SCOPE_REQUIRED`.

**Commit boundary:** One commit: `feat(g1): compose deterministic prepared apply plans`.

**Performance constraints:** Preparation is bounded by affected targets, staged records, and required connector closure; repeated preparation does not rebuild unrelated indexes.

**Required final report:** Report stage trace, disposition, plan projection digest, no-mutation spy results, operation-family coverage, and explicit confirmation that no post-plan field was added.

---

## Task Package B9 — ReferenceObjectStore versus IndexedObjectStore Differential

**Purpose:** Prove that state-dependent decisions and `PreparedApplyPlan` projections are equivalent across the independent reference and indexed stores.

**Authority references:** GT-G1-03 ObjectStore contract; G1 package ReferenceObjectStore oracle rule; semantic conformance authority differential principle; B boundary rule that B-DIFF is not C.

**Dependencies:** B8 complete; existing `runtime/semantic/tests/object_store_differential_test.cpp`; both store implementations unchanged in semantic behavior.

**Files (Create / Modify / Test):**

- Test (Create) `runtime/semantic/tests/stateful_validation_differential_test.cpp`.
- Create `verification/tools/compare_g1_04_b_plans.py` only if the existing evidence tooling cannot compare JSON projections without production coupling; otherwise record the existing tool path in the evidence.
- Modify `runtime/semantic/tests/CMakeLists.txt` and, only if required by the test target, `runtime/semantic/CMakeLists.txt`.

**Interfaces consumed:** `OperationEngine::prepare`, `StatefulValidationContext`, Reference/Indexed stores, deterministic plan projection.

**Interfaces produced:** A test-only `DifferentialObservation` containing `disposition`, state issue, plan projection, and final store/index projection; no new production semantic authority.

**RED tests and expected failure:** Add replayed traces covering inserts, placements, connector references, delete cascades, restore batches, mask/text state, and all fifteen operation names. Before B8 exists they fail because no planning facade is available; after B8, deliberately inject a divergent indexed/reference fixture and require the test to expose the mismatch.

**Implementation obligations:** Seed identical canonical records through the existing internal test/bootstrap seam; run the same normalized Operations; compare disposition, issue, sorted creates/replacements/requested-delete IDs/hierarchy closure/connector-cascade closure/final delete set/touched IDs/fields, and unchanged base projection; separately assert indexed lookup instrumentation does not perform ObjectId full scans on single-target paths.

**GREEN verification:** Differential test passes for deterministic fixtures and randomized bounded traces; any mismatch reports the first operation index, OperationId, semantic path, and store side.

**Oracle/differential:** ReferenceObjectStore is the state oracle; B9 compares the two implementations only for B state decisions and plan projections. It does not bless final positive/negative intent or C golden error categories.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-DIFF.json` and optional `verification/evidence/gates/G1/<commit>/GT-G1-04-B/B-DIFF-lookup.json`.

**Non-goals:** Rewriting ObjectIndex, changing store APIs, implementing Atomic Apply, or creating GT-G1-04-C's independent fixture compiler/golden corpus.

**Exit criteria:** Reference and Indexed agree for the B trace suite; indexed lookup constraints are evidenced; no production state mutation occurs during planning.

**Block conditions:** A store behavior discrepancy that cannot be localized, a required ObjectStore API expansion, or an attempt to use C golden data yields `TEST_ORACLE_INSUFFICIENT` or `CROSS_LANE_SCOPE_REQUIRED`.

**Commit boundary:** One commit: `test(g1): add stateful reference-indexed differential`.

**Performance constraints:** Differential instrumentation must prove zero ObjectId full scans on indexed single-target paths; bounded fixture enumeration is allowed for batch traces.

**Required final report:** Report first divergence (operation index/OperationId/path/store), lookup counts, deterministic fixture seed, and final parity result.

---

## Task Package B10 — Complete Fifteen-Operation Matrix and B No-Mutation Gate

**Purpose:** Assemble the B gate evidence, prove all fifteen families are statefully covered, and prove planning/rejection never changes canonical state.

**Authority references:** all B authorities listed in the header; Semantic Conformance Golden Corpus §operation list and mandatory special cases as a coverage input only; 07-03 whole-op atomicity; 07-11 boundary that post-commit generation/ChangeSet belongs after Apply.

**Dependencies:** B1–B9; existing A no-mutation test `runtime/semantic/tests/a_lane_no_mutation_test.cpp`; existing store differential tests.

**Files (Create / Modify / Test):**

- Test (Create) `runtime/semantic/tests/g1_04_b_operation_matrix_test.cpp`.
- Test (Create) `runtime/semantic/tests/g1_04_b_no_mutation_test.cpp`.
- Modify `runtime/semantic/tests/CMakeLists.txt`.
- Create evidence-only JSON under `verification/evidence/gates/G1/<commit>/GT-G1-04-B/`: `operation-matrix.json`, `no-mutation-results.json`, `summary.json`, and one package artifact for each B-IDEM/B-STATE/B-HIER/B-CONN/B-DELETE/B-RESTORE/B-OP15/B-PLAN/B-NOMUT/B-DIFF family.

**Interfaces consumed:** `OperationEngine::prepare`, both ObjectStore implementations, test/bootstrap mutator only for setup, all package evidence records.

**Interfaces produced:** Gate matrix rows with operation name, positive/negative state case, disposition, issue, deterministic plan projection, before/after canonical projection, and store implementation; a summary that explicitly says B ends at `PreparedApplyPlan`.

**RED tests and expected failure:** Add matrix rows for exactly these fifteen operations: InsertObjects, DeleteObjects, RestoreObjects, SetPlacements, SetTransforms, PatchProperties, SetObjectSize, SetVectorPathGeometry, SetImageContent, AddStroke, SplitStrokes, AddEraseMasks, RemoveEraseMasks, EditRichText, SetConnectorContent. For each, add at least one valid plan and one state rejection. Add rejection snapshots for missing target, wrong kind, missing parent/reference, cycle, connector target, mask/text state, and whole-batch collision. Before B completion the matrix fails because one or more package APIs/evidence files are absent.

**Implementation obligations:** Keep operation rows independent and reviewable; include RST-B01..B12 references in the Restore rows; assert every rejected or AlreadyApplied operation leaves ObjectStore projection, ObjectIndex projection, and any caller-supplied applied-operation view unchanged; assert prepared plans do not mutate state; ensure no row claims a generation, ChangeSet, commit stamp, history, or publication result.

**GREEN verification:** Run all semantic CTest targets, then the B matrix and no-mutation tests; generate evidence with the exact source commit and command lines; inspect that only the planned documentation/evidence files and B implementation files changed.

**Oracle/differential:** Use B9 observations and ReferenceObjectStore projections. Keep C's human-reviewed intent and final expected error authority out of the matrix.

**Evidence artifact:** `verification/evidence/gates/G1/<commit>/GT-G1-04-B/operation-matrix.json`, `no-mutation-results.json`, `summary.json`, plus the ten family artifacts `B-IDEM`, `B-STATE`, `B-HIER`, `B-CONN`, `B-DELETE`, `B-RESTORE`, `B-OP15`, `B-PLAN`, `B-NOMUT`, and `B-DIFF`.

**Non-goals:** No final Gate PASS declaration, no GT-G1-04-C implementation, no GT-G1-05, no changes to A authority or wire schema.

**Exit criteria:** Fifteen operation rows are present; RST-B01..B12 are linked; all ten evidence families are source-bound; no-mutation checks pass for both stores; B review can independently inspect the plan boundary.

**Block conditions:** Missing package evidence, a production/test mutation outside B scope, an uncovered operation, an unresolved authority conflict, or any post-plan commit behavior yields `TEST_ORACLE_INSUFFICIENT`, `AUTHORITY_CONFLICT`, or `CROSS_LANE_SCOPE_REQUIRED`; stop rather than fill the gap with inferred semantics.

**Commit boundary:** One evidence-only commit after all B implementation commits: `evidence(g1): bind GT-G1-04-B stateful validation package`.

**Performance constraints:** Matrix/evidence generation is deterministic and bounded; it must not require renderer, network, storage, or cloud services.

**Required final report:** Report the exact source/evidence commit pair, the independent P32 Authorization Record/baseline used for execution, 15/15 operation coverage, RST-B01..RST-B12 mapping, all ten evidence families, no-mutation result, differential result, `GT-G1-04-B implementation/evidence = READY_FOR_P34_REVIEW`, `GT-G1-04-C = NOT AUTHORIZED / DEFERRED`, `GT-G1-05 = NOT AUTHORIZED`, and that P34 retains the final Gate verdict.

---

## Coverage Matrix — Fifteen Operations to Packages

| Operation | Stateful ownership | Plan/evidence package |
| --- | --- | --- |
| InsertObjects | B2 identity/kind + B3 staged parent/cycle + B4 connector references | B7, B8, B10 |
| DeleteObjects | B2 target existence + B3 descendants + B5 fixed-point connector closure | B5, B8, B10 |
| RestoreObjects | B1 gate + B2 absent identity/kind + B3/B4 resulting graph + B6 | B6, B8, B10 |
| SetPlacements | B2 target/parent + B3 resulting hierarchy | B7, B8, B10 |
| SetTransforms | B2 actual target kind/version and target existence | B7, B8, B10 |
| PatchProperties | B2 actual FieldId applicability/current target kind | B7, B8, B10 |
| SetObjectSize | B2 actual target kind/version and existence | B7, B8, B10 |
| SetVectorPathGeometry | B2 VectorPath v1 target and existence | B7, B8, B10 |
| SetImageContent | B2 Image v1 target and existence; resource availability remains outside ObjectStore state | B7, B8, B10 |
| AddStroke | B2 absent identity/kind + B3/B4 references if present | B7, B8, B10 |
| SplitStrokes | B2 source/replacement identities + B3 staged replacements | B7, B8, B10 |
| AddEraseMasks | B2 stroke target + current/staged mask identity | B7, B8, B10 |
| RemoveEraseMasks | B2 stroke target + current mask existence | B7, B8, B10 |
| EditRichText | B2 RichText v1 target + staged paragraph/scalar ranges | B7, B8, B10 |
| SetConnectorContent | B2 Connector v1 target + B4 attached target/connectability/StablePort | B4, B7, B8, B10 |

## Evidence Matrix

| Evidence family | Required proof | Producing packages |
| --- | --- | --- |
| `B-IDEM` | New/equivalent/collision classification; gate precedes state lookup | B1, B10 |
| `B-STATE` | Existence, non-existence, actual kind/version, FieldId applicability | B2, B10 |
| `B-HIER` | Parent/reference existence and staged cycle rejection | B3, B10 |
| `B-CONN` | Connector target, connectability, actual StablePort applicability | B4, B10 |
| `B-DELETE` | Descendant closure plus Connector fixed-point closure | B5, B10 |
| `B-RESTORE` | RST-B01..B12, no tombstone ledger, source-independent result | B6, B10 |
| `B-OP15` | Positive/negative stateful rows for all fifteen Operations | B7, B10 |
| `B-PLAN` | Deterministic immutable PreparedApplyPlan projection | B8, B10 |
| `B-NOMUT` | Rejection, AlreadyApplied, and planning preserve state/indexes | B8, B10 |
| `B-DIFF` | Reference/Indexed decision and plan equivalence, plus lookup instrumentation | B9, B10 |

`B-DIFF` is limited to B state decisions and plan projections. It is not GT-G1-04-C's independent golden authority.

## Restore Matrix — RST-B01..RST-B12 Mapping

| Case | Exact test/package mapping |
| --- | --- |
| RST-B01 | `runtime/semantic/tests/restore_stateful_validation_test.cpp` `RST_B01_AllCandidateIdsAbsentProducesPlan`, B6/B-RESTORE |
| RST-B02 | same file `RST_B02_ExistingSameRecordRejects`, B6/B-RESTORE |
| RST-B03 | same file `RST_B03_ExistingDifferentRecordRejects`, B6/B-RESTORE |
| RST-B04 | same file `RST_B04_ParentAndChildSamePayloadUsesStagedHierarchy`, B6+B3 |
| RST-B05 | same file `RST_B05_AbsentParentRejects`, B6+B3 |
| RST-B06 | same file `RST_B06_TargetAndCascadedConnectorTogetherSucceeds`, B6+B4 |
| RST-B07 | same file `RST_B07_ConnectorAloneAbsentTargetRejects`, B6+B4 |
| RST-B08 | same file `RST_B08_EquivalentOperationIdStopsBeforeExistence`, B6+B1 |
| RST-B09 | same file `RST_B09_NewOperationIdCollidesAfterPriorRestore`, B6+B1+B2 |
| RST-B10 | same file `RST_B10_OneCollisionRejectsWholeBatch`, B6+B2 |
| RST-B11 | same file `RST_B11_SourceLabelsHaveSameResult`, B6 |
| RST-B12 | same file `RST_B12_SnapshotWithoutTombstoneLedgerAllowsValidTail`, B6 |

## Execution Prerequisites Before P32

P32 remains `NOT AUTHORIZED`. Before any implementation execution can begin, the following must be resolved and source-bound:

1. This plan, the repository RestoreObjects authority mirror, and the B4 StablePort closure require independent review before a separate P32 authorization; this document does not grant that authorization.
2. B4 StablePort Authority is READY. It requires no new semantic authority: the fixed V1 registry and actual target kind/version revalidation are current authority.
3. Keep GT-G1-04-C deferred until B has observable implementation semantics and a separate C authorization; do not use this plan to create C fixtures or expected outcomes.
4. Keep GT-G1-05 unauthorized; any request to add Atomic Apply, SemanticGeneration, ChangeSet, CanonicalCommitStamp, History, DataBridge, or Outbox is a lane violation.

## Final Self-Review

- Authority coverage: every B-owned obligation maps to B0–B10; Restore has a dedicated package with a current repository authority mirror, and B4 StablePort authority is READY.
- Fifteen-operation coverage: every canonical operation appears in the matrix and B10 gate.
- Restore coverage: RST-B01 through RST-B12 each map to a named test and package.
- Boundary: no package performs Atomic Apply or creates SemanticGeneration/ChangeSet/commit-stamp/history output.
- A/C separation: A structural rules are consumed, not redesigned; C golden authoring remains deferred.
- Exact files: every planned path is either an existing repository path or explicitly marked Create in its package.
- Evidence: every package names a RED test, GREEN verification, oracle, and source-bound artifact.
- Placeholder scan: no unresolved implementation placeholder is present in this plan.
- Dependency check: B0 precedes all consumers; B1–B7 converge at B8; B9 and B10 are final verification packages.
- Authorization: P32 remains `NOT AUTHORIZED`.

**Plan status:** P30 implementation planning complete; P31 task packaging complete; implementation and P32 are not authorized by this document.

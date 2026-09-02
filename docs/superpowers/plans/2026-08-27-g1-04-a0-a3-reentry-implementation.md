# G1-04 A0–A3 Re-entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement only the newly unblocked `GT-G1-04-A` stateless path: encoding-neutral typed Operation → canonical normalization → envelope validation → payload structural validation.

**Architecture:** Extend the existing `runtime/semantic/` module without ObjectStore reads, idempotency, ApplyPlan, Atomic Apply, ChangeSet publication, golden expected outcomes, Scene, Skia, Arc, storage, sync, or platform dependencies. Human Current Authority remains normative; the three G1-04 machine projections are checked for drift and are never regenerated from production behavior.

**Tech Stack:** C++20, CMake/Ninja, GoogleTest/CTest, existing Protobuf Edition-2024 codec boundary, current YAML machine projections, Python stdlib verification tooling already used by the repository.

**Spec:** `docs/planning/GT_G1_04_A_CONTRACT_MATRIX.md`

## Global Constraints

- Current authority baseline is `main@17ff844f3b72e1976dee248550aa80f59ad38990` or a later main descendant that preserves the same Current Authority.
- `GT-G1-04-B`, `GT-G1-04-C`, and `GT-G1-05` are not authorized by this plan.
- `Operation` remains the only canonical mutation unit; do not introduce a canonical multi-operation Transaction.
- A rejected A0–A3 input must not mutate `ObjectStore`, indexes, `SemanticGeneration`, `ChangeSet`, History, Snapshot, or Replay state.
- `schema_version` and `payload_version` accept only explicit value `1`; missing, zero, and unknown values reject.
- If the current wire/DTO boundary cannot preserve version-field occurrence, stop with `WIRE_SCHEMA_CHANGE_REQUIRED`; do not default missing versions to `1` and do not modify Proto in this task.
- Operation-level keyed collections use the frozen canonical-set profile; leaf-owned `RichTextDelta.steps`, `VectorPathGeometry.commands`, vector samples, and dab dabs remain ordered sequences.
- ObjectKind V1 accepts only the released `kind_id + kind_version=1 + ObjectContent branch` triples; no reserved-range policy may be invented.
- Stable protocol error stage/path/category and reviewed positive/negative intent remain C-owned. A tests may assert acceptance/rejection and explicitly non-normative internal diagnostics; production output is not a golden oracle.
- The historical mixed Wave 3 in `docs/superpowers/plans/2026-08-26-g1-semantic-kernel-codex-package.md` is superseded **for GT-G1-04 execution only** by this plan. Other waves are not redefined here.

---

### Task 1: A0 Typed Operation Domain and Version-Presence Carrier

**Files:**
- Create: `runtime/semantic/include/canvas/semantic/document_id.hpp`
- Create: `runtime/semantic/include/canvas/semantic/operation_payload.hpp`
- Modify: `runtime/semantic/include/canvas/semantic/operation.hpp`
- Modify: `runtime/semantic/include/canvas/semantic/codec.hpp`
- Modify: `runtime/semantic/src/codec.cpp`
- Modify: `runtime/semantic/tests/semantic_types_test.cpp`
- Modify: `runtime/semantic/tests/codec_test.cpp`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: existing `OperationId`, `ObjectRecord`, leaf semantic types, and private generated Protobuf DTOs in `codec.cpp`.
- Produces: `DocumentId`, 15 typed payload structs, `OperationPayload`, `operationKind(const OperationPayload&)`, upgraded `Operation`, and `OperationFieldPresence` carried by `DecodedOperation`.

- [ ] **Step 1: Write failing strong-ID and typed-payload tests**

```cpp
TEST(SemanticTypes, OperationCarriesClosedTypedPayloadAndStrongDocumentId) {
    static_assert(!std::is_convertible_v<DocumentId, ObjectId>);
    static_assert(!std::is_convertible_v<ObjectId, DocumentId>);
    static_assert(std::variant_size_v<OperationPayload> == 15);

    Operation op{};
    op.payload = DeleteObjectsOp{{ObjectId::fromUint64(7)}};
    EXPECT_EQ(op.kind(), OperationKind::kDeleteObjects);
}
```

- [ ] **Step 2: Run the focused test and confirm RED**

```bash
cmake -S . -B out/g1-04-a -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCANVAS_BUILD_SEMANTIC=ON
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'semantic_types' --output-on-failure
```

Expected: compilation/test failure because `DocumentId`, `OperationPayload`, and the new `Operation` shape do not exist.

- [ ] **Step 3: Implement the minimal typed domain**

Use a strong `DocumentId` wrapper following the existing `OperationId` physical-representation pattern. Define the 15 payload structs from `operation.proto` and the closed variant:

```cpp
using OperationPayload = std::variant<
    InsertObjectsOp,
    DeleteObjectsOp,
    RestoreObjectsOp,
    SetPlacementsOp,
    SetTransformsOp,
    PatchPropertiesOp,
    SetObjectSizeOp,
    SetVectorPathGeometryOp,
    SetImageContentOp,
    AddStrokeOp,
    SplitStrokesOp,
    AddEraseMasksOp,
    RemoveEraseMasksOp,
    EditRichTextOp,
    SetConnectorContentOp>;

[[nodiscard]] OperationKind operationKind(const OperationPayload&) noexcept;

struct Operation final {
    OperationId id{};
    DocumentId document_id{};
    std::uint32_t schema_version = 0;
    std::uint32_t payload_version = 0;
    OperationPayload payload{};

    [[nodiscard]] OperationKind kind() const noexcept {
        return operationKind(payload);
    }
};
```

Do not keep an independently writable `OperationKind` member. Do not expose protobuf types in public semantic headers.

- [ ] **Step 4: Add explicit version-field occurrence to the codec result**

```cpp
struct OperationFieldPresence final {
    bool schema_version = false;
    bool payload_version = false;
};

struct DecodedOperation final {
    Operation operation{};
    OperationFieldPresence presence{};
    std::vector<CanonicalField> fields;
    SemanticError error = SemanticError::kNone;
    [[nodiscard]] bool ok() const noexcept { return error == SemanticError::kNone; }
};
```

Set the presence booleans from wire-preflight field occurrence before protobuf scalar-default erasure. If the current preflight cannot distinguish absent from explicit zero, stop and record `WIRE_SCHEMA_CHANGE_REQUIRED`; do not change `.proto`.

- [ ] **Step 5: Add presence tests and run GREEN**

Cover explicit `1`, missing, explicit `0`, and unknown nonzero version. Task 1 proves occurrence preservation only; Task 3 decides rejection.

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'semantic_types|codec' --output-on-failure
```

Expected: PASS, with missing and explicit-zero inputs distinguishable through `OperationFieldPresence`.

- [ ] **Step 6: Commit Task 1**

```bash
git add runtime/semantic/include/canvas/semantic/document_id.hpp \
        runtime/semantic/include/canvas/semantic/operation_payload.hpp \
        runtime/semantic/include/canvas/semantic/operation.hpp \
        runtime/semantic/include/canvas/semantic/codec.hpp \
        runtime/semantic/src/codec.cpp \
        runtime/semantic/tests/semantic_types_test.cpp \
        runtime/semantic/tests/codec_test.cpp \
        runtime/semantic/tests/CMakeLists.txt
git commit -m "feat(g1): add typed operation boundary for G1-04 A0"
```

---

### Task 2: A1 Canonical Operation Normalizer

**Files:**
- Create: `runtime/semantic/include/canvas/semantic/normalizer.hpp`
- Create: `runtime/semantic/src/normalizer.cpp`
- Create: `runtime/semantic/tests/normalization_test.cpp`
- Create: `tools/check_g1_04_a_projection_contract.py`
- Modify: `runtime/semantic/CMakeLists.txt`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: typed `Operation`, existing `normalizeFinite`, Field Registry behavior already encoded by current semantic types, and the frozen operation collection profile.
- Produces:

```cpp
struct NormalizeResult final {
    Operation value{};
    SemanticError error = SemanticError::kNone;
    [[nodiscard]] bool ok() const noexcept { return error == SemanticError::kNone; }
};

[[nodiscard]] NormalizeResult normalizeOperation(const Operation& input);
```

`SemanticError` here is an implementation diagnostic surface only; C must not use these values as reviewed golden outcomes unless separately frozen.

- [ ] **Step 1: Write RED normalization tests**

```cpp
TEST(OperationNormalizer, SortsDeleteIdsAndRejectsDuplicateIds) {
    Operation op = makeDeleteOperation({ObjectId::fromUint64(9), ObjectId::fromUint64(2)});
    auto normalized = normalizeOperation(op);
    ASSERT_TRUE(normalized.ok());
    const auto& ids = std::get<DeleteObjectsOp>(normalized.value.payload).object_ids;
    EXPECT_EQ(ids[0], ObjectId::fromUint64(2));
    EXPECT_EQ(ids[1], ObjectId::fromUint64(9));

    op = makeDeleteOperation({ObjectId::fromUint64(2), ObjectId::fromUint64(2)});
    EXPECT_FALSE(normalizeOperation(op).ok());
}
```

Also cover composite `(object_id, field_id)` order, nested split replacements, nested erase masks, cross-split replacement-id uniqueness, cross-item mask-id uniqueness, PropertyBag, persistent erase-mask ordering, `-0 → +0`, and leaf ordered-sequence preservation.

- [ ] **Step 2: Run RED**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'normalization' --output-on-failure
```

Expected: target/test absent or failing.

- [ ] **Step 3: Implement only frozen normalization**

Implement canonical-set normalization for exactly these 13 paths from `operation_structural_profile_v1.yaml`:

```text
insert_objects.objects                    key=id
delete_objects.object_ids                 key=object_id
restore_objects.objects                   key=id
set_placements.items                      key=object_id
set_transforms.items                      key=object_id
patch_properties.patches                  key=(object_id,field_id)
set_object_size.items                     key=object_id
split_strokes.splits                      key=source_stroke_id
split_strokes.splits.replacements         key=id
add_erase_masks.items                     key=object_id
add_erase_masks.items.masks               key=mask_id
remove_erase_masks.items                  key=object_id
remove_erase_masks.items.mask_ids         key=mask_id
```

The normalizer must not accept `ObjectStore`, `IndexedObjectStore`, `ObjectStoreMutator`, or commit-state types.

- [ ] **Step 4: Add the projection-drift guard**

`tools/check_g1_04_a_projection_contract.py` is verification-only and uses Python stdlib text/regex extraction; it must not become a production parser dependency. It reads:

```text
schema/axiom/v1/canonical/operation_structural_profile_v1.yaml
schema/axiom/v1/canonical/semantic_leaf_constraints_v1.yaml
schema/axiom/v1/registry/object_kind_registry_v1.yaml
```

and verifies the implementation-declared inventory has exactly the frozen paths/keys, both operation-wide uniqueness rules, ObjectKind ids `1..9` at version `1`, and the required VectorPath/RichText/Stroke leaf markers. If extraction cannot recognize the projection shape, the tool fails closed.

- [ ] **Step 5: Run GREEN plus drift guard**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'normalization|semantic' --output-on-failure
python3 tools/check_g1_04_a_projection_contract.py
python3 tools/check_runtime_boundaries.py
```

Expected: all commands exit 0.

- [ ] **Step 6: Commit Task 2**

```bash
git add runtime/semantic/include/canvas/semantic/normalizer.hpp \
        runtime/semantic/src/normalizer.cpp \
        runtime/semantic/tests/normalization_test.cpp \
        runtime/semantic/CMakeLists.txt runtime/semantic/tests/CMakeLists.txt \
        tools/check_g1_04_a_projection_contract.py
git commit -m "feat(g1): implement G1-04 A1 canonical normalization"
```

---

### Task 3: A2 Envelope Validator

**Files:**
- Create: `runtime/semantic/include/canvas/semantic/validator.hpp`
- Create: `runtime/semantic/src/validator.cpp`
- Create: `runtime/semantic/tests/envelope_validation_test.cpp`
- Modify: `runtime/semantic/CMakeLists.txt`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: canonical typed `Operation` plus `OperationFieldPresence`.
- Produces:

```cpp
enum class ValidationIssue : std::uint8_t {
    kNone = 0,
    kInvalidId,
    kUnsupportedVersion,
    kInvalidCollection,
    kInvalidObjectKind,
    kInvalidPropertyPatch,
    kInvalidLeaf,
};

// Non-normative implementation diagnostic. GT-G1-04-C owns any future
// stable protocol stage/path/category mapping.
struct ValidationResult final {
    ValidationIssue issue = ValidationIssue::kNone;
    [[nodiscard]] bool ok() const noexcept { return issue == ValidationIssue::kNone; }
};

[[nodiscard]] ValidationResult validateEnvelope(
    const Operation& operation,
    const OperationFieldPresence& presence) noexcept;

[[nodiscard]] ValidationResult validatePayloadStructure(const Operation& operation) noexcept;
```

Task 3 implements `validateEnvelope`; Task 4/5 implement `validatePayloadStructure`.

- [ ] **Step 1: Write the exact version matrix as RED tests**

```cpp
TEST(EnvelopeValidation, AcceptsOnlyExplicitVersionOne) {
    Operation op = makeMinimalOperation();
    op.schema_version = 1;
    op.payload_version = 1;
    EXPECT_TRUE(validateEnvelope(op, {.schema_version=true, .payload_version=true}).ok());

    EXPECT_FALSE(validateEnvelope(op, {.schema_version=false, .payload_version=true}).ok());
    EXPECT_FALSE(validateEnvelope(op, {.schema_version=true, .payload_version=false}).ok());

    op.schema_version = 0;
    EXPECT_FALSE(validateEnvelope(op, {.schema_version=true, .payload_version=true}).ok());
    op.schema_version = 2;
    EXPECT_FALSE(validateEnvelope(op, {.schema_version=true, .payload_version=true}).ok());
}
```

Add equivalent zero/unknown coverage for `payload_version`, plus zero OperationId and zero DocumentId.

- [ ] **Step 2: Run RED**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'envelope_validation' --output-on-failure
```

Expected: test absent/failing.

- [ ] **Step 3: Implement minimal A2 validation**

Accept only explicit version `1` with both presence bits true. Internal check ordering may be deterministic for debugging but must not be advertised as a C golden.

- [ ] **Step 4: Prove no state dependency and run GREEN**

`validator.hpp` must not include ObjectStore, ApplyPlan, ChangeSet, SemanticDocument, history, snapshot, or replay headers.

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'envelope_validation|semantic' --output-on-failure
python3 tools/check_runtime_boundaries.py
```

Expected: PASS.

- [ ] **Step 5: Commit Task 3**

```bash
git add runtime/semantic/include/canvas/semantic/validator.hpp \
        runtime/semantic/src/validator.cpp \
        runtime/semantic/tests/envelope_validation_test.cpp \
        runtime/semantic/CMakeLists.txt runtime/semantic/tests/CMakeLists.txt
git commit -m "feat(g1): implement G1-04 A2 envelope validation"
```

---

### Task 4: A3 Operation, ObjectKind, and PropertyPatch Structural Validation

**Files:**
- Modify: `runtime/semantic/src/validator.cpp`
- Create: `runtime/semantic/tests/payload_structure_test.cpp`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: normalized typed `Operation`, current ObjectKind registry, operation structural profile, and current Field Registry/value types.
- Produces: the non-stateful collection/ObjectKind/PropertyPatch portion of `validatePayloadStructure(const Operation&)`.

- [ ] **Step 1: Write RED tests for all 13 keyed collection paths**

For each frozen path, test empty reject, duplicate key reject, and canonical normalized order acceptance. Explicitly test duplicate replacement IDs across two `StrokeSplit`s and duplicate mask IDs across two erase-mask items.

- [ ] **Step 2: Write RED ObjectKind triple tests**

For released ids `1..9`, build a positive `(kind_id, version=1, active branch)` case. Add negatives for version 0, version 2, branch mismatch, invalid kind 0, and unknown kind 10. Do not claim `10..999` is reserved.

- [ ] **Step 3: Write RED PropertyPatch tests**

```text
SET   + value present  -> structurally allowed if FieldId/value branch is registry-valid
SET   + value absent   -> reject
CLEAR + value absent   -> structurally allowed
CLEAR + value present  -> reject
INVALID/unknown action -> reject
```

Field applicability to an existing target ObjectKind is B and must not be queried here.

- [ ] **Step 4: Run RED, implement stateless checks, then run GREEN**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'payload_structure' --output-on-failure
```

Expected before implementation: FAIL. Expected after minimal implementation: PASS.

- [ ] **Step 5: Add a state-dependency poison check**

Ensure the payload-structure test target links the semantic value/validator code without linking `reference_object_store.cpp`, `indexed_object_store.cpp`, or private mutation helpers. This proves A3 has no hidden store dependency.

- [ ] **Step 6: Commit Task 4**

```bash
git add runtime/semantic/src/validator.cpp \
        runtime/semantic/tests/payload_structure_test.cpp \
        runtime/semantic/tests/CMakeLists.txt
git commit -m "feat(g1): implement G1-04 A3 operation structure validation"
```

---

### Task 5: A3 Leaf Structural Validation and A-lane Evidence

**Files:**
- Modify: `runtime/semantic/src/validator.cpp`
- Create: `runtime/semantic/tests/leaf_structure_validation_test.cpp`
- Create: `runtime/semantic/tests/a_lane_no_mutation_test.cpp`
- Modify: `runtime/semantic/tests/CMakeLists.txt`
- Create after source commit: `verification/evidence/gates/G1/<SOURCE_SHA>/GT-G1-04-A/a0-a3-summary.json`
- Create after source commit: `verification/evidence/gates/G1/<SOURCE_SHA>/GT-G1-04-A/a0-a3-results.json`

**Interfaces:**
- Consumes: `semantic_leaf_constraints_v1.yaml` plus existing Image/Connector/Brush/RichText authority.
- Produces: completed A0–A3 stateless validator and commit-bound A-lane evidence only.

- [ ] **Step 1: Write RED VectorPath grammar cases**

Cover empty commands, first-not-MoveTo, multiple valid subpaths, drawing without active subpath, Close without open subpath, drawing after Close without new MoveTo, invalid command oneof, MoveTo-only subpath accepted, valid `NON_ZERO`/`EVEN_ODD`, unknown FillRule, and non-finite coordinates.

- [ ] **Step 2: Write RED RichText and Stroke cases**

RichText: only explicit delta version 1, non-empty ordered steps, exactly-one step branch. Stroke: at least one vector sample/dab, preserve sequence order, finite dab center/rotation, size `>0`, opacity `[0,1]`.

Do not test paragraph existence/current text range, target existence/current target kind, resource availability, connector connectability, or current mask/stroke state.

- [ ] **Step 3: Implement leaf checks and run focused GREEN**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'leaf_structure_validation|payload_structure|envelope_validation|normalization' --output-on-failure
```

Expected: PASS.

- [ ] **Step 4: Run the full semantic regression suite**

```bash
ctest --test-dir out/g1-04-a -R 'semantic|codec|object_store' --output-on-failure
python3 tools/check_g1_04_a_projection_contract.py
python3 tools/check_runtime_boundaries.py
python3 tools/check_docs.py
git diff --check
```

Expected: all commands exit 0; existing G1-01/02/02R/03 behavior remains unchanged.

- [ ] **Step 5: Prove the A lane does not mutate state**

The A public API receives no store parameter. In `a_lane_no_mutation_test.cpp`, keep an external sentinel store, snapshot canonical records before and after accepted/rejected A calls, and assert equality. This catches accidental global/private mutation without making ObjectStore part of the A interface.

- [ ] **Step 6: Commit the A0–A3 source and bind evidence to that SHA**

```bash
git add runtime/semantic tools/check_g1_04_a_projection_contract.py
git commit -m "feat(g1): complete G1-04 A0-A3 stateless validation"
SOURCE_SHA=$(git rev-parse HEAD)
```

Create evidence under exactly:

```text
verification/evidence/gates/G1/${SOURCE_SHA}/GT-G1-04-A/
```

`a0-a3-summary.json` records source SHA, authority baseline, three projection hashes, test commands/counts, `proto_change`, `stateful_B_implemented=false`, `verification_C_materialized=false`, `g1_05_authorized=false`, and first divergence if any. `a0-a3-results.json` records behavior coverage and accept/reject observations but does not claim C-owned stable stage/path/category.

- [ ] **Step 7: Commit only the evidence**

```bash
git add verification/evidence/gates/G1/${SOURCE_SHA}/GT-G1-04-A
git commit -m "evidence(g1): bind G1-04 A0-A3 validation"
```

- [ ] **Step 8: Stop at the A-lane review gate**

Do not create `idempotency.hpp`, `apply_plan.hpp`, stateful reference validation, reviewed golden corpus, or Atomic Apply. Request independent review of the A source SHA and evidence. Only later explicit authorization may open `GT-G1-04-B` or `GT-G1-04-C`.

---

## Plan Exit State

Successful execution may establish `GT-G1-04-A = implementation/validation candidate for Pass`, subject to independent review and commit-bound evidence. It cannot establish `GT-G1-04 overall Pass`, authorize `GT-G1-05`, or manufacture C-owned expected outcomes from production behavior.

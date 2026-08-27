# G1-04 A0–A3 Re-entry Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement only the newly unblocked `GT-G1-04-A` stateless path: encoding-neutral typed Operation → canonical normalization → envelope validation → payload structural validation.

**Architecture:** The implementation extends the existing `runtime/semantic/` module without introducing ObjectStore reads, idempotency, ApplyPlan, Atomic Apply, ChangeSet publication, golden expected outcomes, Scene, Skia, Arc, storage, sync, or platform dependencies. Human Current Authority remains normative; the three G1-04 machine projections are executable inputs and must be checked for drift rather than re-derived from production behavior.

**Tech Stack:** C++20, CMake/Ninja, GoogleTest/CTest, existing Protobuf Edition-2024 codec boundary, current YAML machine projections, existing Python verification tooling.

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
- Stable protocol error stage/path/category and reviewed positive/negative intent remain C-owned. A tests may assert acceptance/rejection and internal diagnostics, but production output is not a golden oracle.
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
- Consumes: existing `OperationId`, `ObjectRecord`, `ObjectContent`, placement/property/leaf semantic types, and private generated Protobuf DTOs in `codec.cpp`.
- Produces: `DocumentId`, the 15 closed typed payload structs, `OperationPayload`, `operationKind(const OperationPayload&)`, `Operation`, and `OperationFieldPresence` carried by `DecodedOperation`.

- [ ] **Step 1: Write failing strong-ID and typed-payload tests**

Add tests that require a distinct `DocumentId`, require exactly 15 variant alternatives, and make `OperationKind` derive from the active payload rather than from an independently writable member.

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

Run:
```bash
cmake -S . -B out/g1-04-a -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCANVAS_BUILD_SEMANTIC=ON
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'semantic_types' --output-on-failure
```
Expected: compilation/test failure because `DocumentId`, `OperationPayload`, and the new `Operation` shape do not exist.

- [ ] **Step 3: Implement the minimal typed domain**

Use a strong `DocumentId` wrapper with the same physical `Id128` representation pattern as `OperationId`. Define the 15 payload structs from `operation.proto` and a closed variant:

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

struct Operation final {
    OperationId id{};
    DocumentId document_id{};
    std::uint32_t schema_version = 0;
    std::uint32_t payload_version = 0;
    OperationPayload payload{};
    [[nodiscard]] OperationKind kind() const noexcept;
};
```

Do not add ObjectStore pointers, replay state, ApplyPlan fields, or protobuf types to public headers.

- [ ] **Step 4: Add explicit version-field occurrence to the codec result**

Extend the existing encoding-neutral decode seam:

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

In `codec.cpp`, set the two booleans from wire-preflight field occurrence, not from protobuf scalar values after decode. If the existing preflight path cannot distinguish absent from explicit zero, stop this task and record `WIRE_SCHEMA_CHANGE_REQUIRED`; do not change `.proto`.

- [ ] **Step 5: Write and run presence tests**

Cover four cases independently: explicit `1`, missing, explicit `0`, and unknown nonzero version. The Task 1 test only proves occurrence preservation; rejection is Task 3.

Run:
```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'semantic_types|codec' --output-on-failure
```
Expected: PASS with missing and explicit-zero inputs producing different `OperationFieldPresence` while retaining their scalar values.

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
- Modify: `runtime/semantic/CMakeLists.txt`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: typed `Operation` from Task 1, existing `normalizeFinite`, Field Registry behavior already encoded by current semantic types, and the frozen operation collection profile.
- Produces: `NormalizeResult normalizeOperation(const Operation&)` whose success value is a canonical copy and whose failure is an implementation diagnostic only, not a C-owned protocol oracle.

- [ ] **Step 1: Write RED normalization tests**

Require canonicalization of `-0`, key ordering, and rejection of duplicate keys without querying any store.

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

Also cover composite `(object_id, field_id)` ordering, nested split replacements, nested erase-mask collections, cross-split replacement-id uniqueness, cross-item mask-id uniqueness, PropertyBag sorting, persistent erase-mask sorting, and ordered-sequence preservation for RichText/VectorPath/stroke leaves.

- [ ] **Step 2: Run RED**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'normalization' --output-on-failure
```
Expected: target/test absent or failing.

- [ ] **Step 3: Implement only frozen normalization**

Implement canonical-set normalization for exactly the paths in `operation_structural_profile_v1.yaml`. Use unsigned ID lexicographic comparison and unsigned `field_id` for the composite patch key. Recursively call existing finite-number normalization. Do not normalize away or reorder leaf-owned sequences.

The normalizer must copy/return values; it must not accept `ObjectStore&`, `ObjectStore*`, `IndexedObjectStore`, `ObjectStoreMutator`, or any commit-state type.

- [ ] **Step 4: Add a projection-drift guard**

Add a verification test/tool that fails if the implementation’s declared collection inventory diverges from the three Current Authority projection files. It may compare stable path/key identifiers, but it must not generate expected semantic behavior from production output. No new YAML parser dependency is introduced in the production library.

Run:
```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'normalization|semantic' --output-on-failure
python3 tools/check_runtime_boundaries.py
```
Expected: PASS and no semantic header gains forbidden state/render/platform dependencies.

- [ ] **Step 5: Commit Task 2**

```bash
git add runtime/semantic/include/canvas/semantic/normalizer.hpp \
        runtime/semantic/src/normalizer.cpp \
        runtime/semantic/tests/normalization_test.cpp \
        runtime/semantic/CMakeLists.txt runtime/semantic/tests/CMakeLists.txt \
        tools/check_runtime_boundaries.py
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
- Consumes: canonical typed `Operation` plus `OperationFieldPresence` from Task 1.
- Produces: `ValidationResult validateEnvelope(const Operation&, const OperationFieldPresence&)` and later `validatePayloadStructure(const Operation&)` in Task 4. Diagnostics are explicitly implementation-local until C freezes reviewed outcomes.

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

- [ ] **Step 3: Implement the minimal validator**

Validation order inside A2 is deterministic for implementation purposes: ID carrier/presence/version checks. Do not publish that internal ordering as a stable C golden. Accept only `schema_version == 1 && payload_version == 1` with both presence bits true.

- [ ] **Step 4: Prove no state dependency**

Add compile/boundary assertions that `validator.hpp` does not include ObjectStore/ApplyPlan/ChangeSet/SemanticDocument headers. Run:

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
        runtime/semantic/CMakeLists.txt runtime/semantic/tests/CMakeLists.txt \
        tools/check_runtime_boundaries.py
git commit -m "feat(g1): implement G1-04 A2 envelope validation"
```

---

### Task 4: A3 Operation, ObjectKind, and PropertyPatch Structural Validation

**Files:**
- Modify: `runtime/semantic/include/canvas/semantic/validator.hpp`
- Modify: `runtime/semantic/src/validator.cpp`
- Create: `runtime/semantic/tests/payload_structure_test.cpp`
- Modify: `runtime/semantic/tests/CMakeLists.txt`

**Interfaces:**
- Consumes: normalized typed `Operation`, ObjectKind registry, operation structural profile, existing Field Registry/value types.
- Produces: `ValidationResult validatePayloadStructure(const Operation&)` that never reads current Document/ObjectStore state.

- [ ] **Step 1: Write RED tests for all 13 keyed collection paths**

For each profile path, test: empty reject, duplicate key reject, canonical normalized order accepted. Explicitly test operation-wide duplicate replacement IDs across two `StrokeSplit`s and operation-wide duplicate mask IDs across two erase-mask items.

- [ ] **Step 2: Write RED ObjectKind triple tests**

For each released kind `1..9`, construct one positive `(kind_id, version=1, active branch)` case. Add negatives for version 0, version 2, branch mismatch, invalid kind 0, and unknown kind 10. Do not add a test claiming `10..999` is reserved.

- [ ] **Step 3: Write RED PropertyPatch tests**

Require:

```text
SET   + value present  -> structurally allowed if FieldId/value branch is registry-valid
SET   + value absent   -> reject
CLEAR + value absent   -> structurally allowed
CLEAR + value present  -> reject
INVALID/unknown action -> reject
```

Do not query whether the field applies to an existing target ObjectKind; that is B.

- [ ] **Step 4: Run RED and implement only stateless checks**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'payload_structure' --output-on-failure
```
Expected before implementation: FAIL. After minimal implementation: PASS.

- [ ] **Step 5: Add an ObjectStore poison-boundary test**

Compile the A3 test target without linking `reference_object_store.cpp`, `indexed_object_store.cpp`, or `object_store_mutator.hpp`. The validator API must remain usable. This proves A3 has no accidental stateful dependency.

- [ ] **Step 6: Commit Task 4**

```bash
git add runtime/semantic/include/canvas/semantic/validator.hpp \
        runtime/semantic/src/validator.cpp \
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
- Consumes: the leaf rules already represented by `semantic_leaf_constraints_v1.yaml` plus existing Image/Connector/Brush/RichText authorities.
- Produces: completed A0–A3 validator and commit-bound evidence. It does **not** produce B/C artifacts.

- [ ] **Step 1: Write RED VectorPath grammar cases**

Cover: empty commands, first command not MoveTo, multiple valid subpaths, draw without active subpath, Close without open subpath, draw after Close without new MoveTo, command with invalid oneof shape, MoveTo-only subpath accepted, valid `NON_ZERO`/`EVEN_ODD`, unknown FillRule, and non-finite coordinates.

- [ ] **Step 2: Write RED RichText and Stroke cases**

RichText: only explicit delta version 1, non-empty ordered steps, exactly-one step branch. Stroke: at least one vector sample / dab, preserve sequence order, finite dab center/rotation, size `>0`, opacity in `[0,1]`.

Do not test paragraph existence/current text range, target existence/current target kind, resource availability, connector connectability, or current mask/stroke state; those require B/later state.

- [ ] **Step 3: Implement leaf checks and run focused GREEN**

```bash
cmake --build out/g1-04-a
ctest --test-dir out/g1-04-a -R 'leaf_structure_validation|payload_structure|envelope_validation|normalization' --output-on-failure
```
Expected: PASS.

- [ ] **Step 4: Run the full semantic regression suite**

```bash
ctest --test-dir out/g1-04-a -R 'semantic|codec|object_store' --output-on-failure
python3 tools/check_runtime_boundaries.py
python3 tools/check_docs.py
git diff --check
```
Expected: all commands exit 0. Existing G1-01/02/02R/03 behavior remains unchanged.

- [ ] **Step 5: Prove the A lane does not mutate state**

`a_lane_no_mutation_test.cpp` should construct a store before calling the public A pipeline, snapshot its canonical records, run accepted and rejected A inputs, and assert the store snapshot is unchanged. The A API itself must receive no store parameter; the store exists only in the test as an external sentinel.

- [ ] **Step 6: Commit the A0–A3 source and bind evidence to that SHA**

First commit source/tests:
```bash
git add runtime/semantic runtime/semantic/tests tools/check_runtime_boundaries.py
git commit -m "feat(g1): complete G1-04 A0-A3 stateless validation"
SOURCE_SHA=$(git rev-parse HEAD)
```

Then create evidence under exactly:
```text
verification/evidence/gates/G1/${SOURCE_SHA}/GT-G1-04-A/
```

`a0-a3-summary.json` must record:
- source SHA and current authority baseline;
- hashes of the three G1-04 machine projections;
- test commands and pass/fail counts;
- `proto_change: NONE` unless the earlier presence stop condition caused an explicit halt instead;
- `stateful_B_implemented: false`;
- `verification_C_materialized: false`;
- `g1_05_authorized: false`;
- first divergence, if any.

`a0-a3-results.json` records behavior coverage counts and acceptance/rejection observations, but does not claim C-owned stable stage/path/category.

- [ ] **Step 7: Commit only the evidence**

```bash
git add verification/evidence/gates/G1/${SOURCE_SHA}/GT-G1-04-A
git commit -m "evidence(g1): bind G1-04 A0-A3 validation"
```

- [ ] **Step 8: Stop at the A-lane human/review gate**

After evidence is committed, do not start `idempotency.hpp`, `apply_plan.hpp`, stateful reference validation, reviewed golden corpus, or Atomic Apply. Request an independent review of the A source SHA and evidence. Only a later explicit authorization may open `GT-G1-04-B` or `GT-G1-04-C`.

---

## Plan Exit State

Successful execution of this plan may establish `GT-G1-04-A = implementation/validation candidate for Pass`, subject to independent review and commit-bound evidence. It cannot establish `GT-G1-04 overall Pass`, cannot authorize `GT-G1-05`, and cannot manufacture C-owned expected outcomes from production behavior.

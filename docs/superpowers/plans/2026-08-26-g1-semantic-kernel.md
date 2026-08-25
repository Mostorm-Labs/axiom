# G1 Semantic Kernel Implementation Plan / Codex Package

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Every behavior change follows RED → GREEN → refactor, every Gate Task gets its own review checkpoint, and no task may be marked Pass without commit-bound Evidence.

**Goal:** Build the first production Axiom Semantic Kernel: current V1 semantic authority → executable machine contract → canonical C++ types → strict codec → object stores → normalize/validate/idempotency/ApplyPlan → atomic SemanticDocument apply + ChangeSet → deterministic snapshot/projection/digest/replay → Semantic Replay Inspector → G1 Gate Evidence.

**Architecture:** Create a production `runtime/semantic/` module that depends on `runtime/foundation/` and is forbidden from depending on `runtime/scene/`, Skia, Arc, platform surfaces, storage/sync, network, or renderer state. Notion remains the living design authority; G1 promotes only the machine-readable artifacts needed for executable implementation into GitHub. The closed `docs/notion-bridge-bootstrap` branch is reference material only: artifacts may be reused only after comparison with the current Notion authority and must never be wholesale merged or treated as source of truth.

**Tech Stack:** C++20, CMake 3.30+, CTest, Edition 2024 protobuf/Protobuf toolchain as frozen by current semantic authority, YAML/JSON registry artifacts, Python/Node verification tooling already present in `verification/`, SHA-256 evidence binding.

**Spec / Authority Inputs:**
- `docs/planning/GATE_TASK_TRACKER.md` — normative Gate Task identities, dependencies, status and Evidence paths.
- GitHub ADRs: ADR-0001, ADR-0003, ADR-0014, ADR-0016, ADR-0019, ADR-0020, ADR-0025.
- Notion: `G1 Semantic Kernel Implementation Plan v0.1`.
- Notion: `Axiom Semantic Schema V1 Release Candidate Final Gate v0.1`.
- Notion: `Schema Freeze Review + V1 Release Candidate Gate v0.1`.
- Notion: `Axiom Semantic Schema Spec + IDL v0.1`.
- Notion: `Axiom Reference IDL + Codec Mapping v0.1`.
- Notion: `Generated Proto Baseline + Canonical Codec Freeze v0.1`.
- Notion: `Canonical Codec Differential Runner + Descriptor Lock v0.1`.
- Notion: `Canonical Codec Binary Golden Corpus v0.1 — Operation / Object / Snapshot Expansion`.
- Notion: current Field/ShapeKind/BrushFamily/Operation registries, Common Wire Rules, OrderKey authority, RichText/Stroke/Connector leaf authorities.

Do not place private Notion URLs/page IDs in repository files. Reference authority by stable document title and record the retrieval timestamp in task Evidence.

---

## 0. Reconciliation Corrections Before Execution

These corrections are mandatory because the current `main` differs from the older G1 plan assumptions.

### RC-G1-01 — G0 does not contain product Semantic acceptance artifacts

Current `main` has `verification/corpus/semantic/README.md`, but no production semantic corpus. `verification/tools/generate_semantic_bootstrap.py` explicitly records:

```text
NOT_G1_SEMANTIC_ACCEPTANCE
NO_PRODUCT_SEMANTIC_KERNEL
NO_60_CASE_SEMANTIC_CORPUS
```

Therefore G1 must formally promote the current Notion V1 semantic machine contract and golden corpus into GitHub as executable Gate artifacts. Do not pretend they were already accepted by G0.

### RC-G1-02 — only compatible foundation primitives may be reused

Current `runtime/foundation/` contains useful production primitives, but not all of them match the semantic wire authority:

- `canvas::foundation::ObjectId`: reusable candidate; it is 16 bytes and supports zero detection.
- `canvas::foundation::Result<T>`: reusable container pattern, but the current `ErrorCode` enum is not the semantic codec/validation taxonomy.
- `SceneRevision` / `ContentRevision`: do not silently reinterpret as SemanticDocument revision; add a semantic-specific revision type or alias only after authority alignment.
- `canvas::foundation::StableOrderKey`: **not** canonical semantic OrderKey; it currently stores `uint64_t`, while V1 semantic OrderKey is opaque 1..32 bytes ordered unsigned-lexicographically with ObjectId tiebreak.
- `WorldPoint` / `WorldRect`: **not** durable semantic geometry; they currently use `float`, while durable canonical semantic numbers are binary64/f64, finite, with `-0 → +0` normalization.

### RC-G1-03 — POC-01 is an oracle input, never the production model

`pocs/shared_engine` may supply behavior tests, replay ideas and differential reference data, but it uses experimental `uint64_t` IDs, integer order, `float` geometry and only a subset of object kinds. No production semantic header may include or depend on POC headers.

### RC-G1-04 — the closed migration branch is reference-only

`docs/notion-bridge-bootstrap` contains candidate 12-file proto, registry/profile artifacts, descriptor lock and semantic golden files. For each artifact promoted in G1:

1. re-read current Notion authority;
2. compare candidate content against current authority;
3. reuse only content that remains byte/semantic equivalent;
4. regenerate tool-derived artifacts from the promoted source;
5. record provenance in Gate Evidence;
6. never copy `docs/notion/**` or private Notion identifiers into `main`.

---

## 1. Global Constraints

- G1 has exactly eight Gate Tasks: `GT-G1-01..GT-G1-08`. This plan introduces no new Gate Task IDs.
- Promotion order remains `G0 Pass → G1 → G2`; G1 cannot claim G2 RuntimeScene/SceneCompiler correctness.
- All canonical mutation is Operation-only. There is no global canonical Transaction.
- V1 has 9 ObjectKinds and 15 canonical Operations exactly as classified by the current V1 Semantic Schema RC authority.
- Validate before mutate. Any reject leaves semantic state, revision, indexes and ChangeSet observably unchanged.
- Unknown semantic kind/enum/operation fails closed. Unknown-field/version behavior follows Common Wire Rules, not protobuf library defaults.
- Durable canonical numeric values are finite binary64/f64; canonicalization includes `-0 → +0` where required.
- Semantic OrderKey is opaque 1..32 bytes; sibling total order is `(OrderKey unsigned lexicographic, ObjectId)`.
- Single-object lookup in the production store must not perform an O(N) object scan.
- `runtime/semantic/include/**` must remain free of Scene, Skia, Arc, platform, storage/sync, networking and public product ABI dependencies.
- Snapshot restore reconstructs canonical state and does not synthesize user-edit Operations.
- `SemanticChangeSet` is a semantic delta/hint contract for downstream G2; it must not contain RuntimeScene, GPU or renderer pointers.
- Historical POC failures/evidence remain historical; never overwrite them with G1 results.
- Every task writes Evidence under `verification/evidence/gates/G1/<commit>/GT-G1-XX/` and updates the tracker only after real validation.

---

## 2. Codex Model / Reasoning Classes

Model names change over time; use capability classes rather than a hard-coded SKU.

| Class | Execution model | Reasoning | Context | Review policy |
| --- | --- | --- | --- | --- |
| `XL` | Largest available Codex coding/reasoning model | High or Very High | Fresh full-repo context + named Notion sources | Fresh independent reviewer required |
| `L` | Strong general coding model | Medium-High | Full relevant module + authority context | Independent reviewer recommended |
| `M` | Standard coding model | Medium | Frozen interfaces + focused module context | Normal code review |

### Task allocation

| Task | Complexity | Execution | Reasoning | Review | Why |
| --- | ---: | --- | --- | --- | --- |
| GT-G1-01 | 5/5 | `XL` | High | `XL` fresh review | Establishes machine contract + canonical C++ boundary; errors fan out to every later Gate |
| GT-G1-02 | 5/5 | `XL` | High | `XL` fresh review | Wire/codec/error/canonical-byte boundary and descriptor lock |
| GT-G1-03 | 4/5 | `L` | Medium-High | `L` | Differential oracle + production indexing, but semantics are already frozen |
| GT-G1-04 | 5/5 critical | `XL` | **Very High** | `XL` fresh review | Correctness center: normalize, validation order, idempotency, ApplyPlan, fail-closed behavior |
| GT-G1-05 | 5/5 critical | `XL` | **Very High** | `XL` fresh review | Atomic state transition + revision + ChangeSet; direct G2 input contract |
| GT-G1-06 | 5/5 | `XL` | High | `XL` fresh review | Determinism, canonical projection, snapshot/replay equivalence, digest stability |
| GT-G1-07 | 3/5 | `M` | Medium | `L` | Composition/observability over already-frozen APIs; must not invent semantics |
| GT-G1-08 | 4/5 | `L` | Medium-High | `XL` Gate review | Cross-task Evidence, hashes, lineage, Gate verdict |

Do not execute GT-G1-04 or GT-G1-05 with a small/fast model. If a task grows beyond one coherent review unit, split implementation commits inside the same Gate Task rather than inventing new Gate IDs.

---

## 3. Wave / Dependency Plan

```text
Wave 1    GT-G1-01
             │
Wave 2    ┌──┴─────────┐
          │            │
       GT-G1-02     GT-G1-03     (parallel worktrees allowed after G1-01 merges)
          │            │
          └─────┬──────┘
                │
Wave 3       GT-G1-04
                │
Wave 4       GT-G1-05
                │
Wave 5       GT-G1-06
                │
Wave 6       GT-G1-07
                │
Wave 7       GT-G1-08
                │
              G1 PASS
```

Each Wave starts from the latest accepted `main`. Do not stack long-lived unreviewed branches across Waves.

---

# Wave 1 — GT-G1-01: Semantic Target + Canonical Type Boundary

**Recommended model:** `XL`, High reasoning, fresh `XL` reviewer.

**Authority Inputs:** G1 plan; V1 Semantic Schema RC Final Gate; Semantic Schema Spec + IDL; Reference IDL + Codec Mapping; Field/ShapeKind/BrushFamily/Operation registries; ADR-0001/0003/0016/0019/0025.

**Goal:** Promote the current V1 machine-readable semantic source required for implementation and establish a production C++ semantic type boundary with no Scene/render/platform leakage.

### Files

**Create/promote after current-authority comparison:**
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/common.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/geometry.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/property.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/object.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/operation.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/paint.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/brush_stroke.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/connector.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/erase_mask.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/rich_text.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/snapshot.proto`
- `schema/axiom/v1/proto/auditoryworks/axiom/v1/vector_path.proto`
- `schema/axiom/v1/registry/field_registry_v1.yaml`
- `schema/axiom/v1/registry/shape_kind_registry_v1.yaml`
- `schema/axiom/v1/registry/brush_family_registry_v1.yaml`
- `schema/axiom/v1/registry/operation_registry_v1.yaml`
- `schema/axiom/v1/canonical/canonical_profile_v1.yaml`
- `schema/axiom/v1/canonical/protocol_hard_limits_v1.yaml`
- `schema/axiom/v1/README.md` — executable-contract ownership/provenance, no private URLs.

**Create production runtime:**
- `runtime/semantic/CMakeLists.txt`
- `runtime/semantic/include/canvas/semantic/canonical_numeric.hpp`
- `runtime/semantic/include/canvas/semantic/order_key.hpp`
- `runtime/semantic/include/canvas/semantic/object_record.hpp`
- `runtime/semantic/include/canvas/semantic/operation.hpp`
- `runtime/semantic/include/canvas/semantic/change_set.hpp`
- `runtime/semantic/include/canvas/semantic/semantic_revision.hpp`
- `runtime/semantic/tests/CMakeLists.txt`
- `runtime/semantic/tests/semantic_types_test.cpp`

**Modify:**
- `CMakeLists.txt`
- `tools/check_runtime_boundaries.py`

### Interfaces

The public semantic headers must provide encoding-neutral C++ representations of the current V1 authority. Wire-generated protobuf types do not escape the codec layer.

Required invariants exposed/testable at this boundary:

```text
ObjectId       = 16 opaque bytes; all-zero invalid at semantic validation boundary
OrderKey       = 1..32 opaque bytes; unsigned lexicographic comparison
Canonical f64  = finite; -0 normalized to +0 at canonical normalization boundary
Operation      = one of exactly 15 V1 canonical operation payloads
ObjectRecord   = one of exactly 9 V1 ObjectKinds with authority-defined content/property boundary
ChangeSet      = semantic IDs/hints only; no RuntimeScene/Skia/platform types
```

### TDD Steps

- [ ] **Step 1 — Authority preflight.** Re-read every named authority input. Compare the reference branch’s 12 proto + six registry/profile files to current Notion. Record `REUSE`, `REGENERATE`, or `REJECT` per artifact in `verification/evidence/gates/G1/<commit>/GT-G1-01/authority-reconciliation.json`.
- [ ] **Step 2 — Write RED boundary tests.** `semantic_types_test.cpp` must compile/use ObjectRecord, Operation, OrderKey and SemanticChangeSet and assert zero-ID detection, 1/32-byte OrderKey acceptance, 0/33-byte rejection, unsigned lexicographic ordering and canonical negative-zero normalization.
- [ ] **Step 3 — Write RED dependency test.** Extend `tools/check_runtime_boundaries.py` so `runtime/semantic/include` is scanned and rejects includes/symbols from `runtime/scene`, Skia, Windows, Apple, Android/JNI, Emscripten, network/storage/thread/callback and public product ABI.
- [ ] **Step 4 — Run RED.** Configure with `CANVAS_BUILD_SEMANTIC=ON`; expected failure is missing semantic target/types, not unrelated G0 failure.
- [ ] **Step 5 — Promote verified machine source.** Add only current-authority-equivalent proto/registry/profile source. Do not import `docs/notion/**`, descriptor binaries or generated code in this step.
- [ ] **Step 6 — Implement minimal semantic headers.** Reuse `canvas::foundation::ObjectId` only if tests prove its byte representation matches authority. Do not reuse foundation `StableOrderKey` or float `WorldPoint/WorldRect` as canonical semantic types.
- [ ] **Step 7 — Fix root CMake topology.** Add `CANVAS_BUILD_SEMANTIC`; ensure `runtime/foundation` is added exactly once when Semantic and/or RF01 Scene are enabled; add `runtime/semantic` independently of Scene.
- [ ] **Step 8 — Run GREEN.** Build semantic tests with warnings-as-errors and run boundary checker.
- [ ] **Step 9 — Evidence.** Save authority reconciliation, build/test command manifest and source hashes under the GT-G1-01 Evidence directory.

### Mock / Oracle
- Current Notion semantic authority is the design oracle.
- Reference migration branch is comparison input only.
- Compile-time dependency checker is the module-boundary oracle.
- No POC runtime type is an oracle for field layout.

### Exit Criteria
- 12 proto sources + six registry/profile sources promoted only after authority comparison.
- `runtime/semantic` builds without Scene/Skia/platform/storage/sync dependencies.
- Canonical numeric, ID and OrderKey tests pass.
- Public semantic headers are self-contained and warnings-clean.
- GT-G1-01 Evidence is commit-bound and reproducible.

---

# Wave 2A — GT-G1-02: Codec Boundary + Strict Decode Result

**Recommended model:** `XL`, High reasoning, fresh `XL` reviewer.

**Authority Inputs:** GT-G1-01 promoted schema; Reference IDL + Codec Mapping; Common Wire Rules; Canonical Codec Freeze; Descriptor Lock; Binary Golden Corpus; V1 Schema RC Final Gate.

### Files

**Create:**
- `runtime/semantic/include/canvas/semantic/codec.hpp`
- `runtime/semantic/include/canvas/semantic/semantic_error.hpp`
- `runtime/semantic/src/codec.cpp`
- `runtime/semantic/tests/codec_test.cpp`
- `runtime/semantic/tests/codec_negative_test.cpp`
- `schema/axiom/v1/descriptor/descriptor.lock.pb`
- `schema/axiom/v1/descriptor/descriptor.lock.sha256`
- `schema/axiom/v1/toolchain/toolchain.lock.json`
- `verification/corpus/semantic/v1/corpus.json`
- `verification/corpus/semantic/v1/suites/seed-v0.1.json`
- `verification/corpus/semantic/v1/wire/` — materialized BG/BGX and release binary vectors required by current authority.
- `verification/tools/validate_semantic_contract.py`

**Modify:**
- `runtime/semantic/CMakeLists.txt`
- `verification/corpus/semantic/README.md`
- verification workspace validation/tests as needed to make semantic corpus first-class.

### Interfaces

```cpp
namespace canvas::semantic {

enum class SemanticErrorCode {
  // Populate exactly from current Common Wire / codec / validation authority.
  // Do not reuse scene-oriented foundation ErrorCode as the wire taxonomy.
};

struct DecodeOperationResult {
  // success: canonical Operation
  // failure: stable SemanticError with stage/path/code; no Document mutation
};

DecodeOperationResult DecodeOperation(std::span<const std::byte> bytes);
SemanticResult<std::vector<std::byte>> EncodeCanonicalOperation(const Operation& operation);

} // namespace canvas::semantic
```

The exact error enum is generated/copied from current authority; do not invent names from this plan.

### TDD Steps

- [ ] Write RED tests for valid seed decode, truncated wire, malformed tags, unsupported semantic version, unknown kind/enum/operation, duplicate canonical keys, non-finite numbers, hard-limit violation and canonical `-0 → +0` behavior.
- [ ] Add RED round-trip assertions: `decode(BG) → semantic → canonical encode == expected BG` for cases classified same-semantic/same-bytes by authority.
- [ ] Compile promoted Edition 2024 proto with the pinned toolchain from authority and generate `descriptor.lock.pb` from source; never copy a stale descriptor binary blindly.
- [ ] Materialize the immutable 60-case seed identity and release-specific suites from current authority. Assert exactly 60 IDs in `seed-v0.1`; additional cases belong to independent suites.
- [ ] Implement codec as the only protobuf↔encoding-neutral semantic translation layer. Generated protobuf types must not appear in `object_record.hpp`, `operation.hpp`, ObjectStore, SemanticDocument or G2-facing headers.
- [ ] Implement explicit semantic unknown-field/version policy rather than relying on protobuf unknown-field preservation.
- [ ] Run descriptor fingerprint, corpus identity, positive/negative codec and canonical re-encode tests.
- [ ] Run `verification/tools/validate_semantic_contract.py` twice from clean output and assert reproducible descriptor/corpus hashes.

### Mock / Oracle
- Current Notion Common Wire + Reference IDL are normative.
- Materialized BG/BGX corpus is byte oracle.
- Descriptor lock is structural proto oracle, not complete semantic-contract identity.
- POC-01 replay bytes are historical comparison only.

### Exit Criteria
- Real Edition 2024 proto compiles with pinned toolchain.
- Descriptor lock is reproducible from checked-in proto source.
- 60-case seed identity is immutable and machine-checked.
- Positive/negative codec corpus passes; fail-closed taxonomy is stable.
- Same-semantic→same-bytes cases match binary golden exactly.
- No codec decode mutates SemanticDocument.

---

# Wave 2B — GT-G1-03: ReferenceObjectStore + IndexedObjectStore

**Recommended model:** `L`, Medium-High reasoning, independent `L` review.

**Authority Inputs:** GT-G1-01 canonical types; Product Object Model; ObjectContent/Placement/OrderKey authorities; ADR-0003/0016/0019.

### Files

**Create:**
- `runtime/semantic/include/canvas/semantic/object_store.hpp`
- `runtime/semantic/include/canvas/semantic/reference_object_store.hpp`
- `runtime/semantic/include/canvas/semantic/indexed_object_store.hpp`
- `runtime/semantic/src/reference_object_store.cpp`
- `runtime/semantic/src/indexed_object_store.cpp`
- `runtime/semantic/tests/object_store_test.cpp`
- `runtime/semantic/tests/object_store_differential_test.cpp`
- `runtime/semantic/tests/object_store_lookup_instrumentation_test.cpp`

**Modify:**
- `runtime/semantic/CMakeLists.txt`

### Interfaces

```cpp
class ObjectStore {
 public:
  virtual ~ObjectStore() = default;
  virtual const ObjectRecord* Find(const foundation::ObjectId& id) const = 0;
  virtual SemanticResult<void> Insert(ObjectRecord record) = 0;
  virtual SemanticResult<void> Replace(ObjectRecord record) = 0;
  virtual SemanticResult<void> Erase(const foundation::ObjectId& id) = 0;
  virtual std::vector<foundation::ObjectId> CanonicalObjectIds() const = 0;
};
```

If the repository Result template cannot express `void`, add an authority-neutral status/result specialization in the semantic module; do not change unrelated scene error semantics just for convenience.

### TDD Steps

- [ ] RED: deterministic insert/find/replace/erase tests for ReferenceObjectStore.
- [ ] RED: same sequence against IndexedObjectStore must produce equal canonical object projections.
- [ ] RED: 10K deterministic mutation trace, compare stores after every 100 mutations.
- [ ] RED: instrument production Find/single-record mutation; assert object full-scan count remains zero.
- [ ] Implement Reference store first with intentionally simple correctness-first storage.
- [ ] Implement Indexed store with ObjectId lookup and only the authority-required secondary indexes (parent/sibling/order/reference indexes). Do not pre-build Scene/Spatial indexes.
- [ ] Add index consistency invariant checks available in Debug/tests.
- [ ] Run 1K/10K/100K non-SLO baseline and record counters/timings as observational Evidence; G1 acceptance is correctness + no full scan, not a guessed Product latency SLO.

### Mock / Oracle
- `ReferenceObjectStore` is the permanent differential correctness oracle.
- Canonical projection from GT-G1-06 will become the stronger external equality oracle; until then use deterministic canonical record enumeration.

### Exit Criteria
- Reference and Indexed stores agree for deterministic differential traces.
- Production ID lookup and single-record mutation perform zero O(N) object scans by instrumentation.
- Index invariants remain valid after insert/replace/erase.
- No Scene/renderer-specific index exists in semantic storage.

---

# Wave 3 — GT-G1-04: Normalize + Validation + Idempotency + ApplyPlan

**Recommended model:** `XL`, **Very High** reasoning, mandatory fresh `XL` correctness review.

**Authority Inputs:** Operation-only mutation baseline; Operation Payload + Validation Rules; Field Registry; Object/Placement/Connector/RichText/Stroke/Erase authorities; Common Wire; ADR-0014/0016/0019/0025.

**Tracker reconciliation:** the older direct plan omits Normalize and Idempotency. This task must include both; do not simplify it back to only `ValidateAndPrepare`.

### Files

**Create:**
- `runtime/semantic/include/canvas/semantic/normalizer.hpp`
- `runtime/semantic/include/canvas/semantic/validator.hpp`
- `runtime/semantic/include/canvas/semantic/idempotency.hpp`
- `runtime/semantic/include/canvas/semantic/apply_plan.hpp`
- `runtime/semantic/src/normalizer.cpp`
- `runtime/semantic/src/validator.cpp`
- `runtime/semantic/src/idempotency.cpp`
- `runtime/semantic/src/apply_plan.cpp`
- `runtime/semantic/tests/normalization_test.cpp`
- `runtime/semantic/tests/validation_test.cpp`
- `runtime/semantic/tests/idempotency_test.cpp`
- `runtime/semantic/tests/apply_plan_test.cpp`
- `runtime/semantic/tests/validation_no_mutation_test.cpp`

### Required pipeline

```text
Decoded/typed Operation
  → Normalize
  → Envelope Validation
  → Payload Validation
  → Reference Validation
  → Kind Validation
  → Invariant Validation
  → Idempotency Classification
  → Prepare ApplyPlan
```

`ApplyPlan` contains resolved targets/final values sufficient for atomic commit; validation/preparation must not mutate stores, revision, indexes or history.

### TDD Steps

- [ ] RED normalization cases: negative zero, canonical collection ordering/classification, duplicate-key rejection, authority-defined normalization only.
- [ ] RED envelope/payload cases for all 15 Operations.
- [ ] RED reference/kind/invariant cases: zero/missing IDs, duplicate create, wrong content kind, invalid FieldId/value type, parent/placement cycle, invalid connector target/anchor, invalid stroke/mask target, rich-text boundary violations and protocol hard limits.
- [ ] RED idempotency cases using the authority-defined Operation identity semantics. Same accepted Operation replay must not create a second semantic mutation/revision advance.
- [ ] RED `validation_no_mutation`: capture canonical store state/revision/index counters before each reject and assert byte-for-byte/equivalent state after reject.
- [ ] Implement minimal normalization stage with no policy beyond authority.
- [ ] Implement stage-specific validation errors with deterministic first failure ordering.
- [ ] Implement idempotency ledger/classification scoped to SemanticDocument semantics; do not add server/sync acknowledgement state.
- [ ] Implement ApplyPlan resolution and assert it is immutable/non-owning only where lifetime is safe; no partial write during prepare.
- [ ] Run full seed + release negative corpus through Decode→Normalize→Validate→Prepare.

### Mock / Oracle
- Authority validation tables are normative.
- ReferenceObjectStore supplies simple state lookup oracle.
- Negative golden corpus supplies error-stage/path/code oracle.

### Exit Criteria
- Every V1 Operation has positive and negative coverage.
- Validation stage order and first failure are deterministic.
- Duplicate/idempotent replay behavior matches authority.
- Every rejection leaves state/revision/indexes unchanged.
- ApplyPlan fully resolves commit inputs without mutation.

---

# Wave 4 — GT-G1-05: SemanticDocument Atomic Apply + ChangeSet

**Recommended model:** `XL`, **Very High** reasoning, mandatory fresh `XL` correctness review.

**Authority Inputs:** GT-G1-04 pipeline; ADR-0003/0014/0019/0025; Runtime Data Flow canonical commit pipeline; ChangeSet ownership rules.

### Files

**Create:**
- `runtime/semantic/include/canvas/semantic/document.hpp`
- `runtime/semantic/src/document.cpp`
- `runtime/semantic/tests/document_apply_test.cpp`
- `runtime/semantic/tests/document_atomicity_test.cpp`
- `runtime/semantic/tests/change_set_test.cpp`
- `runtime/semantic/tests/reference_indexed_document_differential_test.cpp`

**Modify:**
- `runtime/semantic/CMakeLists.txt`

### Interfaces

```cpp
class SemanticDocument {
 public:
  SemanticApplyResult Apply(const Operation& operation);
  SemanticRevision revision() const noexcept;
  const ObjectStore& objects() const noexcept;
};

struct SemanticApplyResult {
  // APPLIED: exactly one revision advance + ChangeSet
  // IDEMPOTENT_NOOP: no revision advance + explicit no-op classification
  // REJECTED: no state/revision/index mutation + SemanticError
};
```

Exact naming may follow current repository naming conventions, but the three observable outcomes above are mandatory and must not be conflated.

### TDD Steps

- [ ] RED apply tests cover all 15 operation families from the release corpus.
- [ ] RED atomicity tests inject commit-time store/index failures at deterministic seams; assert rollback/no visibility, no revision advance and no partial ChangeSet.
- [ ] RED idempotent replay: no second mutation/revision.
- [ ] RED ChangeSet tests verify added/removed/modified IDs and old/new semantic invalidation hints required by G2, with no RuntimeScene/GPU pointers.
- [ ] Implement commit of an already prepared ApplyPlan; revision advances exactly once after all semantic/index mutations are committed.
- [ ] Ensure failure is all-or-nothing. If rollback is required internally, it must be completed before returning failure; no observer can see partial state.
- [ ] Run the same operation streams against ReferenceObjectStore-backed and IndexedObjectStore-backed documents; canonical results must match.
- [ ] Extend module-boundary tests so `document.hpp` cannot include Scene/platform/storage/sync concepts.

### Mock / Oracle
- Reference-store-backed SemanticDocument is the differential oracle.
- Failure-injection store/index seam is the atomicity oracle.
- ChangeSet tests are the contract oracle for G2 consumption.

### Exit Criteria
- All 15 operation families apply correctly.
- Success advances revision exactly once; reject/idempotent no-op does not.
- Commit-time injected failures leave canonical state unchanged.
- Reference and Indexed document variants converge to identical state.
- ChangeSet is complete enough for G2 but contains no downstream runtime object.

---

# Wave 5 — GT-G1-06: Snapshot + Canonical Projection + Digest + Replay

**Recommended model:** `XL`, High reasoning, fresh `XL` determinism review.

**Authority Inputs:** Snapshot authority; Canonical Projection schema; Common Wire/canonical ordering; ADR-0016/0020; canonical binary golden corpus.

### Files

**Create:**
- `runtime/semantic/include/canvas/semantic/snapshot.hpp`
- `runtime/semantic/include/canvas/semantic/projection.hpp`
- `runtime/semantic/include/canvas/semantic/digest.hpp`
- `runtime/semantic/src/snapshot.cpp`
- `runtime/semantic/src/projection.cpp`
- `runtime/semantic/src/digest.cpp`
- `runtime/semantic/tests/snapshot_test.cpp`
- `runtime/semantic/tests/projection_test.cpp`
- `runtime/semantic/tests/digest_test.cpp`
- `runtime/semantic/tests/replay_conformance_test.cpp`
- `verification/tools/run_g1_semantic_differential.py`

### Interfaces

```cpp
SemanticSnapshot CreateSnapshot(const SemanticDocument& document);
SemanticResult<SemanticDocument> RestoreSnapshot(const SemanticSnapshot& snapshot,
                                                 ObjectStoreKind store_kind);
CanonicalProjection ProjectCanonical(const SemanticDocument& document);
SemanticDigest SemanticDigestOf(const SemanticDocument& document);
```

Digest algorithm/width/serialization are not invented here; use the current semantic projection/digest authority. If authority does not freeze a product digest algorithm, bind G1 Evidence to canonical projection bytes/hash as a verification artifact and record the open product choice rather than silently creating a compatibility promise.

### TDD Steps

- [ ] RED: direct replay to revision N equals snapshot(R)+tail(R+1..N).
- [ ] RED: snapshot restore into Reference and Indexed stores yields identical canonical projection.
- [ ] RED: repeat the same corpus 100 times and assert identical canonical projection bytes/digest.
- [ ] RED: ordering cases cover sibling OrderKey tie-break by ObjectId and all authority-classified canonical collections.
- [ ] RED: snapshot rejects malformed/unknown semantic content according to wire policy.
- [ ] Implement snapshot creation/restore without synthesizing edit Operations.
- [ ] Implement projection independently of storage container iteration order.
- [ ] Implement digest strictly over the frozen projection representation or documented verification representation.
- [ ] Run full 60 seed + release suites against Reference/Indexed stores and compare projection/error/digest outputs.
- [ ] Record clean-run hashes and first-divergence output from `run_g1_semantic_differential.py`.

### Mock / Oracle
- Canonical projection schema/golden is external semantic oracle.
- ReferenceObjectStore is storage oracle.
- Repeated replay and snapshot-tail equivalence are metamorphic oracles.

### Exit Criteria
- `direct N == snapshot R + tail` for all applicable corpus cases.
- Reference/Indexed projection parity passes.
- Repeated replay deterministic.
- Canonical ordering independent of internal hash/container ordering.
- Snapshot invalid inputs fail closed.

---

# Wave 6 — GT-G1-07: Semantic Replay Inspector CLI

**Recommended model:** `M`, Medium reasoning, `L` reviewer.

**Authority Inputs:** Public G1 semantic APIs only; no new semantic rules.

### Files

**Create:**
- `tools/semantic_replay/CMakeLists.txt`
- `tools/semantic_replay/main.cpp`
- `runtime/semantic/tests/semantic_replay_cli_test.cpp`

**Modify:**
- `CMakeLists.txt`
- `runtime/semantic/tests/CMakeLists.txt`

### CLI contract

Support at minimum:

```text
--snapshot <path>
--ops <path>
--store reference|indexed
--step <operation-index>
--dump-object <object-id>
--dump-change-set
--projection
--digest
--format text|json
```

Non-zero exit codes must distinguish invalid arguments, decode/semantic rejection and internal/tool failure. The CLI prints existing semantic error codes; it does not reinterpret them.

### TDD Steps

- [ ] RED CLI smoke test on one checked-in semantic fixture.
- [ ] RED reference/indexed run must produce equal final projection/digest.
- [ ] RED invalid-op fixture must report operation index + stable semantic error and return non-zero.
- [ ] Implement CLI using only public semantic APIs; no direct ObjectStore internals and no protobuf parsing outside codec API.
- [ ] Add deterministic JSON output suitable for Evidence diffs.
- [ ] Run on seed/release replay corpus and capture representative outputs.

### Mock / Oracle
- Production semantic APIs are the sole behavior source.
- Golden projection/error corpus validates output.

### Exit Criteria
- Inspector can replay snapshot + op stream, step, dump object/change set, projection and digest.
- Reference/Indexed modes match.
- Invalid input yields deterministic machine-readable diagnosis.
- No CLI-only semantic behavior exists.

---

# Wave 7 — GT-G1-08: G1 Gate Evidence

**Recommended model:** `L`, Medium-High reasoning, final `XL` Gate reviewer.

**Authority Inputs:** G0 Evidence discipline; `GATE_TASK_TRACKER.md`; G1 outputs; current Verification Strategy. Do not reuse the G0 platform-specific Gate Report type as if G1 were a platform Gate.

### Files

**Create:**
- `verification/schemas/gates/g1-gate-report.schema.json`
- `verification/tools/generate_g1_gate_evidence.py`
- `verification/tests/g1_gate_evidence_test.py`
- `verification/evidence/gates/G1/<commit>/GT-G1-01/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-02/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-03/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-04/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-05/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-06/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-07/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-08/gate-report.json`
- `docs/quality/evidence/g1/gt-g1-08-gate-review-<date>.md`

**Modify only after evidence is real:**
- `docs/planning/GATE_TASK_TRACKER.md`
- `docs/planning/R_MILESTONE_STATUS.md`
- applicable Requirement/Decision/Implementation/Validation/Evidence trace view files discovered in current `main`.

### Required Gate evidence dimensions

```text
E1 Machine contract + type boundary
E2 Strict codec / descriptor / positive-negative corpus
E3 Reference vs Indexed store differential + zero full-scan instrumentation
E4 Normalize / validation / idempotency / ApplyPlan fail-closed behavior
E5 Atomic SemanticDocument apply + ChangeSet
E6 Snapshot / projection / digest / replay determinism
E7 Semantic Replay Inspector reproducibility
E8 Commit-bound artifact hashes + lineage + clean CI
```

### TDD / Evidence Steps

- [ ] Write RED evidence-schema tests: missing task, mismatched source commit, missing artifact, wrong hash, failed test status or non-zero full-scan counter must prevent Gate PASS.
- [ ] Implement generic G1 evidence generator using existing repository-safe path/hash practices from G0, but with a semantic-specific input/report schema rather than platform lineage requirements.
- [ ] Run full semantic CTest suite in Debug and Release where applicable.
- [ ] Run semantic contract validator, descriptor reproducibility, 60-seed/release corpus, reference/indexed differential, atomicity fault tests and replay inspector smoke.
- [ ] Run 1K/10K/100K observational store baseline; acceptance requires correctness and `full_scan_count=0` for ID lookup/single-record mutation, not an invented Product latency SLO.
- [ ] Bind every Evidence file to the exact implementation commit and SHA-256.
- [ ] Generate Gate Report. Any missing/failed E1..E8 yields non-PASS.
- [ ] Only after report PASS, update `GT-G1-01..08` tracker fields, G1 aggregate status, R milestone impact and traceability in the same PR.

### Mock / Oracle
- G1 gate-report schema + hash verifier is the evidence integrity oracle.
- Existing G0 safe-path/hash binding patterns are implementation references, not G1 semantic policy.

### Exit Criteria
- E1..E8 all PASS on one commit-bound run.
- No missing task Evidence or hash mismatch.
- Tracker and R milestone state match generated Gate Report.
- Historical G0/POC Evidence remains untouched.
- G1 is promoted to Pass only after independent Gate review.

---

## 4. Cross-Task Verification Matrix

| Invariant | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| No Scene/Skia/platform dependency | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | Gate check |
| 9 ObjectKinds / 15 Operations | ✓ | ✓ | — | ✓ | ✓ | ✓ | consumes | Gate check |
| f64 finite / -0 canonical | ✓ | ✓ | — | ✓ | ✓ | ✓ | observes | Gate check |
| OrderKey bytes 1..32 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | observes | Gate check |
| Unknown semantic fail closed | — | ✓ | — | ✓ | ✓ | ✓ | reports | Gate check |
| Validate-before-mutate | — | — | — | ✓ | ✓ | — | observes | Gate check |
| Idempotency no second mutation | — | — | — | ✓ | ✓ | ✓ | observes | Gate check |
| Reference == Indexed | — | — | ✓ | ✓ | ✓ | ✓ | ✓ | Gate check |
| Atomic revision/ChangeSet | — | — | — | plan | ✓ | ✓ | observes | Gate check |
| Snapshot-tail == direct replay | — | — | — | — | — | ✓ | ✓ | Gate check |
| No ObjectId full scan | — | — | ✓ | ✓ | ✓ | observational | — | Gate check |

---

## 5. Recommended Codex Dispatch Prompts

Use one fresh Codex thread per Wave/review unit.

### Wave 1 prompt

```text
Execute GT-G1-01 from docs/superpowers/plans/2026-08-26-g1-semantic-kernel.md.
Use the largest available coding/reasoning model with High reasoning.
First re-read the named Notion semantic authority pages and current main; do not use the closed migration branch as authority.
Follow TDD exactly and stop after GT-G1-01 Evidence + review-ready commit. Do not begin codec/store implementation.
```

### Wave 2A prompt

```text
Execute GT-G1-02 only. Start from main after accepted GT-G1-01.
Use XL / High reasoning. Re-read Common Wire, Reference IDL, codec freeze and binary-golden authority.
Materialize current semantic corpus/descriptor as executable GitHub artifacts, not a Notion mirror.
Stop after codec/descriptor/corpus Evidence is review-ready.
```

### Wave 2B prompt

```text
Execute GT-G1-03 only, in a separate worktree from GT-G1-02.
Use L / Medium-High reasoning. Preserve ReferenceObjectStore as simple permanent oracle and prove IndexedObjectStore has no ObjectId full scan.
Do not implement validation or SemanticDocument.
```

### Wave 3 prompt

```text
Execute GT-G1-04 only from main containing accepted G1-02 and G1-03.
Use XL / Very High reasoning and request a fresh correctness review.
The tracker correction is binding: Normalize and Idempotency are required. Validate/Prepare must not mutate semantic state.
```

### Wave 4 prompt

```text
Execute GT-G1-05 only. Use XL / Very High reasoning.
Focus on atomic SemanticDocument state transition, exact revision semantics and G2-safe SemanticChangeSet. Inject commit-time failures and prove no partial visibility.
```

### Wave 5 prompt

```text
Execute GT-G1-06 only. Use XL / High reasoning.
Prove deterministic canonical projection, snapshot restore, digest representation and direct-vs-snapshot-tail replay parity across Reference and Indexed stores.
Do not introduce SceneCompiler behavior.
```

### Wave 6 prompt

```text
Execute GT-G1-07 only. Use M / Medium reasoning.
Build the Semantic Replay Inspector strictly over public semantic APIs. Do not invent semantic rules in the CLI.
```

### Wave 7 prompt

```text
Execute GT-G1-08 only. Use L / Medium-High reasoning, then request XL Gate review.
Generate commit-bound E1..E8 Evidence using repository-safe path/hash verification. Only update tracker/G1 status if the generated Gate Report passes.
```

---

## 6. PR / Commit Strategy

Recommended review units:

```text
PR G1-A: GT-G1-01
PR G1-B: GT-G1-02
PR G1-C: GT-G1-03              (B/C may be developed in parallel; merge only after G1-01)
PR G1-D: GT-G1-04
PR G1-E: GT-G1-05
PR G1-F: GT-G1-06
PR G1-G: GT-G1-07
PR G1-H: GT-G1-08 + final Gate state transition
```

This is intentionally more granular than G0’s larger work packages because G1 correctness contracts become dependencies for G2, G4, G6, G7 and G8. Squash is acceptable for a task PR only if its Evidence does not require ancestry to an intermediate tested commit; otherwise preserve the tested commit according to existing evidence policy.

---

## 7. Final G1 Exit Criteria

G1 is Pass only when all of the following are true on one accepted `main` lineage:

1. Current V1 semantic machine contract needed for execution is promoted into GitHub without creating a second narrative Notion mirror.
2. Production `runtime/semantic` exists and its public boundary is encoding-neutral and Scene/render/platform/storage/sync free.
3. Real proto compile + descriptor lock + registry/profile validation is reproducible.
4. Immutable 60-case seed identity and release-specific semantic/codec suites are materialized and validated.
5. Strict decode/canonical encode and normalized negative-error corpus pass.
6. ReferenceObjectStore and IndexedObjectStore produce equivalent canonical state; production ObjectId lookup/single-record mutation reports zero full scans.
7. Normalize → Validate → Idempotency → ApplyPlan is deterministic and never mutates on reject.
8. SemanticDocument apply is atomic, revision semantics are exact and ChangeSet is correct for downstream G2 without containing downstream runtime objects.
9. Snapshot restore, canonical projection, digest representation and replay are deterministic; snapshot-tail replay equals direct replay.
10. Semantic Replay Inspector is runnable and produces deterministic machine-readable diagnosis.
11. E1..E8 Evidence is commit-bound, hash-verified and reproducible.
12. `GT-G1-01..08` and G1 status are updated only after the generated Gate Report and independent review both pass.

**Primary G1 artifacts:** production `runtime/semantic/`, promoted executable semantic contract/corpus, Semantic Replay Inspector, and commit-bound G1 Gate Report.

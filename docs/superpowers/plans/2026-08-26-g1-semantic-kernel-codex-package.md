# G1 Semantic Kernel Implementation Plan / Codex Package

> **For agentic workers:** REQUIRED SUB-SKILL: use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans`. Every behavior change follows RED → GREEN → refactor. Every Gate Task gets an independent review checkpoint. No task is Pass without commit-bound Evidence.

**Goal:** Build the first production Axiom Semantic Kernel: current V1 semantic authority → executable machine contract → encoding-neutral C++ types → strict codec → reference/production object stores → normalize/validate/idempotency/ApplyPlan → atomic SemanticDocument + ChangeSet → deterministic snapshot/projection/digest/replay → Semantic Replay Inspector → G1 Gate Evidence.

**Architecture:** Create `runtime/semantic/` as a production module depending on `runtime/foundation/` and forbidden from depending on `runtime/scene/`, Skia, Arc, native platform APIs, storage/sync, networking, or renderer state. Notion remains the living design authority. GitHub receives only executable contracts, code, tests, corpus and Evidence promoted for G1. The closed `docs/notion-bridge-bootstrap` branch is comparison/reference material only.

**Tech Stack:** C++20, CMake 3.30+, CTest, the protobuf/Edition 2024 path currently proposed by the Notion release-candidate material, YAML/JSON registries, Python/Node verification tooling already present under `verification/`, SHA-256 evidence binding. Protobuf/Edition 2024 becomes an accepted repository codec contract only after GT-G1-01 authority reconciliation, GT-G1-02 codec validation and descriptor reproducibility; this plan does not pre-accept it.

**Normative task source:** `docs/planning/GATE_TASK_TRACKER.md` on the accepted `main` lineage.

## Authority Inputs

Before each task, re-read the applicable current Notion pages by title and record retrieval timestamp in Evidence. Do not commit private Notion URLs/page IDs.

Core authority set:
- `G1 Semantic Kernel Implementation Plan v0.1`
- `Axiom Semantic Schema V1 Release Candidate Final Gate v0.1`
- `Schema Freeze Review + V1 Release Candidate Gate v0.1`
- `Axiom Semantic Schema Spec + IDL v0.1`
- `Axiom Reference IDL + Codec Mapping v0.1`
- `Generated Proto Baseline + Canonical Codec Freeze v0.1`
- `Canonical Codec Differential Runner + Descriptor Lock v0.1`
- `Canonical Codec Binary Golden Corpus v0.1 — Operation / Object / Snapshot Expansion`
- current Field Registry, ShapeKind Registry, BrushFamily Registry, Operation Registry, Common Wire Rules, OrderKey, RichText, Stroke, Connector and hard-limit authorities
- GitHub ADR-0001, ADR-0003, ADR-0014, ADR-0016, ADR-0019, ADR-0020, ADR-0025

## G1 Task Matrix

The tracker remains the normative repository task source. This compact matrix is repeated here so each
implementation PR carries the same minimum traceability fields instead of relying on a file list alone.

| Notion Task ID | Gate Task ID | R contribution | Requirements | ADR/RFC/Contract | Dependencies | Design | Implementation | Validation | Evidence | Blocker | Final |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| G1/Task 1 | `GT-G1-01` | R1,R2 | REQ-OBJ, REQ-INK, REQ-TEXT | D-G1; semantic authority reconciliation | G0 Pass | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-01/` | authority reconciliation | Not Started |
| G1/Task 2 | `GT-G1-02` | R1,R2 | REQ-OBJ, REQ-INK, REQ-TEXT | D-G1; codec/descriptor contract | GT-G1-01 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-02/` | — | Not Started |
| G1/Task 3 | `GT-G1-03` | R1,R2 | REQ-OBJ, REQ-EDIT | D-G1; ObjectStore contract | GT-G1-01 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-03/` | — | Not Started |
| G1/Task 4 | `GT-G1-04` | R1,R2 | REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | D-G1; validation/ApplyPlan contract | GT-G1-02, GT-G1-03 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-04/` | — | Not Started |
| G1/Task 5 | `GT-G1-05` | R1,R2 | REQ-OBJ, REQ-EDIT, REQ-INK, REQ-TEXT | D-G1; ChangeSet contract | GT-G1-04 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-05/` | — | Not Started |
| G1/Task 6 | `GT-G1-06` | R1,R2 | REQ-GAP-DATA, REQ-EDIT-HIST-001 | D-G1; Snapshot/Projection contract | GT-G1-05 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-06/` | — | Not Started |
| G1/Task 7 | `GT-G1-07` | R1,R2 | REQ-GAP-VER | D-G1; public semantic APIs | GT-G1-06 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-07/` | — | Not Started |
| G1/Task 8 | `GT-G1-08` | R1,R2 | REQ-GAP-VER | D-G1; G1 Gate Report contract | GT-G1-01..GT-G1-07 | Not Started | Not Started | Not Started | `verification/evidence/gates/G1/<commit>/GT-G1-08/` | — | Not Started |

---

## 0. Mandatory Reconciliation Corrections

### RC-G1-01 — G0 did not accept a production Semantic Kernel

Current `main` intentionally contains only a semantic bootstrap. `verification/tools/generate_semantic_bootstrap.py` records `NOT_G1_SEMANTIC_ACCEPTANCE`, `NO_PRODUCT_SEMANTIC_KERNEL`, and `NO_60_CASE_SEMANTIC_CORPUS`. Therefore G1 must promote the current semantic machine contract and corpus into GitHub; do not claim G0 already supplied them.

### RC-G1-02 — reuse only foundation primitives that actually match semantic authority

- Candidate reuse: `canvas::foundation::ObjectId` after byte-layout/zero-ID tests.
- Pattern reuse only: `canvas::foundation::Result<T>`; do not reuse its RF01 error enum as semantic codec taxonomy.
- Do not reinterpret `SceneRevision` or `ContentRevision` as SemanticDocument revision.
- Do not use `canvas::foundation::StableOrderKey`: it is `uint64_t`, while semantic OrderKey is opaque 1..32 bytes.
- Do not use `WorldPoint` / `WorldRect` as durable semantic geometry: they are `float`, while canonical semantic numbers are finite binary64/f64 with authority-defined normalization.

### RC-G1-03 — POC-01 is reference material, not the production data model

`pocs/shared_engine` may provide replay/test patterns. It uses experimental `uint64_t` IDs, integer order, float geometry and a subset of objects. No production semantic header may include POC headers.

### RC-G1-04 — migration branch artifacts require promotion review

For every candidate artifact from `docs/notion-bridge-bootstrap`:
1. re-read current authority;
2. compare candidate content;
3. classify `REUSE`, `REGENERATE`, or `REJECT`;
4. regenerate derived artifacts from promoted source;
5. bind provenance in task Evidence;
6. never copy `docs/notion/**` into `main`.

---

## 1. Global Constraints

- G1 contains exactly `GT-G1-01..GT-G1-08`.
- Promotion route remains `G0 Pass → G1 → G2`.
- V1 canonical mutation is Operation-only; no global canonical Transaction.
- V1 ObjectKinds and canonical Operations are the sets produced by GT-G1-01 authority reconciliation. The current Notion RC material proposes 9 ObjectKinds and 15 Operations, but those counts are candidate inputs until the reconciliation artifact records source status and promotion decision; they are not pre-frozen by this plan.
- Reject-before-mutate: any rejected operation leaves state, revision, indexes and emitted ChangeSet unchanged.
- Unknown semantic kind/enum/operation fails closed. Unknown-field/version behavior follows Common Wire Rules, not protobuf defaults.
- Canonical durable numerics are finite binary64/f64; normalize `-0 → +0` where authority requires.
- Semantic OrderKey is opaque 1..32 bytes; sibling total order is `(OrderKey unsigned lexicographic, ObjectId)`.
- Production single-object lookup must not perform an O(N) object scan.
- `runtime/semantic/include/**` must not include Scene, Skia, Arc, platform, persistence/sync, networking or product-shell ABI dependencies.
- A Product Page maps to exactly one Axiom `Document`. G1 does not create a Page `ObjectKind`, a `DocumentRoot → Page*` synthetic root, or a multi-Page semantic document. Page Collection ownership, navigation and lifecycle remain in the upper Product Shell; snapshots, revisions, projections, digests and replay are bound to one Document identity.
- Resource identity is semantic (`ResourceId`, `ResourceManifest`, `ContentHash`); resource availability, blob storage and cache state are not semantic object mutations.
- RichText, VectorStroke, DabStroke, EraseMask, Connector, Group, Sticky and any other V1 surface are covered only when their authority-reconciled registry entries exist. This plan must not infer a final surface from a file count or historical POC subset.
- Snapshot restore reconstructs canonical state and does not synthesize user-edit Operations.
- `SemanticChangeSet` contains semantic IDs/hints only; no RuntimeScene/GPU/renderer pointers.
- Historical POC/G0 Evidence is preserved; G1 creates new lineage.
- Every task writes Evidence under `verification/evidence/gates/G1/<commit>/GT-G1-XX/`.

---

## 2. Codex Model / Reasoning Classes

Use capability classes rather than a model SKU so the plan survives model renames.

| Class | Execution model | Reasoning | Context | Review |
| --- | --- | --- | --- | --- |
| `XL` | largest available Codex coding/reasoning model | High / Very High | fresh full-repo context + named Notion authority | fresh independent reviewer required |
| `L` | strong general coding model | Medium-High | full relevant module + authority | independent reviewer recommended |
| `M` | standard coding model | Medium | focused context with frozen interfaces | normal review |

| Task | Complexity | Execution | Reasoning | Reviewer |
| --- | ---: | --- | --- | --- |
| GT-G1-01 | 5/5 | `XL` | High | fresh `XL` |
| GT-G1-02 | 5/5 | `XL` | High | fresh `XL` |
| GT-G1-03 | 4/5 | `L` | Medium-High | `L` |
| GT-G1-04 | 5/5 critical | `XL` | **Very High** | fresh `XL` |
| GT-G1-05 | 5/5 critical | `XL` | **Very High** | fresh `XL` |
| GT-G1-06 | 5/5 | `XL` | High | fresh `XL` |
| GT-G1-07 | 3/5 | `M` | Medium | `L` |
| GT-G1-08 | 4/5 | `L` | Medium-High | final `XL` Gate review |

Do not execute GT-G1-04 or GT-G1-05 with a small/fast model.

---

## 3. Wave Topology

```text
Wave 1     GT-G1-01
              │
Wave 2    ┌────┴────┐
           02       03       separate worktrees allowed after 01 merges
           └────┬────┘
Wave 3          04
                │
Wave 4          05
                │
Wave 5          06
                │
Wave 6          07
                │
Wave 7          08
                │
              G1 Pass
```

Each Wave starts from the latest accepted `main`.

---

# Wave 1 — GT-G1-01: Semantic Target + Canonical Type Boundary

**Model:** `XL` / High; fresh `XL` review.

**Authority Inputs:** core authority set; Field/ShapeKind/BrushFamily/Operation registries; ADR-0001/0003/0016/0019/0025.

### Exact files

Promote after authority comparison:
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
- `schema/axiom/v1/README.md`

Create runtime:
- `runtime/semantic/CMakeLists.txt`
- `runtime/semantic/include/canvas/semantic/canonical_numeric.hpp`
- `runtime/semantic/include/canvas/semantic/order_key.hpp`
- `runtime/semantic/include/canvas/semantic/semantic_revision.hpp`
- `runtime/semantic/include/canvas/semantic/object_record.hpp`
- `runtime/semantic/include/canvas/semantic/operation.hpp`
- `runtime/semantic/include/canvas/semantic/change_set.hpp`
- `runtime/semantic/tests/CMakeLists.txt`
- `runtime/semantic/tests/semantic_types_test.cpp`

Modify:
- `CMakeLists.txt`
- `tools/check_runtime_boundaries.py`

### Tests / RED → GREEN

1. Write `semantic_types_test.cpp` first. RED must cover: 16-byte ObjectId compatibility, zero ID, OrderKey lengths 0/1/32/33, unsigned lexicographic ordering, canonical negative-zero normalization, the ObjectKind/Operation registry generated by GT-G1-01, and encoding-neutral construction without protobuf/Scene headers. A hard-coded count is invalid before reconciliation.
2. Extend `tools/check_runtime_boundaries.py` before implementation. RED must reject a deliberate semantic header fixture containing Scene/Skia/platform/storage/network symbols.
3. Run:
   ```bash
   cmake -S . -B out/g1-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCANVAS_BUILD_POC01=OFF -DCANVAS_BUILD_SEMANTIC=ON
   cmake --build out/g1-debug
   ctest --test-dir out/g1-debug -R 'semantic|runtime_boundaries' --output-on-failure
   ```
   Expected RED: semantic target/types absent.
4. Promote only authority-equivalent proto/registry/profile source.
5. Add `CANVAS_BUILD_SEMANTIC`; ensure `runtime/foundation` is added once when Semantic and/or RF01 is enabled.
6. Implement the minimal C++ type boundary and rerun the same commands to GREEN.

### Mock / Oracle
- Design oracle: current Notion semantic authority.
- Boundary oracle: `tools/check_runtime_boundaries.py`.
- Reference branch: comparison input only.
- POC types are not a layout oracle.

### Evidence
Create:
- `verification/evidence/gates/G1/<commit>/GT-G1-01/authority-reconciliation.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-01/test-manifest.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-01/summary.json`

`authority-reconciliation.json` records title, retrieval timestamp, candidate path, classification `REUSE|REGENERATE|REJECT`, and promoted SHA-256; no private source URL.

### Exit Criteria
- The candidate proto and registry/profile sources are promoted only after comparison; the current Notion inventory of 12 proto files and six registry/profile files is a candidate inventory, not a pre-approved repository manifest.
- semantic target builds warnings-clean without forbidden dependencies.
- ID/OrderKey/numeric/type-surface tests pass.
- public semantic headers are self-contained.
- Evidence hashes bind the exact implementation commit.

---

# Wave 2A — GT-G1-02: Codec Boundary + Strict Decode Result

**Model:** `XL` / High; fresh `XL` review.

**Authority Inputs:** GT-G1-01 promoted source; Reference IDL + Codec Mapping; Common Wire Rules; Canonical Codec Freeze; Descriptor Lock; Binary Golden Corpus; V1 RC Final Gate.

### Exact files

Create:
- `runtime/semantic/include/canvas/semantic/semantic_error.hpp`
- `runtime/semantic/include/canvas/semantic/codec.hpp`
- `runtime/semantic/src/codec.cpp`
- `runtime/semantic/tests/codec_test.cpp`
- `runtime/semantic/tests/codec_negative_test.cpp`
- `schema/axiom/v1/descriptor/descriptor.lock.pb`
- `schema/axiom/v1/descriptor/descriptor.lock.sha256`
- `schema/axiom/v1/toolchain/toolchain.lock.json`
- `verification/corpus/semantic/v1/corpus.json`
- `verification/corpus/semantic/v1/suites/seed-v0.1.json`
- `verification/tools/validate_semantic_contract.py`
- `verification/tests/test_semantic_contract.py`

Promote binary corpus into:
- `verification/corpus/semantic/v1/wire/bg/`
- `verification/corpus/semantic/v1/wire/bgx/`
- `verification/corpus/semantic/v1/suites/shape-kind-v1.json`
- `verification/corpus/semantic/v1/suites/image-content-v1.json`
- `verification/corpus/semantic/v1/suites/connector-v1.json`
- `verification/corpus/semantic/v1/suites/brush-family-v1.json`
- `verification/corpus/semantic/v1/suites/brush-interpreter-v1.json`
- `verification/corpus/semantic/v1/suites/pressure-tilt-v1.json`
- `verification/corpus/semantic/v1/suites/richtext-font-v1.json`
- `verification/corpus/semantic/v1/suites/protocol-hard-limits-v1.json`

Modify:
- `runtime/semantic/CMakeLists.txt`
- `verification/corpus/semantic/README.md`
- `verification/workspace.json`
- `verification/tests/test_workspace.py`

### Interface

`codec.hpp` exports an encoding-neutral success/failure result. The exact semantic error codes come from current authority; the plan does not invent enum members. Generated protobuf types are private to `codec.cpp`/generated-code implementation files and never appear in `object_record.hpp`, `operation.hpp`, ObjectStore or SemanticDocument public headers.

### Tests

RED first:
- valid seed decode;
- truncated/malformed wire;
- unsupported semantic version;
- unknown kind/enum/operation;
- duplicate canonical keys;
- non-finite values;
- hard-limit violation;
- `-0 → +0` canonical behavior;
- same-semantic→same-bytes cases;
- exactly 60 stable IDs in `seed-v0.1`.

Then:
1. Compile promoted Edition 2024 proto with the pinned toolchain.
2. Generate `descriptor.lock.pb` from source; never copy a stale descriptor blindly.
3. Implement explicit Common Wire unknown-field/version policy.
4. Run clean generation twice and compare descriptor/corpus hashes.
5. Run:
   ```bash
   python3 verification/tools/validate_semantic_contract.py --root .
   python3 -m unittest verification.tests.test_semantic_contract -v
   ctest --test-dir out/g1-debug -R semantic_codec --output-on-failure
   ```

### Mock / Oracle
- Common Wire + Reference IDL: normative policy oracle.
- BG/BGX: byte oracle.
- Descriptor lock: structural proto oracle only.
- 60-case seed identity: immutable corpus identity oracle.

### Evidence
- `verification/evidence/gates/G1/<commit>/GT-G1-02/descriptor.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-02/corpus-identity.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-02/codec-results.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-02/summary.json`

### Exit Criteria
- real proto compile succeeds with pinned toolchain;
- descriptor is reproducible;
- 60-case seed identity is machine-locked;
- positive/negative codec corpus passes;
- same-semantic→same-bytes vectors match exactly;
- decode never mutates Document state.

---

# Wave 2B — GT-G1-03: ReferenceObjectStore + IndexedObjectStore

**Model:** `L` / Medium-High; `L` review.

**Authority Inputs:** GT-G1-01 types; Product Object Model; ObjectContent/Placement/OrderKey authority; ADR-0003/0016/0019.

### Exact files
Create:
- `runtime/semantic/include/canvas/semantic/object_store.hpp`
- `runtime/semantic/include/canvas/semantic/reference_object_store.hpp`
- `runtime/semantic/include/canvas/semantic/indexed_object_store.hpp`
- `runtime/semantic/src/reference_object_store.cpp`
- `runtime/semantic/src/indexed_object_store.cpp`
- `runtime/semantic/tests/object_store_test.cpp`
- `runtime/semantic/tests/object_store_differential_test.cpp`
- `runtime/semantic/tests/object_store_lookup_instrumentation_test.cpp`

Modify:
- `runtime/semantic/CMakeLists.txt`

### Interface
One ObjectStore contract supports Find/Insert/Replace/Erase and deterministic canonical ID enumeration. If the existing Result template cannot carry `void`, add a semantic-local status/result specialization; do not alter RF01 error semantics.

### Tests
1. RED Reference store insert/find/replace/erase.
2. RED Indexed store parity using the same deterministic operation sequence.
3. RED 10K mutation trace; compare after every 100 mutations.
4. RED instrumentation: production ID lookup and single-record mutation must report `full_scan_count == 0`.
5. Implement Reference store with deliberately simple correctness-first storage.
6. Implement Indexed store with ObjectId lookup plus only authority-required parent/sibling/order/reference indexes.
7. Add Debug/test index-consistency assertions.
8. Record 1K/10K/100K timings and counters as observational data, not Product SLO.

### Mock / Oracle
- `ReferenceObjectStore` is the permanent differential oracle.
- deterministic canonical record enumeration is the equality oracle until GT-G1-06 projection exists.

### Evidence
- `verification/evidence/gates/G1/<commit>/GT-G1-03/differential.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-03/lookup-instrumentation.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-03/baseline.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-03/summary.json`

### Exit Criteria
- Reference and Indexed stores agree on deterministic traces;
- production ID lookup/single-record mutation performs zero full scans;
- index invariants hold after insert/replace/erase;
- no Scene/spatial/render index appears in semantic storage.

---

# Wave 3 — GT-G1-04: Normalize + Validation + Idempotency + ApplyPlan

**Model:** `XL` / **Very High**; mandatory fresh `XL` correctness review.

**Authority Inputs:** Operation Payload + Validation Rules; Field Registry; Object/Placement/Connector/RichText/Stroke/Erase authorities; Common Wire; ADR-0014/0016/0019/0025. Tracker correction is binding: Normalize and Idempotency are required.

### Exact files
Create:
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

Modify:
- `runtime/semantic/CMakeLists.txt`

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
No stage before commit mutates ObjectStore, indexes, revision or history.

### Tests
RED first:
- negative-zero/canonical collection normalization;
- duplicate canonical keys;
- positive/negative cases for all 15 Operations;
- zero/missing IDs;
- duplicate create;
- wrong content kind;
- invalid FieldId/value type;
- placement cycle;
- connector target/anchor violations;
- stroke/mask target violations;
- RichText boundary violations;
- protocol hard limits;
- deterministic first-error ordering;
- idempotent accepted-operation replay;
- `validation_no_mutation` snapshot before/after each rejection.

Then implement the minimal stages and run the entire semantic negative corpus through Decode→Normalize→Validate→Prepare.

### Mock / Oracle
- authority validation tables are normative;
- ReferenceObjectStore is state lookup oracle;
- negative golden corpus is error-stage/path/code oracle.

### Evidence
- `verification/evidence/gates/G1/<commit>/GT-G1-04/operation-matrix.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-04/negative-results.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-04/no-mutation-results.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-04/summary.json`

### Exit Criteria
- all 15 Operations have positive/negative coverage;
- stage order and first failure are deterministic;
- idempotent replay matches authority;
- every rejection leaves state/revision/indexes unchanged;
- ApplyPlan fully resolves commit inputs without mutation.

---

# Wave 4 — GT-G1-05: SemanticDocument Atomic Apply + ChangeSet

**Model:** `XL` / **Very High**; mandatory fresh `XL` correctness review.

**Authority Inputs:** GT-G1-04 pipeline; Runtime Data Flow canonical commit pipeline; ChangeSet ownership; ADR-0003/0014/0019/0025.

### Exact files
Create:
- `runtime/semantic/include/canvas/semantic/document.hpp`
- `runtime/semantic/src/document.cpp`
- `runtime/semantic/tests/document_apply_test.cpp`
- `runtime/semantic/tests/document_atomicity_test.cpp`
- `runtime/semantic/tests/change_set_test.cpp`
- `runtime/semantic/tests/reference_indexed_document_differential_test.cpp`

Modify:
- `runtime/semantic/CMakeLists.txt`

### Observable result contract
SemanticDocument Apply has three observable outcomes:
- `APPLIED`: exactly one revision advance + one ChangeSet;
- `IDEMPOTENT_NOOP`: no revision advance, explicit no-op result;
- `REJECTED`: no state/revision/index mutation, SemanticError.

The exact C++ type/member spelling is chosen once in this task and frozen by tests; the three outcome semantics are mandatory.

### Tests
1. RED apply cases for all 15 operation families.
2. RED commit-time fault injection at store/index seams; require no partial state/revision/ChangeSet.
3. RED idempotent replay; require no second revision.
4. RED ChangeSet added/removed/modified IDs and semantic invalidation hints; forbid RuntimeScene/GPU fields.
5. Implement commit of a prevalidated ApplyPlan.
6. Run operation streams against Reference-backed and Indexed-backed documents and compare canonical state.
7. Extend boundary checks to `document.hpp`.

### Mock / Oracle
- Reference-backed SemanticDocument: differential state oracle.
- deterministic failure-injection seam: atomicity oracle.
- ChangeSet test matrix: downstream G2 contract oracle.

### Evidence
- `verification/evidence/gates/G1/<commit>/GT-G1-05/apply-matrix.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-05/atomicity-faults.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-05/change-set.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-05/summary.json`

### Exit Criteria
- all 15 operation families apply correctly;
- success advances revision once; reject/idempotent no-op does not;
- injected commit failures leave canonical state unchanged;
- Reference/Indexed documents converge;
- ChangeSet is G2-usable and downstream-runtime-free.

---

# Wave 5 — GT-G1-06: Snapshot + Canonical Projection + Digest + Replay

**Model:** `XL` / High; fresh `XL` determinism review.

**Authority Inputs:** Snapshot authority; Canonical Projection schema; Common Wire/canonical ordering; ADR-0016/0020; canonical binary golden corpus.

### Exact files
Create:
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
- `verification/tests/test_g1_semantic_differential.py`

Modify:
- `runtime/semantic/CMakeLists.txt`

### Tests
RED first:
- direct revision N == snapshot(R)+tail(R+1..N);
- snapshot restore into Reference/Indexed stores has equal projection;
- same corpus repeated 100 times has identical projection bytes;
- sibling OrderKey tie-break by ObjectId;
- all canonical collection orderings;
- snapshot identity, revision, frontier and digest remain bound to one Document; no Page ObjectKind or synthetic multi-Page root is emitted;
- ResourceId/ResourceManifest/ContentHash bindings survive projection and replay without treating blob availability or cache state as Document mutation;
- malformed/unknown snapshot fail-closed.

Then implement projection independent of internal container iteration order. Digest algorithm/width must come from current authority; if product digest is not frozen, G1 uses canonical projection bytes plus a verification-only SHA-256 in Evidence and records no product compatibility promise.

Run the 60 seed plus release suites through both stores and compare projection/error/digest outputs.

### Mock / Oracle
- canonical projection schema/golden: external semantic oracle;
- ReferenceObjectStore: storage oracle;
- replay repetition and snapshot-tail equivalence: metamorphic oracles.

### Evidence
- `verification/evidence/gates/G1/<commit>/GT-G1-06/replay-differential.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-06/snapshot-tail.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-06/determinism.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-06/summary.json`

### Exit Criteria
- direct == snapshot+tail for applicable corpus cases;
- Reference/Indexed projection parity passes;
- repeated replay is deterministic;
- canonical ordering is container-order independent;
- invalid snapshots fail closed.

---

# Wave 6 — GT-G1-07: Semantic Replay Inspector CLI

**Model:** `M` / Medium; `L` reviewer.

**Authority Inputs:** accepted public G1 semantic APIs only. This task introduces no semantic policy.

### Exact files
Create:
- `tools/semantic_replay/CMakeLists.txt`
- `tools/semantic_replay/main.cpp`
- `runtime/semantic/tests/semantic_replay_cli_test.cpp`

Modify:
- `CMakeLists.txt`
- `runtime/semantic/tests/CMakeLists.txt`

### CLI contract
Support:
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
Exit classes distinguish invalid CLI arguments, codec/semantic rejection and internal/tool failure. CLI emits existing semantic error codes verbatim.

### Tests
1. RED valid fixture smoke.
2. RED Reference/Indexed equal final projection/digest.
3. RED invalid fixture returns operation index + stable semantic error + non-zero exit.
4. Implement strictly over public semantic APIs; no direct ObjectStore internals, no direct protobuf parsing.
5. Assert deterministic JSON output byte-for-byte for a fixed fixture.

### Mock / Oracle
- public G1 semantic API is the sole behavior source;
- projection/error golden validates output.

### Evidence
- `verification/evidence/gates/G1/<commit>/GT-G1-07/cli-valid.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-07/cli-invalid.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-07/summary.json`

### Exit Criteria
- replay, step, object/change-set dump, projection and digest work;
- Reference/Indexed modes match;
- invalid inputs produce deterministic machine-readable diagnosis;
- no CLI-only semantic rule exists.

---

# Wave 7 — GT-G1-08: G1 Gate Evidence

**Model:** `L` / Medium-High; final `XL` Gate review.

**Authority Inputs:** G0 Evidence discipline; GATE_TASK_TRACKER; all accepted G1 outputs; Verification Strategy. G1 must not masquerade as a platform G0 Gate.

### Exact files
Create:
- `verification/schemas/gates/g1-gate-report.schema.json`
- `verification/tools/generate_g1_gate_evidence.py`
- `verification/tests/test_g1_gate_evidence.py`
- `docs/quality/evidence/g1/gt-g1-08-gate-review-20260826.md`

Generate under the exact tested commit:
- `verification/evidence/gates/G1/<commit>/GT-G1-01/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-02/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-03/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-04/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-05/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-06/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-07/summary.json`
- `verification/evidence/gates/G1/<commit>/GT-G1-08/gate-report.json`

Modify only after generated Gate Report PASS:
- `docs/planning/GATE_TASK_TRACKER.md`
- `docs/planning/R_MILESTONE_STATUS.md`
- `docs/planning/AXIOM_GATES_AND_STAGES.md`

### Required evidence dimensions
```text
E1 machine contract + type boundary
E2 strict codec + descriptor + positive/negative corpus
E3 Reference/Indexed differential + zero full-scan instrumentation
E4 normalize/validation/idempotency/ApplyPlan fail-closed behavior
E5 atomic SemanticDocument + ChangeSet
E6 snapshot/projection/digest/replay determinism
E7 Replay Inspector reproducibility
E8 commit-bound hashes + clean CI + lineage
```

### Tests / Evidence Steps
1. RED evidence tests: missing task, source-commit mismatch, missing artifact, wrong hash, failed status, or non-zero full-scan counter must prevent PASS.
2. Implement `generate_g1_gate_evidence.py` using the same repository-root, safe-relative-path, regular-file, byte-count and SHA-256 principles already used by G0 tooling.
3. Run semantic CTest in Debug and Release.
4. Run semantic contract validator, descriptor reproducibility, corpus tests, differential tests, atomicity faults and CLI smoke.
5. Run 1K/10K/100K store baseline; acceptance checks correctness + `full_scan_count=0`, not a guessed latency SLO.
6. Bind every Evidence artifact to exact source commit and SHA-256.
7. Generate Gate Report; missing/failed E1..E8 yields non-PASS.
8. Only after PASS and independent `XL` review, update the three planning documents above in the same PR.

### Mock / Oracle
- G1 gate-report schema + hash verifier: integrity oracle.
- G0 safe-path/hash implementation: implementation pattern only, not G1 semantic authority.

### Exit Criteria
- E1..E8 PASS on one accepted commit lineage;
- no missing Evidence/hash mismatch;
- tracker/R milestone/gate overview agree with generated report;
- G0/POC Evidence is untouched;
- independent Gate review approves G1 promotion.

---

## 4. Cross-Task Verification Matrix

| Invariant | 01 | 02 | 03 | 04 | 05 | 06 | 07 | 08 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| no Scene/Skia/platform dependency | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | gate |
| authority-reconciled ObjectKinds / Operations | ✓ | ✓ | — | ✓ | ✓ | ✓ | consume | gate |
| f64 finite / -0 canonical | ✓ | ✓ | — | ✓ | ✓ | ✓ | observe | gate |
| OrderKey bytes 1..32 | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | observe | gate |
| unknown semantic fail closed | — | ✓ | — | ✓ | ✓ | ✓ | report | gate |
| validate-before-mutate | — | — | — | ✓ | ✓ | — | observe | gate |
| idempotency no second mutation | — | — | — | ✓ | ✓ | ✓ | observe | gate |
| Reference == Indexed | — | — | ✓ | ✓ | ✓ | ✓ | ✓ | gate |
| atomic revision + ChangeSet | — | — | — | plan | ✓ | ✓ | observe | gate |
| snapshot-tail == direct replay | — | — | — | — | — | ✓ | ✓ | gate |
| no ObjectId full scan | — | — | ✓ | ✓ | ✓ | observe | — | gate |
| one Page = one Document; no Page ObjectKind/synthetic root | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | gate |
| ResourceId/ResourceManifest/ContentHash boundary | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | observe | gate |
| RichText/Stroke/EraseMask/Connector/Group/Frame/PDF authority coverage | ✓ | ✓ | — | ✓ | ✓ | ✓ | consume | gate |
| Snapshot identity binds one Document; Page Collection stays above runtime | — | — | — | — | — | ✓ | ✓ | gate |

---

## 5. Recommended Codex Dispatch Prompts

### Wave 1
```text
Execute GT-G1-01 from docs/superpowers/plans/2026-08-26-g1-semantic-kernel-codex-package.md.
Use the largest available coding/reasoning model with High reasoning.
First re-read the named current Notion semantic authority and current main. The closed migration branch is reference-only.
Follow TDD and stop after GT-G1-01 Evidence plus review-ready commit. Do not begin codec/store work.
```

### Wave 2A
```text
Execute GT-G1-02 only from main after accepted GT-G1-01.
Use XL / High reasoning. Re-read Common Wire, Reference IDL, codec freeze, descriptor lock and binary golden authority.
Promote current machine contract/corpus as executable GitHub artifacts; do not create a Notion narrative mirror.
Stop after codec/descriptor/corpus Evidence is review-ready.
```

### Wave 2B
```text
Execute GT-G1-03 only in a separate worktree from GT-G1-02.
Use L / Medium-High reasoning. Keep ReferenceObjectStore intentionally simple and prove IndexedObjectStore performs zero ObjectId full scans.
Do not implement validation or SemanticDocument.
```

### Wave 3
```text
Execute GT-G1-04 only after accepted G1-02 and G1-03.
Use XL / Very High reasoning and require a fresh correctness review.
Normalize and Idempotency are mandatory. Decode/Normalize/Validate/Prepare must not mutate semantic state.
```

### Wave 4
```text
Execute GT-G1-05 only. Use XL / Very High reasoning.
Focus on atomic SemanticDocument state transition, exact revision semantics and downstream-safe SemanticChangeSet. Inject deterministic commit failures and prove no partial visibility.
```

### Wave 5
```text
Execute GT-G1-06 only. Use XL / High reasoning.
Prove canonical projection, snapshot restore and direct-vs-snapshot-tail replay parity across Reference and Indexed stores. Do not implement SceneCompiler behavior.
```

### Wave 6
```text
Execute GT-G1-07 only. Use M / Medium reasoning.
Build the Replay Inspector strictly over public semantic APIs. Do not invent semantic policy in the CLI.
```

### Wave 7
```text
Execute GT-G1-08 only. Use L / Medium-High reasoning and request a final XL Gate review.
Generate commit-bound E1..E8 Evidence. Update G1 tracker/status only if the generated Gate Report passes and the independent review approves promotion.
```

---

## 6. PR Strategy

Recommended review units:
```text
PR G1-A  GT-G1-01
PR G1-B  GT-G1-02
PR G1-C  GT-G1-03   (B/C may develop in parallel after G1-A)
PR G1-D  GT-G1-04
PR G1-E  GT-G1-05
PR G1-F  GT-G1-06
PR G1-G  GT-G1-07
PR G1-H  GT-G1-08 + final Gate state transition
```

Squash is acceptable only when Evidence does not require ancestry to an intermediate tested commit; otherwise preserve the tested commit according to repository evidence policy.

---

## 7. Final G1 Exit Criteria

G1 is Pass only when all conditions hold on one accepted `main` lineage:
1. current V1 executable semantic contract is promoted without creating a second narrative Notion mirror;
2. production `runtime/semantic` exists with an encoding-neutral, downstream-runtime-free public boundary;
3. proto compile, descriptor lock and registry/profile validation are reproducible;
4. immutable 60-case seed and release semantic/codec suites are materialized and validated;
5. strict decode/canonical encode and normalized negative-error corpus pass;
6. Reference/Indexed stores are canonically equivalent and production lookup/mutation reports zero full scans;
7. Normalize→Validate→Idempotency→ApplyPlan is deterministic and reject-safe;
8. SemanticDocument apply is atomic with exact revision semantics and correct G2-facing ChangeSet;
9. snapshot restore, canonical projection and replay are deterministic; snapshot-tail equals direct replay;
10. Replay Inspector is runnable and deterministic;
11. E1..E8 Evidence is commit-bound, hash-verified and reproducible;
12. `GT-G1-01..08` and G1 status change only after generated Gate Report PASS plus independent review.

**Primary artifacts:** `runtime/semantic/`, promoted executable semantic contract/corpus, Semantic Replay Inspector, and commit-bound G1 Gate Report.

# GT-G1-02R-INGRESS P31 Task Package v0.1

> Repository-readable materialization of the approved Notion package
> `notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1`.
>
> This file is transport/materialization only. It does not expand or redesign
> Authority. The historical Notion field that pointed `package_materialization_ref`
> at `7d74d4544ca599fdf0294c80e4070290f57104db` is superseded by the exact
> repository commit containing this file. That exact commit SHA is supplied by
> the Aegis P31 repair return / subsequent P32 surface handoff.

## Package envelope

```yaml
package_id: GT-G1-02R-INGRESS-P31-v0.1
package_ref: notion://3d44c57a-590c-81f6-a0d7-d8ae909a968c/GT-G1-02R-INGRESS-P31-v0.1
task_id: GT-G1-02R-INGRESS
stage: P31_TASK_PACKAGING
stage_owner: aegis-implementation
verdict: READY
purpose: complete canonical protobuf Operation bytes -> complete typed Operation ingress

repository:
  provider: github
  full_name: Mostorm-Labs/axiom
canonical_branch: main

task_anchor:
  revision: 7d74d4544ca599fdf0294c80e4070290f57104db
  relation: ancestor
resume_cursor: null

execution_policy:
  package_boundary: OPERATION_CODEC_INGRESS_CAPABILITY
  substantive_package_budget: 1
  primary_gate_budget: 1
  verification_bound: true
  broad_reopen_forbidden: true

preferred_execution_ref:
  name: codex/gt-g1-02r-ingress
  state_at_packaging: ABSENT

P32_authorized: false
P34_executed: false
GT_G1_03_reopen: false
GT_G1_04_reopen: false
GT_G1_05_reopen: false
GT_G1_06_reopen: false
GT_G1_07_primary_gate: SUSPENDED_PENDING_CODEC_INGRESS_CLOSURE
GT_G1_08_authorized: false
```

## 1. Current Authority / trusted basis

This package is bound to the following trusted basis and may not silently
substitute newer floating labels:

- GT-G1-02R-INGRESS P30 Implementation Plan v0.1 — MINIMAL CODEC INGRESS CLOSURE / READY_FOR_P31
  - https://app.notion.com/p/3d44c57a590c81b88c52fa64e458baeb
- GT-G1-07 / GT-G1-02 P22 Five-Axis Drift Review v0.1 — MINIMAL REPAIR RADIUS
  - https://app.notion.com/p/3d44c57a590c813e88f0fc2676f57aad
- GT-G1-07 / GT-G1-02 P21 Authority Review v0.1 — READY_WITH_FINDINGS / CODEC DEPENDENCY COVERAGE DRIFT
  - https://app.notion.com/p/3d44c57a590c81f5a441c7299113077a
- G1 Semantic Kernel Implementation Plan v0.2
  - https://app.notion.com/p/3c84c57a590c816cb398dbf5481121ed
- Operation -> Semantic Document Runtime Data Flow v0.1
  - https://app.notion.com/p/3c44c57a590c81dc9f65ec33c8109492
- Generated Proto Baseline + Canonical Codec Freeze v0.1
  - https://app.notion.com/p/3c44c57a590c81c29b93c774869cff99
- Canonical Codec Implementation Skeleton + Binary Golden Seed v0.1
  - https://app.notion.com/p/3c44c57a590c8101a8d4e54bd9537e23

Repository code is Implementation Reality, not a replacement for these Authority surfaces.

## 2. Dependency and starting-state contract

P32 must establish repository identity first, then resolve the exact same-repository
package materialization commit containing this file, then verify `task_anchor`
ancestry.

```yaml
repository_identity: Mostorm-Labs/axiom
required_anchor: 7d74d4544ca599fdf0294c80e4070290f57104db
required_relation: ancestor
historical_head_equality_required: false
resume_cursor: null
```

If current execution HEAD is a valid descendant of the anchor, reconcile the
descendant delta and continue only if it does not conflict with this package.
If ancestry or scope safety cannot be established, fail closed.

## 3. Required production change

The only semantic production change is to complete the existing
`SemanticCodec::decodeProtobufOperation(...)` seam in
`runtime/semantic/src/codec.cpp`.

Required behavior:

1. Preserve existing protobuf wire preflight, duplicate/unknown/wrong-wire rejection, hard limits, parser failure behavior, envelope presence observations and protobuf-off behavior.
2. After generated DTO parse succeeds, map `operation_id`, `document_id`, `schema_version`, `payload_version` and the selected payload branch into a complete `canvas::semantic::Operation`.
3. Implement all 15 released Operation families using existing codec-private mapping helpers where available; add only codec-private helpers needed for missing DTO-to-domain conversions.
4. Return `SemanticError::kNone` only after the selected payload has been completely reconstructed.
5. Any structurally representable but unmappable domain value remains fail-closed; no hidden default repair.
6. Do not introduce a second validator, normalizer, apply engine, downstream inspector reconstruction layer, public protobuf DTO ABI, or alternate byte-ingress path.
7. Do not repurpose legacy `SemanticCodec::decodeOperation(...)`; its AX/canonical-field contract remains unchanged.

## 4. Exact source scope

### MODIFY

- `runtime/semantic/src/codec.cpp`
- `runtime/semantic/include/canvas/semantic/codec.hpp`
  - comment/contract correction only
  - existing signature must remain unchanged
- `runtime/semantic/tests/CMakeLists.txt`
- `.github/workflows/g1-semantic-codec.yml`
  - only the minimum wiring needed for exact-source ingress evidence/provider capture

### CREATE

- `runtime/semantic/tests/codec_operation_ingress_test.cpp`
- `verification/tools/generate_g1_02r_ingress_evidence.py`
- `verification/tests/test_g1_02r_ingress_evidence.py`

### FORBIDDEN production/source changes

- `schema/axiom/v1/**`
- Operation enum/payload/domain type redesign
- `runtime/semantic/src/operation_engine*`
- object-store/index implementation
- GT-G1-03, GT-G1-04, GT-G1-05 or GT-G1-06 production semantics
- historical `GT-G1-02R` fixture/evidence files or namespaces

Any additional source path requires a blocked return to CONTROL_REASONING before mutation.

## 5. Non-goals

- no proto/tag/descriptor refreeze
- no new Operation family
- no canonical writer redesign
- no Snapshot decoder work
- no RuntimeScene/rendering work
- no GT-G1-07 inspector repair inside this package
- no repository integration/merge to `main`
- no P34 verdict generation
- no performance optimization unrelated to linear DTO-to-domain reconstruction

## 6. Implementation steps

This is one P31 package; the steps are implementation mechanics, not lifecycle subpackages.

```text
STEP-I01  Add failing independent 15-family byte-ingress oracle test.
STEP-I02  Lock envelope identity/presence assertions and protobuf-off behavior.
STEP-I03  Implement Insert/Delete/Restore object mapping.
STEP-I04  Implement Placement/Transform/Property/ObjectSize mapping.
STEP-I05  Implement VectorPath/Image mapping.
STEP-I06  Implement Stroke/Split/EraseMask mapping.
STEP-I07  Implement RichText/Connector mapping.
STEP-I08  Replace incomplete-seam comment only after all 15 families succeed.
STEP-I09  Run focused negative/preflight/legacy decoder regressions.
STEP-I10  Run full semantic protobuf ON/OFF verification.
STEP-I11  Generate exact-source ingress evidence and validate its schema/ref bindings.
STEP-I12  Push exact source, obtain hosted exact-source provider result, then materialize reviewer-accessible evidence-only descendant.
```

## 7. Acceptance criteria

```yaml
AC-I01:
  all_15_operation_families: COMPLETE_TYPED_DECODE
  decode_surface: SemanticCodec::decodeProtobufOperation
  successful_error: kNone
AC-I02:
  envelope_exact:
    operation_id: true
    document_id: true
    schema_version: true
    payload_version: true
    schema_version_presence: true
    payload_version_presence: true
AC-I03:
  family_payload_variant_exact: true
  non_empty_nested_semantic_content_required: true
  silent_empty_default_substitution: forbidden
AC-I04:
  independent_typed_oracle: required
  production_encodeProtobufOperation_as_sole_input_oracle: forbidden
AC-I05:
  existing_normalize_validate_apply_path_consumable: required
  alternate_semantic_engine: forbidden
AC-I06:
  malformed_wire_regression: pass
  unknown_field_regression: pass
  duplicate_singular_regression: pass
  wrong_wire_type_regression: pass
  limit_regression: pass
AC-I07:
  protobuf_off_decodeProtobufOperation: kRuntimeUnavailable
AC-I08:
  legacy_decodeOperation_behavior_change: none
AC-I09:
  schema_proto_tag_descriptor_changes: 0
  GT_G1_03_through_06_semantic_changes: 0
AC-I10:
  exact_source_clean_checkout: pass
  hosted_exact_source_run: success
  reviewer_accessible_materialized_ref: required
```

## 8. Acceptance oracle / tests

`acceptance_oracle_id: GT-G1-02R-INGRESS-ORACLE-v0.1`

### T-I01 — 15-family complete typed reconstruction

`codec_operation_ingress_test.cpp` must contain at least one non-empty valid
canonical protobuf byte representative for every released family:

1. InsertObjects
2. DeleteObjects
3. RestoreObjects
4. SetPlacements
5. SetTransforms
6. PatchProperties
7. SetObjectSize
8. SetVectorPathGeometry
9. SetImageContent
10. AddStroke
11. SplitStrokes
12. AddEraseMasks
13. RemoveEraseMasks
14. EditRichText
15. SetConnectorContent

For every row, assert exact envelope fields, exact payload variant and
family-specific nested domain values. Input bytes may come from a test-only wire
builder or frozen canonical fixture, but must not be generated solely by the
production encoder under test. Expected typed values must be authored
independently of production decode.

### T-I02 — downstream semantic consumability

Use decoded Operations with the existing normalization/validation surfaces and
at least one existing OperationEngine-compatible representative path. The test
must prove that no GT-G1-07-style reconstruction adapter is required. Do not
create a second validator/apply implementation for this test.

### T-I03 — decoder regression set

Run existing `canvas_semantic_codec_test`, including `codec_test.cpp` and
`codec_negative_test.cpp`, and retain current malformed/unknown/duplicate/wire-type/limit behavior.

### T-I04 — legacy decoder non-regression

Retain existing legacy `decodeOperation(...)` tests and add a targeted assertion
if needed to prove the AX/canonical-field decoder behavior did not change.

### T-I05 — protobuf-off

A protobuf-disabled semantic build must compile and its byte-ingress assertion
must observe `kRuntimeUnavailable`; no test fake may emulate successful protobuf decode.

### T-I06 — full semantic matrix

Run full semantic CTest with protobuf ON, then protobuf OFF. OFF may contain only
already-declared skips/not-applicable cases tied to protobuf availability; no
new unexpected failure is allowed.

## 9. VerificationSpec / proof binding

```yaml
verification_spec_id: GT-G1-02R-INGRESS-VS-v0.1
trusted_basis_id: GT-G1-02R-INGRESS-TB-v0.1
scope_contract_id: GT-G1-02R-INGRESS-SCOPE-v0.1
acceptance_oracle_id: GT-G1-02R-INGRESS-ORACLE-v0.1
evidence_compilation_contract_id: GT-G1-02R-INGRESS-ECC-v0.1
obligations:
  OI-01: AC-I01 + AC-I02 + AC-I03
  OI-02: AC-I04 + AC-I05
  OI-03: AC-I06 + AC-I07 + AC-I08
  OI-04: AC-I09
  OI-05: AC-I10
```

P32 must run `PackageBindingPreflight` and `EvidenceContractPreflight` before
mutation. Any unresolved semantic choice, floating trust label, or evidence
requirement that cannot be produced on the selected provider is a blocker.

## 10. Evidence compilation contract

`evidence_compilation_contract_id: GT-G1-02R-INGRESS-ECC-v0.1`

The exact-source provider artifact and final evidence-only materialization must
compile exactly six package evidence files under:

`verification/evidence/gates/G1/<source_ref>/GT-G1-02R-INGRESS/`

1. `INGRESS-PLAN.json` — package/task anchor, actual start, exact source, changed-path inventory.
2. `INGRESS-MATRIX.json` — 15 family identities and per-family typed-oracle verdict refs; do not duplicate full test logs.
3. `INGRESS-REGRESSION.json` — negative/preflight, protobuf-off and legacy-decoder regression observations.
4. `INGRESS-CTEST.txt` — exact protobuf ON/OFF commands and process results.
5. `INGRESS-PROVIDER.json` — exact hosted run/attempt/job/artifact identities and exact tested source SHA.
6. `INGRESS-GATE-MANIFEST.json` — inventory/hash/ref binding for the five inputs above.

The evidence generator/test must reject wrong task ID, wrong anchor/source,
missing family rows, wrong inventory, source/materialized provenance mismatch,
or historical `GT-G1-02R` namespace reuse.

Machine facts owned by CTest/provider outputs are referenced rather than manually
re-authored as competing totals.

## 11. Performance / safety constraints

- DTO-to-domain reconstruction must be O(payload size) apart from already-authorized existing normalization behavior.
- No unbounded recursion, hidden retry loop or second full parse of arbitrary payloads may be added without demonstrating necessity.
- Existing preflight size/count/depth limits remain authoritative and must not be weakened.
- No generated protobuf type enters public semantic headers.
- No new heap-heavy copy layer should be introduced when existing mapper helpers can move/copy directly into the domain structs.

## 12. Exact verification commands / provider obligations

P32 must record the actual commands used. At minimum execution must cover:

- focused `canvas_semantic_codec_operation_ingress_test` with protobuf ON
- existing `canvas_semantic_codec_test`
- relevant existing semantic conformance/Operation tests needed for T-I02
- full semantic CTest protobuf ON
- full semantic CTest protobuf OFF
- `python3 -m unittest verification.tests.test_g1_02r_ingress_evidence -v`
- `git diff --check`
- clean tracked source state after exact-source verification and before evidence-only materialization
- hosted exact-source `G1 Semantic Codec` workflow, or an equivalently exact workflow only if that existing workflow cannot produce the required package evidence

## 13. Exact-source / materialization contract

P32 return must distinguish source from evidence materialization:

```yaml
source_ref: REQUIRED_IMMUTABLE_SOURCE_COMMIT
materialized_ref: REQUIRED_REVIEWER_ACCESSIBLE_EVIDENCE_ONLY_DESCENDANT
source_to_materialized_relation: EVIDENCE_ONLY_DESCENDANT
```

`source_ref` must contain only authorized source changes. `materialized_ref` may
add only the six evidence files above. The source must not depend on its own
later evidence commit.

## 14. Blocked return behavior

Return without widening scope when any of the following occurs:

```yaml
BLOCKED_REPOSITORY_IDENTITY:
  when: repository/package materialization cannot be resolved exactly
BLOCKED_EXECUTION_DIVERGENCE:
  when: task anchor ancestry or descendant delta cannot be safely reconciled
BLOCKED_AUTHORITY:
  when: complete mapping requires an unresolved semantic/defaulting/schema decision
BLOCKED_SCOPE:
  when: a required source change falls outside the exact path contract
BLOCKED_TEST_ORACLE:
  when: an independent 15-family typed oracle cannot be constructed from frozen Authority/schema
BLOCKED_ENVIRONMENT:
  when: protobuf/toolchain/provider environment cannot run the required matrix
BLOCKED_EVIDENCE:
  when: exact source succeeds but provider/evidence-only reviewer materialization cannot be produced
```

Do not redesign Authority or reopen GT-G1-03 through GT-G1-06 inside P32.

## 15. P31 exit state

```yaml
P31_status: COMPLETE
package_boundary: ONE_CAPABILITY_PACKAGE
P32_status: NOT_AUTHORIZED
next_owner: aegis-implementation
next_stage: P32_IMPLEMENTATION_AUTHORIZATION
preferred_executor: codex
preferred_execution_ref: codex/gt-g1-02r-ingress
P34_status: NOT_STARTED
GT_G1_07_primary_gate: SUSPENDED
GT_G1_08_authorized: false
```

P31 completion does not execute P32, create the execution branch, mutate runtime
source/tests/build files, or claim any Gate result.

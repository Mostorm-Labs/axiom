# Implementation Contract Skeleton v0.2

> Status: **Current Derived Materialization Specification / Repo Blueprint — NOT Verification Evidence**
>
> Notion source: `3c84c57a-590c-81d8-b892-fb484ee170aa`
>
> Supersedes the stale pre-CBR Skeleton v0.1 as the current materialization target.

## Source authority

Materialization MUST consume only:

```text
ICF-00 v0.3
ICF-01 v0.1
ICF-02 v0.2
ICF-03 v0.2
ICF-04 v0.2
ICF-05 v0.1
```

Superseded pages may be used for provenance/diff only.

## Logical repo target

```text
contracts/
  idl/
    axiom.base.v1.yaml
    axiom_data_bridge.v1.yaml
    axiom_platform_runtime.v1.yaml
    canvas_document_binding.v1.yaml
    presented_feedback.v1.yaml
    document_close.v1.yaml
  generated/
    authority-manifest.json
    normalized-idl.json
    fingerprints.json
    abi-shape.lp64.json
    function-table.v1.json
    enum-registry.v1.json
    ts-binding.snapshot.d.ts
    behavior-vector-manifest.json

native/axiom/include/axiom/contracts/
native/axiom/include/axiom/cpp/
packages/axiom-contracts-ts/src/generated/
packages/axiom-contracts-ts/src/adapters/
tools/contracts/
tests/contracts/{source_closure,abi,generated_ts,projection,behavior}
```

Physical paths may adapt to repo conventions, but the logical artifacts and ownership must remain equivalent.

## Projection ownership

Exactly three standalone native API families:

```text
AxiomDataBridgeApiV1
AxiomPlatformRuntimeApiV1
AxiomCanvasDocumentApiV1
```

`presented_feedback` generates types only and embeds into PlatformRuntime ingress. `document_close` reuses DataBridge close functions and MUST NOT generate a second API table.

## Expected ABI comparison oracle

These historical LP64 values are comparison targets, **not current evidence**:

```text
AxiomId128                       16
AxiomByteSpan                    16
AxiomOwnedBuffer                 24
AxiomCanonicalCommitStamp        16
AxiomOperationPacketV1           40
AxiomApplyItemResultV1           56
AxiomSnapshotPacketV1            48
AxiomCloseFenceV1                24
AxiomPresentedFeedbackV1         56
AxiomCanonicalHostSlotDescV1     40
AxiomSurfaceMetricsV1           112
AxiomFrameTickV1                 40
AxiomCanvasDocumentAttachDescV1  40
AxiomDataBridgeApiV1             88
AxiomPlatformRuntimeApiV1        72
AxiomCanvasDocumentApiV1         24
```

Current generation must emit its own sizeof/offsetof/function/enum manifests and compare them. Unexpected current V1 shape changes are BLOCKED pending classification; do not auto-update goldens.

## Ordering

Normalized machine source preserves concrete paths `input.document`, `input.canvas`, `input.binding`. No stage may canonicalize these back to type names. Detach specifically normalizes to `input.binding` while runtime resolves that handle to the owning Canvas record before mutation.

## TypeScript boundary

Generator owns low-level binding types/mappings and async bridge primitives. Hand-written adapters own ergonomic Platform Host and BindingController orchestration. Required proof is adapter conformance, not identical source text.

## Required behavioral hooks

```text
binding_stale_handle_isolation
binding_generation_switch_order
presented_live_metrics_guard
presented_visibility_guard
presented_old_surface_guard
presented_destroyed_view_guard
presented_approximate_no_clear
data_bridge_external_no_echo
data_bridge_already_applied_no_ordinal
close_publication_barrier
```

Test-only observation hooks do not enter public product ABI.

## Fingerprints

Emit independently:

```text
AuthoritySetFingerprint
NormalizedIDLHash
CAbiShapeHash
GeneratedTSBindingHash
BehaviorVectorHash
```

Expected transition from stale v0.1 materialization: authority, normalized IDL and behavior fingerprints change; C ABI shape is expected unchanged but must be proven; generated TS change is allowed only with reviewed diff.

## Deterministic pipeline

```text
resolve current authority set
-> normalize IDL
-> lint source closure / projection / ordering
-> generate C / C++ / low-level TS
-> emit ABI + enum + function-table manifests
-> emit fingerprints
-> verify generated tree reproducible/clean
-> compile native + TS
-> run behavioral vectors
```

No blocking CI path may auto-bless an unexpected ABI or behavior diff.

Current state: **Blueprint Ready / Repo Evidence Pending**.

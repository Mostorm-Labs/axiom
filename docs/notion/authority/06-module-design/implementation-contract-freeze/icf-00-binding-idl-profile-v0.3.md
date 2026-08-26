# ICF-00 Binding IDL Profile + First-Batch IDL Bundle v0.3

> Status: **Current Proposed Freeze Candidate — Deterministic Codegen / Projection Closure**
>
> Notion source: `3c84c57a-590c-812b-882f-d65b965373c7`
>
> Supersedes: ICF-00 v0.2 and v0.1.

## Exact machine source set

```text
ICF-00 v0.3 — machine IDL/profile/projection authority
ICF-01 v0.1 — DataBridge semantic + ABI authority
ICF-02 v0.2 — Platform Runtime semantic + ABI/enum authority
ICF-03 v0.2 — CanvasDocumentBinding semantic/ordering authority
ICF-04 v0.2 — PresentedFeedback + CanonicalVisible predicate authority
ICF-05 v0.1 — Document Close semantic/barrier authority
```

Generator/reviewer MUST NOT consume Superseded ICF versions as normative input.

## Projection model

Each contract declares:

```yaml
projection:
  api_surface: standalone | embedded | reuse
  embedded_in: optional contract.method
  ts_layer: generated_binding | none
```

First Batch is fixed as:

```text
axiom.data_bridge             -> standalone
axiom.platform_runtime        -> standalone
axiom.canvas_document_binding -> standalone
axiom.presented_feedback      -> embedded in axiom.platform_runtime.presented_feedback
axiom.document_close          -> reuse axiom.data_bridge begin/await/destroy
```

Exactly three standalone native API families therefore exist: `AxiomDataBridgeApiV1`, `AxiomPlatformRuntimeApiV1`, `AxiomCanvasDocumentApiV1`.

## Canonical ordering annotation

Machine source only accepts concrete input field paths:

```yaml
ordered_by: input.document
ordered_by: input.canvas
ordered_by: input.binding
```

Type-name inference is forbidden. Lint fails when the field does not exist, is not an allowed key, leaves method input, or requires type inference.

Mappings:

- DataBridge restore/createSnapshot/apply/beginClose/awaitPublication -> `input.document`
- Platform bind/unbind/metrics/state/frameTick/surfaceLost/deviceLost -> `input.canvas`
- CanvasDocumentBinding attach -> `input.canvas`
- CanvasDocumentBinding detach -> `input.binding`
- PresentedFeedback uses `input.feedback.canvas` as routing/correlation, not a fake completion-order contract.

**Resolution rule:** if a child page contains an older type-name-only IDL excerpt, this v0.3 machine canonical form wins for codegen.

## Required type closure

Current machine bundle must close all referenced base/DataBridge/Platform/Binding/Presented/Close types without historical lookup. Key stable registries include:

- ApplyDisposition: APPLIED=1, ALREADY_APPLIED=2, REJECTED=3.
- PlatformBindingKind: WINDOWS=1, ANDROID=2, APPLE=3, WEB=4.
- SurfaceVisibility: VISIBLE=1, HIDDEN=2, OCCLUDED=3; 0 invalid/reserved.
- PresentationState: RUNNING=1, SUSPENDED=2; 0 invalid/reserved.
- DeviceLossReason: u32 diagnostic; 0 UNKNOWN; non-zero common taxonomy not frozen.
- DetachReason: SWITCH_DOCUMENT=1, CANVAS_DESTROY=2, DOCUMENT_CLOSE=3, HOST_UNMOUNT=4.
- PresentOutcome: PRESENTED=1, DROPPED=2, FAILED=3, UNKNOWN=4.
- PresentEvidenceKind: PLATFORM_QUALIFIED=1, APPROXIMATE=2, NONE=3.

## Generated TypeScript boundary

Generator owns low-level binding declarations, numeric enum mappings, handle/u64/byte ownership projection and async bridge primitives. It does not generate the Product Public Runtime Facade, ergonomic Platform Host wrappers, BindingController orchestration or Shared Data Runtime DocumentSession API. Hand-written wrappers require adapter conformance and may not change lifetime/ownership/error semantics.

## Required fingerprints

Do not collapse compatibility into one hash. Emit separately:

- `AuthoritySetFingerprint`
- `NormalizedIDLHash`
- `CAbiShapeHash`
- `GeneratedTSBindingHash`
- `BehaviorVectorHash`

Ordering/behavior revisions may change normalized/behavior hashes without changing C ABI shape. That expectation must be proven, not assumed.

## MUST-fail examples

- unresolved current type/import;
- needing a Superseded page for closure;
- type-name-only `ordered_by`;
- embedded/reuse contract generating standalone API symbols;
- duplicate PresentedFeedback ingress or DocumentClose API;
- ambiguous enum numeric mapping;
- SurfaceVisibility 0/unknown treated as Visible;
- legacy `Transaction*`, server cursor/revision, Scene/Tile/GPU leakage into DataBridge.

# Contract Linter / Golden Matrix v0.2

> Status: **Current Verification Specification — NOT Current Verification Evidence**
>
> Notion source: `3c84c57a-590c-8179-9d56-e8ec2cf33e91`

This document defines what current rematerialization must prove. Without actual command output, generated manifests and CI artifacts, it MUST NOT be cited as evidence that the current contract passes.

## Authority under test

Implementation Contract Skeleton v0.2 derived from exact current set:

```text
ICF-00 v0.3
ICF-01 v0.1
ICF-02 v0.2
ICF-03 v0.2
ICF-04 v0.2
ICF-05 v0.1
```

Historical Skeleton/Compile Review is comparison-only.

## Gate groups

| Gate | Proves | Blocking |
|---|---|---|
| L1 Source Closure | exact current source is self-contained and historical-free | yes |
| L2 Projection / Ordering | projection routing and `input.*` ordering mechanically resolve | yes |
| G1 C ABI Golden | layout/enum/function prefix compatibility | yes |
| G2 Generated TS Golden | deterministic low-level TypeScript projection | yes |
| B1 Behavioral Vectors | stale/order/no-echo/close/presentation semantics | yes |
| R1 Reproducibility | deterministic clean generation; no auto-bless | yes |
| T1 Target Binding | WASM/JSI/native target compile | required for Freeze promotion |

## L1 Source Closure

Must reject unresolved types/imports, Superseded/Historical/Stale generator inputs, ambiguous numeric mappings, missing handle/lifetime/delivery annotations, `Transaction*` vocabulary and server/Scene/Tile/GPU leakage into DataBridge.

Required negative fixtures include missing type, superseded source manifest, duplicate enum mapping, missing async lifetime and legacy Transaction symbol.

## L2 Projection / Ordering

Only concrete `ordered_by: input.<field>` is accepted for current machine source. Type inference such as `ordered_by: CanvasHandle` fails.

Required mappings include:

- DataBridge same-document operations -> `input.document`
- Platform canvas control -> `input.canvas`
- binding attach -> `input.canvas`
- binding detach -> `input.binding`

Expected standalone API tables:

```text
AxiomDataBridgeApiV1
AxiomPlatformRuntimeApiV1
AxiomCanvasDocumentApiV1
```

Forbidden generated standalone families:

```text
AxiomPresentedFeedbackApiV1
AxiomDocumentCloseApiV1
```

## G1 C ABI Golden

Current generation emits its own sizeof/offsetof manifest and compares it against historical V1 comparison values. Any unexpected current binary difference is **BLOCKED**, not automatically blessed.

Enum goldens include:

```text
ApplyDisposition: APPLIED=1 ALREADY_APPLIED=2 REJECTED=3
PlatformBindingKind: WINDOWS=1 ANDROID=2 APPLE=3 WEB=4
SurfaceVisibility: VISIBLE=1 HIDDEN=2 OCCLUDED=3
PresentationState: RUNNING=1 SUSPENDED=2
DetachReason: SWITCH=1 CANVAS_DESTROY=2 DOCUMENT_CLOSE=3 HOST_UNMOUNT=4
PresentOutcome: PRESENTED=1 DROPPED=2 FAILED=3 UNKNOWN=4
PresentEvidenceKind: PLATFORM_QUALIFIED=1 APPROXIMATE=2 NONE=3
```

Visibility 0 MUST NOT map to Visible; PresentationState 0 is invalid/reserved. Pin function pointer order/count for the three standalone API tables.

## G2 Generated TypeScript

Verify low-level handles/u64/Id128/Uint8Array and enum projections, then separately prove adapter conformance from generated platform/binding surfaces to ergonomic Host wrappers. Orchestration methods may be hand-written; lifetime/error/ownership meaning may not change.

## B1 Behavioral vectors

Required vectors:

- BIND-STALE-01: old binding cannot detach new binding.
- BIND-GEN-02: delayed old generation cannot roll visible state back.
- PRESENT-METRICS-01: frame metrics match feedback but not live metrics -> no clear.
- PRESENT-VIS-02: Presented+PlatformQualified while Hidden/Occluded -> no clear.
- PRESENT-FRESH-03: live Visible + live generations + eligible coverage -> clear exactly once.
- PRESENT-SURFACE-04: old SurfaceGeneration feedback ignored.
- PRESENT-DESTROY-05: late feedback after RenderView destruction ignored.
- PRESENT-APPROX-06: Approximate never clears preview.
- DB-NOECHO-01: external/replay APPLIED -> no local operation event.
- DB-IDEMP-02: AlreadyApplied -> no mutation/ChangeSet/ordinal/local event.
- CLOSE-PUB-01: awaitLocalPublication waits for sink callbacks through fence to return.

## Fingerprint expectations

Against stale v0.1:

```text
AuthoritySetFingerprint   CHANGED
NormalizedIDLHash         CHANGED
CAbiShapeHash             EXPECTED SAME — must prove
GeneratedTSBindingHash    REVIEWED CHANGE ALLOWED
BehaviorVectorHash        CHANGED
```

Expectations are not evidence; values come from deterministic repo tooling.

## R1 Reproducibility

```text
clean checkout
-> generate
-> lint
-> generate again
-> byte-for-byte compare generated tree
-> git diff --exit-code on generated artifacts
```

Non-determinism, auto-blessing goldens or unreproducible hand edits are FAIL.

## Verdict model

- PASS = all required gates for the current layer actually executed and passed.
- FAIL = implementation/generated output violates current authority.
- BLOCKED = required evidence/toolchain unavailable or unexpected ABI difference awaits classification.

Forbidden shortcuts: ABI same => whole contract PASS; old PASS => current PASS; absent behavior test => PASS; compile => lifecycle/order semantics proven; unavailable required target => silent SKIP.

Only after L1/L2/G1/G2/B1/R1 PASS may the flow proceed to WASM32, RN/JSI and native host target evidence, then Frozen v1 review.

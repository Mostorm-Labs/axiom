# ICF-02 Axiom Platform Runtime Port v0.2

> Status: **Current Proposed Freeze Candidate — Platform Runtime Representation Closure**
>
> Notion source: `3c84c57a-590c-81a0-a33b-ccb7f5541ee3`
>
> Supersedes: ICF-02 v0.1.

## Change scope

v0.2 closes cross-language representation gaps only. It does not redesign Platform Host/Axiom ownership, method set, SurfaceMetrics field order, async bind/unbind behavior or ABI storage widths.

## SurfaceVisibility

Stable mapping:

```text
VISIBLE  = 1
HIDDEN   = 2
OCCLUDED = 3
0 = invalid/reserved
```

Unknown non-zero values MUST NOT satisfy CanonicalVisible. A wrapper may reject or conservatively treat them as non-visible diagnostic state.

## PresentationState

Stable mapping:

```text
RUNNING   = 1
SUSPENDED = 2
0 = invalid/reserved
```

`Backgrounded` is an application/platform condition, not a third PresentationState. Surface visibility and SurfaceUnavailable/DeviceRecovering remain separate lifecycle axes.

## DeviceLossReason

- stable storage: u32;
- `0 = UNKNOWN`;
- non-zero cross-platform reason taxonomy is not frozen;
- reason is diagnostic/backend-recovery detail only;
- device loss must not roll back SemanticDocument or use snapshot reload as GPU recovery;
- derived GPU resources are rebuilt from current runtime/canonical state.

## ABI rule

Existing V1 binary shape is unchanged. Named typedefs replace semantic ambiguity without changing u32 width. `AxiomSurfaceMetricsV1` field order and `AxiomPlatformRuntimeApiV1` function order/signature count remain unchanged unless a later current authority explicitly versions them.

## Generated TS vs ergonomic host wrapper

IDL generates a low-level binding declaration. Platform/Web host wrappers may expose ergonomic handles/objects and construct the low-level descriptor, but they require conformance mapping and may not create a second Platform Runtime semantics.

## Required conformance

- exact visibility numeric mapping;
- 0/unknown visibility never satisfies CanonicalVisible;
- exact RUNNING/SUSPENDED mapping;
- app background does not invent enum value 3;
- suspend/resume preserves canonical Document state;
- non-zero DeviceLossReason does not trigger semantic rollback/snapshot restore;
- regenerated SurfaceMetrics layout equals the expected historical V1 comparison shape;
- PlatformRuntime function-table V1 prefix remains stable.

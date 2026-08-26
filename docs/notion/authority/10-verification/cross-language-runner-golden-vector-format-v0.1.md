# Cross-language Conformance Runner + Golden Vector File Format v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81a89ac2d3dc36f16787
> Source page id: `3c44c57a-590c-81a8-9ac2-d3dc36f16787`
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Verification Tooling Contract
> MR-10-03 note: first-divergence runner details expanded from the source page; no authority status promotion.

## Boundary

Verification artifact/tooling contract only. Axiom semantic apply authority remains C++ core. WASM is the same core compiled for Web, not a second semantic authority. Shared TS Data Runtime continues to treat canonical payload as opaque; TS participates only in capabilities it explicitly implements. Verification projection/opstream/result JSON is not Product storage, sync protocol or public ABI.

## Topology

```text
Golden Corpus
    ↓
axiom-conformance coordinator
    ↓
C++ / TS / WASM adapters
    ↓
ImplementationObservation
    ↓
coordinator golden + cross-implementation comparison
    ↓
ConformanceResult
    ↓
checkpoint + binary-search first divergence
```

Adapters report observed facts only; coordinator owns PASS/FAIL.

## Logical repository contract

One corpus root; stable case IDs; case-local relative artifacts; shared schemas; coordinator; language adapters; output directory. Physical path may follow repo convention, but responsibilities remain separated.

## Path/artifact rules

All case paths are case-relative POSIX; no absolute paths or `..` escape; no authority artifact requiring symlink; binary truth is raw binary, not base64 JSON; JSON is metadata/projection/observation/result/diagnostic; UTF-8 no BOM; case-sensitive corpus naming. Updating artifact content under an existing ID requires explicit review of whether it fixes bad golden or changes authority.

## `corpus.json`

Carries `formatVersion`, stable `corpusId`, `semanticSchemaVersion`, `referenceCodec`, `projectionFormat`, discovery roots and `requiredRunnerProtocolVersion`. Run/device/build metadata belongs to execution `run.json`, not corpus authority.

## `case.json`

Stable fields: formatVersion, id, title, requirement status, category, authorityRefs, entrypoint, requiredCapabilities, input, expected, capture, blockedByOpenPolicy, notes. Status vocabulary is exactly `SPEC_REQUIREMENT / FREEZE_CANDIDATE / BENCHMARK_TARGET / EXPERIMENTAL_TARGET / OPEN`; execution PASS is not a requirement status.

Entrypoints: DECODE, NORMALIZE, VALIDATE, ENCODE, APPLY, REPLAY, ORDER_KEY_COMPARE, ORDER_KEY_ALLOCATE.

Input kinds: encoded value/operation; semantic fixture; replay stream (snapshot + operations); algorithm fixture. Expected accepted/rejected/OPEN records specify only facts supported by upstream authority. Rejected apply may reference unchanged projection to prove no partial mutation.

## Verification projection v1

Verification-only exact semantic view. Stable Id128 is lowercase tagged hex; bytes tagged hex; f64 exact normalized IEEE bits; later machine-readable closure adds exact f32 and u64 tags. Absent optional field means key absent; oneof only active member; OrderedSequence retains order; canonical sets/maps are normalized arrays/entry arrays. Projection is not Product JSON.

## Operation stream framing

Verification-only `AXOPSTR1` framing: 8-byte magic, u32 little-endian frame count, then repeated u32 little-endian payload length + Operation protobuf bytes. Frame order is replay order and is never sorted. This framing must not be reused as Sync/AXTP transport framing.

## Runner protocol

Adapters expose capability metadata and deterministic observations. Coordinator validates schema/capabilities, dispatches selected cases, captures stage observations, compares expected and cross-implementation results, emits first divergence and stable machine-readable result. Unsupported capability is explicit, not false PASS.

Replay diagnostic adds:

```text
<adapter> run-case ... --stop-after-operation <N>
```

`--stop-after-operation N` means apply Operation indices `[0..N]`, then emit the requested projection/checkpoint. This capability exists specifically so first divergence can be localized without changing the canonical replay order.

Coordinator implementation language is not frozen. Tooling language must not become architecture authority.

## First Divergence Algorithm — Freeze Candidate

### Single value / single Operation

When stage capture is enabled, compare the fixed semantic stage sequence:

```text
DECODE → NORMALIZE → VALIDATE → APPLY → PROJECTION → ENCODE
```

The earliest mismatch stage is the first divergence.

### Short replay

With `EVERY_OPERATION` checkpoints, compare checkpoints in Operation order. The first different checkpoint is the first differing Operation index.

### Long replay

Long streams use two-level localization:

```text
A. Compare coarse checkpoints
   ↓
B. Find first mismatching interval [L, R]
   ↓
C. Re-run adapters with --stop-after-operation M
   ↓
D. Binary search first mismatch index
   ↓
E. Capture detailed stage artifacts around that Operation
```

Source pseudocode:

```text
lo = last_matching_checkpoint + 1
hi = first_mismatching_checkpoint
while lo < hi:
    mid = floor((lo + hi) / 2)
    replay prefix through mid on compared implementations
    if projections agree with golden/reference at mid:
        lo = mid + 1
    else:
        hi = mid
first = lo
```

Deterministic replay is a prerequisite. Bisection must not reorder, skip, merge or reinterpret canonical Operations.

### OPEN / Experimental comparison

If no golden semantic winner exists but implementations diverge, the same replay localization may be used. The result is `OBSERVED_DIVERGENCE_OPEN`; no compared implementation is promoted to expected authority.

## Divergence representation evolution

The older Runner example used `expectedArtifact` plus `actualArtifacts`. The later 10-04 machine-readable authority normalizes this to `basis + reference? + observed[]`. For MR-10-03, **10-04 is authoritative for the current DivergenceRecord field shape**, while this page remains authoritative for runner/replay localization behavior. This is an explicit supersession relationship, not a conflict.

## Golden trust rule

No blocking runner path may auto-bless or update expected from current implementation output. Captured candidates are non-authoritative until reviewed/promoted through corpus governance.

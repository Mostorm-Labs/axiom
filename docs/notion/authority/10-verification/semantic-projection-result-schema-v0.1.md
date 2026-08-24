# Semantic Projection Schema + Conformance Result JSON Schema v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8165a197c6f50e1434d3
> Source page id: `3c44c57a-590c-8165-a197-c6f50e1434d3`
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Machine-readable Verification Contract
> Migration note: expanded during MR-10-03 to preserve the source first-divergence contract at implementation-useful fidelity.

## Scope

This authority freezes **verification artifact / tooling schema only**. It does not change Product wire, Axiom public ABI, Snapshot storage, Shared Data Runtime, Sync/AXTP, 04 semantic equality, or 07 runtime semantics.

Adapter and coordinator ownership remains strict:

```text
case.json
   ↓
adapter
   ↓
ImplementationObservation v1      # observed facts only
   ↓
coordinator comparator
   ↓
ConformanceResult v1
   └── DivergenceRecord v1 when needed
```

Adapters must not declare their own semantic PASS/FAIL. Coordinator judgment is based on golden authority and/or cross-implementation evidence.

## Machine schema baseline

Verification JSON uses JSON Schema Draft 2020-12. Top-level contracts carry `formatVersion = 1`, a fixed format identifier, and default `additionalProperties: false`. Unknown machine-readable fields are not silently ignored.

Relevant schema identifiers:

- `urn:auditoryworks:axiom:verification:projection:v1`
- `urn:auditoryworks:axiom:verification:observation:v1`
- `urn:auditoryworks:axiom:verification:result:v1`

Artifacts are UTF-8 without BOM. Checked-in golden JSON uses LF, two-space indentation and final newline. JSON object member order is not semantic; array order is semantic. Artifact references are run-relative POSIX paths and may not be absolute or escape through `..`.

## Shared verification enums

Requirement status:

`SPEC_REQUIREMENT / FREEZE_CANDIDATE / BENCHMARK_TARGET / EXPERIMENTAL_TARGET / OPEN`

Implementation kind:

`CPP_NATIVE / WASM / TS_REFERENCE`

Stage names:

`DECODE / NORMALIZE / VALIDATE / APPLY / PROJECTION / ENCODE / REPLAY / ORDER_KEY_COMPARE / ORDER_KEY_ALLOCATE / HARNESS`

For single-value / single-Operation first divergence, the semantic stage order is fixed:

```text
DECODE → NORMALIZE → VALIDATE → APPLY → PROJECTION → ENCODE
```

`REPLAY` is an orchestration stage. A replay divergence should still narrow to an Operation index and semantic stage when evidence permits.

## Projection v1 contract relevant to divergence

Projection envelope fields are:

- `format = axiom-verification-projection-v1`
- `formatVersion = 1`
- `semanticSchemaVersion`
- `rootType`
- `form = DECODED | NORMALIZED | CANONICAL`
- `value`

Projection validation is two-layer:

1. generic JSON Schema validates envelope / JSON shape;
2. IDL-aware validation resolves `rootType` against the frozen Reference IDL and validates field types, presence, oneof and form-specific canonical rules.

Canonical scalar projection uses exact tagged representations. In the current source authority this includes:

- u64/fixed64: `u64:` + 16 lowercase hex digits;
- f32: `f32:` + 8 lowercase IEEE-754 bits;
- f64: `f64:` + 16 lowercase IEEE-754 bits;
- Id128: `id128:` + 32 lowercase hex digits;
- arbitrary bytes / OrderKey: `hex:` + even-count lowercase hex;
- u32 / enums / registry IDs: JSON integer;
- bool: JSON boolean;
- strings: JSON string.

`NORMALIZED / CANONICAL` apply Common Wire normalization: `-0 → +0`, NaN/±Infinity forbidden, f32 width is preserved, and semantic equality is exact after normalization rather than epsilon-based.

Presence and collections remain semantic: absent optional = missing key, not null; present(default) remains present; canonical oneof has only its active member; OrderedSequence is order-sensitive; CanonicalSet / CanonicalMap use authority-defined ordering and uniqueness.

## DivergenceRecord v1

The older Runner example used `expectedArtifact + actualArtifacts`. The current source normalizes that into `basis + reference? + observed[]` so OPEN/cross-implementation evidence does not invent a fake expected implementation.

Representative shape:

```json
{
  "kind": "SEMANTIC_PROJECTION",
  "basis": "GOLDEN",
  "stage": "APPLY",
  "operationIndex": 42,
  "operationId": "id128:0000000000000000000000000000002a",
  "semanticPath": "$.placement.parentId",
  "reference": {
    "source": "GOLDEN",
    "artifact": "expected/final.projection.json"
  },
  "observed": [
    {
      "implementationId": "axiom-cpp-native",
      "artifact": "observations/cpp/final.projection.json"
    }
  ],
  "summary": "parentId differs after SetPlacements"
}
```

### Divergence kinds

- `TERMINAL_STAGE`
- `OUTCOME`
- `SEMANTIC_ERROR_CATEGORY`
- `STAGE_PROJECTION`
- `SEMANTIC_PROJECTION`
- `CANONICAL_BYTES`
- `REPLAY_CHECKPOINT`
- `CAPABILITY`
- `IMPLEMENTATION_ERROR`

### Comparison basis

`GOLDEN` and `CROSS_IMPLEMENTATION` are distinct.

Rules:

- `basis = GOLDEN` → `reference` is required and `reference.source = GOLDEN`;
- `basis = CROSS_IMPLEMENTATION` → `reference` is absent and `observed[]` contains at least two implementations;
- OPEN cross-implementation divergence uses `CROSS_IMPLEMENTATION`; coordinator must not choose cpp/wasm/ts as a synthetic expected implementation.

### Location fields

- `operationIndex?` — zero-based opstream frame index;
- `operationId?` — tagged Id128 of that Operation;
- `semanticPath?` — typed verification semantic path;
- `byteOffset?` — zero-based byte offset for canonical-byte comparison.

If an Operation/replay divergence is localized to a concrete operation, `operationIndex` and `operationId` appear together. Semantic projection mismatch should report `semanticPath` when it can be localized. Canonical-byte mismatch should report `byteOffset` when it can be localized. Terminal/capability errors may have no operation location.

### Operand rules

A comparison operand contains at least one of `artifact` or scalar `value`. Scalar `value` is for compact terminal stage / outcome / semantic error-category evidence. Projection and canonical-byte divergence should prefer artifact references instead of embedding full projection data inside `DivergenceRecord`.

## Semantic Path Grammar v1

The source defines one language-neutral diagnostic grammar so C++ / TS / other coordinator implementations do not invent different first-difference paths:

```text
$                         root projection value
.field                    struct field
[N]                       OrderedSequence zero-based index
[key=value]               CanonicalSet / CanonicalMap selector
```

Examples:

```text
$.placement.parentId
$.objects[id=id128:00000000000000000000000000000011].transform.tx
$.patches[objectId=id128:...,fieldId=42]
```

The path is verification diagnostics only. It must not contain host-language pointers, C++ addresses or generated-protobuf field indices and must not enter Axiom public API.

## First-divergence comparison order

The coordinator comparison order is authority, not an implementation preference:

```text
0 Harness / corpus validity
1 Capability availability
2 Terminal stage
3 Accepted vs Rejected outcome
4 Semantic error category when upstream authority specifies it
5 Stage projection when captured
6 Final semantic projection
7 Canonical protobuf bytes when required
8 Replay checkpoints
```

For a single Operation, find the first mismatch using the fixed semantic stage order.

For short replay with `EVERY_OPERATION` checkpoints, the first differing checkpoint index is the first differing Operation index.

For long replay, coarse checkpoints only narrow the interval. The runner then re-executes with `--stop-after-operation M` and binary-searches to the first mismatching Operation. If final projection differs but coarse checkpoints did not identify the concrete first Operation, `diagnose --first-divergence` must re-run localization; reporting only `final mismatch` is insufficient.

Deterministic replay is a prerequisite for this localization method.

OPEN / Experimental cases may use the same localization process across implementations. The result remains observation (`OBSERVED_DIVERGENCE_OPEN`) and does not choose a semantic winner.

## Result invariants

At minimum:

- `PASS` → `divergence = null`;
- `FAIL_GOLDEN_MISMATCH` → `divergence.basis = GOLDEN`;
- `OBSERVED_DIVERGENCE_OPEN` → `divergence.basis = CROSS_IMPLEMENTATION`;
- `BLOCKED_OPEN` / `OBSERVED_AGREEMENT_OPEN` → no divergence;
- required participant/capability failures remain explicit;
- implementation/harness errors do not masquerade as semantic mismatches.

Diagnostics may contain implementation-specific text, but diagnostic text is not semantic authority and does not replace stable `DivergenceRecord` fields.

## Validation layering

A trustworthy result is not established by `JSON Schema validate=true` alone. The verification pipeline is:

```text
JSON Schema structural validation
        ↓
IDL-aware projection validation
        ↓
cross-artifact semantic validation
        ↓
coordinator comparison
        ↓
ConformanceResult + deterministic DivergenceRecord
```

MR-10-01 materialized the first three schema/IDL boundaries. MR-10-03 owns the repo-local lock of the first-divergence result contract; it does not require implementing the complete 60-case semantic engine or all adapters during authority migration.

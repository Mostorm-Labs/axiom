# Cross-language Conformance Runner + Golden Vector File Format v0.1

> Source page: https://app.notion.com/p/3c44c57a590c81a89ac2d3dc36f16787
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Verification Tooling Contract

## Boundary

Verification artifact/tooling contract only. Axiom semantic apply authority remains C++ core. WASM is the same core compiled for Web, not a second semantic authority. Shared TS Data Runtime continues to treat canonical payload as opaque; TS participates only in capabilities it explicitly implements. Verification projection/opstream/result JSON is not Product storage, sync protocol or public ABI.

## Topology

Golden Corpus → coordinator → C++/TS/WASM adapters → ImplementationObservation → coordinator comparator → ConformanceResult + divergence artifacts. Adapters report observed facts only; coordinator owns PASS/FAIL.

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

## First divergence

Divergence should identify case, implementation, stage, replay operation/checkpoint where relevant, semantic path or byte offset, expected vs observed and artifact references. Long replay uses checkpoints and narrowing rather than reporting only final mismatch.

## Golden trust rule

No blocking runner path may auto-bless or update expected from current implementation output. Captured candidates are non-authoritative until reviewed/promoted through corpus governance.

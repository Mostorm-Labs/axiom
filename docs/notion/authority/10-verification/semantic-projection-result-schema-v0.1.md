# Semantic Projection Schema + Conformance Result JSON Schema v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8165a197c6f50e1434d3
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — Machine-readable Verification Contract

## Scope

Field-level machine contract for `axiom-verification-projection-v1`, `ImplementationObservation`, `ConformanceResult` and `DivergenceRecord`. Verification-only; not Product wire, ABI, Snapshot storage, Data Runtime or Sync/AXTP.

## Schema baseline

JSON Schema Draft 2020-12. Top-level contracts carry `formatVersion=1`, fixed format identifier and default `additionalProperties:false`; unknown machine fields are not silently ignored.

URN identifiers include:

- `urn:auditoryworks:axiom:verification:projection:v1`
- `urn:auditoryworks:axiom:verification:observation:v1`
- `urn:auditoryworks:axiom:verification:result:v1`

UTF-8 no BOM; checked-in JSON LF, two-space indent, final newline. Artifact paths are run-relative POSIX, never absolute or escaping via `..`. Object member order is not semantic; array order is.

## Shared enums

Requirement status: SPEC_REQUIREMENT, FREEZE_CANDIDATE, BENCHMARK_TARGET, EXPERIMENTAL_TARGET, OPEN.

Implementation kind: CPP_NATIVE, WASM, TS_REFERENCE.

Stages: DECODE, NORMALIZE, VALIDATE, APPLY, PROJECTION, ENCODE, REPLAY, ORDER_KEY_COMPARE, ORDER_KEY_ALLOCATE, HARNESS. Semantic stage order is Decode→Normalize→Validate→Apply→Projection→Encode; Replay orchestrates operation/checkpoint location.

Observation outcomes: ACCEPTED, REJECTED, NOT_SUPPORTED, HARNESS_ERROR, INTERNAL_ERROR. These are verification enums, not Axiom public errors.

Semantic error categories are populated only where upstream freezes them, including InvalidEnvelope, UnsupportedSchemaVersion/PayloadVersion, InvalidPayload/ObjectKind/Field/Reference, InvariantViolation, ObjectAlreadyExists/NotFound and ProtocolCorruption. Library exceptions remain diagnostics.

## Projection envelope

Required fields: `format=axiom-verification-projection-v1`, `formatVersion=1`, semanticSchemaVersion, rootType, form (`DECODED/NORMALIZED/CANONICAL`), value.

Exact scalar mapping: bool JSON boolean; u32/enum/registry ID JSON integer; u64/fixed64 `u64:` + 16 lowercase hex; f32 `f32:` + 8 lowercase IEEE bits; f64 `f64:` + 16 lowercase bits; Id128 `id128:` + 32 lowercase hex; arbitrary bytes/OrderKey `hex:`; strings ordinary JSON strings.

DECODED may retain raw -0/NaN/Inf if decoder produced them. NORMALIZED/CANONICAL requires -0→+0 and forbids NaN/Inf; f32 width must not silently widen to f64.

Optional absence is key absence, not null. Present(default) remains present. oneof exposes only active member in canonical form. OrderedSequence is array in semantic order; CanonicalSet is sorted unique array; CanonicalMap is ordered entry array; keyed batches/PropertyBag/EraseMask follow authority comparators.

Projection validation is two-layer: generic JSON Schema for envelope/shape plus IDL-aware validator for rootType field typing, form legality and canonical collection rules.

## Observation vs Result

Adapter produces ImplementationObservation only. Coordinator validates observation and compares it against golden/cross-implementation evidence to produce ConformanceResult. Result carries stable case/implementation identity, requirement status, execution status, expected/observed outcome, artifact refs and optional first divergence. Agreement among implementations cannot override golden mismatch.

## Divergence

Divergence is machine-readable and identifies earliest meaningful stage/checkpoint/operation/path or byte offset plus expected/observed references. Diagnostics can contain implementation-specific detail but do not redefine semantic error authority.

# Platform Harness Implementation Plan + CI Job Wiring v0.1

> Source page: https://app.notion.com/p/3c44c57a590c818ea1f3d795ed42136a
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — G3 Platform Harness Implementation / CI Wiring Plan

## Implementation layers

Corpus / Shared Tooling / Platform Test Host. Corpus does not depend on Runtime; shared tooling does not depend on a concrete OS; only platform test host links real Axiom/platform integration.

`protocol-seed-v0.1` 56 vectors is prerequisite trusted-root job for platform G3. Verification-only hooks/hosts never enter Product public ABI. Shared CLI drives protocol and platform conformance; adapters do not implement corpus parser/WAIT/comparator/PASS policy. CI provider is not authority.

TS/Node first reference Shared Runner/CLI is Experimental Target.

## Logical repo packages

Schemas for semantic/platform/protocol; platform protocol vectors and 28 scenarios; packages for harness protocol, runner, scripted adapter, transport and platform-conformance CLI; native verification hooks/common host/platform-specific hosts; generated output outside golden authority.

Workspace package manager remains repo-convention-selected/OPEN.

## Package boundaries

Protocol package: types/schema loader/stable enums/tagged values/path helpers only; no mutable runner state/platform API/expected comparator.

Runner package: handshake/codec/schema validation/registries/sequencers/fault/fence/wait/capture/observation/comparator/aggregator/diagnostics/artifact writer.

Scripted adapter: protocol meta-tests only, deliberately malformed behavior, no Axiom linkage.

Transport: in-process and serialized-loopback minimum; transport metadata not semantic comparison.

CLI: validate, protocol, list, profile, run, compare, aggregate. No bless/update-golden.

## Native build boundary

Production dependency remains contract→semantic→scene→render→platform. Verification is a test-only downstream consumer: production targets → verification hooks → verification host common → platform-specific test host. `production target → verification target` is forbidden; reverse is allowed.

`axiom_verification_platform_hooks` logical target provides deterministic SurfaceLost/DeviceLost ingress, present completion hold/release, stale-generation probe, SourceLease/SourceAttempt instrumentation and event tap. It invokes production normalized paths rather than implementing recovery.

Common verification host bridges harness protocol IDs/profile/capability to native test seam and does not own scenario parser/comparator.

## Platform hosts

Web: WASM verification build + browser driver. Windows: minimal native host. Android: Activity/View/JNI instrumentation host. Apple: UIView/ObjC++ XCTest-style host. Each emits normalized observation/result for applicable scenarios; unsupported is explicit.

## CI DAG

Logical dependency: schema/workspace → protocol package/runner → 56 protocol seed → protocol CI trusted root → verification hooks + 28 scenario materialization → platform adapters → PR aggregate; nightly/release fan out across available platform families. Platform jobs do not claim trustworthy PASS if protocol seed failed.

## Repo grounding

Source page originally noted repo-path reconciliation was required. In current migration branch the authority snapshot lives under `docs/notion/authority/10-verification`; physical implementation paths remain subject to G0 IH-00 mapping, while dependency and ownership rules above are authoritative candidates.

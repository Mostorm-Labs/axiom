# 05 Runtime Capability Architecture — Authority Index

> Source root: Notion `05 Runtime Capability Architecture`
>
> Current root baseline: `Axiom Architecture Baseline v0.4`
>
> Snapshot date: 2026-08-26

## Start here

Repository/Codex authority resolution starts from `docs/notion/manifest.yaml`, then this layer uses:

1. `axiom-architecture-baseline-v0.4.md` for the current cross-layer architecture baseline and routing rules;
2. `runtime-capability-architecture-v0.1.md` for 05 capability ownership/dependency details where not superseded by later current authority;
3. 06/07/08 current authority for more specific implementation/data-flow/platform contracts.

`axiom-architecture-baseline-v0.3.md` is retained only as historical provenance and MUST NOT override v0.4.

## Current ownership interpretation

Platform Host is the platform composition/lifecycle/orchestration root. Axiom Platform Runtime is the Axiom-side platform integration layer; they are not two names for the same ownership boundary.

05 defines capability ownership and allowed dependency direction. It does not authorize implementation agents to move semantic, storage, render, Arc or platform responsibilities across subsystem boundaries for convenience.

## Precedence

When this index or an older 05 snapshot conflicts with a more specific current 06/07/08 contract, follow `docs/notion/manifest.yaml` and the more specific current authority. Historical material may explain design evolution but is not a fallback source for missing current semantics.

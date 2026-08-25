# 05 Runtime Capability Architecture — Authority Index

> Source root: Notion `05 Runtime Capability Architecture`
> Source page: https://app.notion.com/p/3c44c57a590c8040a512c4a941bdc3a8
> Snapshot date: 2026-08-24

## Repository authority

Start with `runtime-capability-architecture-v0.1.md` for the implementation-facing reconciliation of the 05 authority set.

The Notion source root contains:

- Runtime Capability Overview v0.1
- Ownership Matrix v0.1
- Dependency Graph v0.1
- Public Boundary Contract v0.1
- Platform Host Runtime Contract v0.1 — historical Axiom-internal host identity

## Precedence rule

The current architecture-level `Platform Host = Composition Root` interpretation from the Runtime Capability Overview and later 06 Platform Host closure takes precedence over the older document's identity of `Platform Host = Axiom Host Runtime`.

The older document remains useful for retained native surface/frame/Arc details, but its identity/layering must not override the newer architecture baseline.

## Codex rule

05 defines capability ownership and allowed dependency direction. It does not authorize an implementation agent to move semantic, storage, render, Arc, or platform responsibilities across subsystem boundaries for convenience.

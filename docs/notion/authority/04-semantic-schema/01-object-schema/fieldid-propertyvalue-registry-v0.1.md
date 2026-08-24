# FieldId + PropertyValue Registry v0.1

> Source: Notion `FieldId + PropertyValue Registry v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81c58d21e6f93c824f06
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

Defines the registry model for independently patchable semantic properties and the closed `PropertyValue` tagged union.

## Registry invariants

- Field IDs are stable protocol identifiers.
- Published IDs are never reused; retirement reserves the ID.
- Every released field defines value type, applicable ObjectKinds, default semantics, clear semantics, validation and version behavior.
- `PatchProperties` is registry-driven, not a generic JSON patch.
- Placement, Transform, ObjectKind, path/stroke geometry, RichText content and EraseMask are not generic fields.

## Namespace direction

The registry reserves ranges by semantic family, including connector appearance and container-related ranges. Exact released entries/defaults are owned by `Field Registry V1 Release Table + Default Semantics Closure v0.1` and generated registry artifacts.

## Implementation rule

Codex should generate or centralize field metadata from one registry source and make validation/default/clear behavior consume that source. Do not duplicate field applicability/default logic across C++, TS and codec implementations.

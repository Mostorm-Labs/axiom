# Reference IDL Integration + Leaf Schema Reconciliation v0.1

> Source: Notion `Reference IDL Integration + Leaf Schema Reconciliation v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81a1902be95a889f68e0
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Reconciliation principle

Leaf Schema owns semantic detail; integrated Reference IDL owns the compiled protocol surface. Integration must not silently simplify or reinterpret a leaf authority.

## Locked reconciliation outputs

- ObjectKind identity includes Connector=7, Sticky=8, Group=9.
- Operation payload identity is the released 15-kind vocabulary.
- Geometry, Paint/Style, VectorPath, EraseMask, Stroke/Brush and RichText leaf types are integrated without moving renderer/runtime data into the semantic protocol.
- Presence and version semantics are reconciled with Common Wire Rules.

## Conflict rule

If a generated IDL cannot represent a released leaf semantic exactly, the correct response is to change/review the IDL mapping, not to weaken the leaf semantic in implementation.

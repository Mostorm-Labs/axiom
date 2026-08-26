# Geometry Types v0.1

> Source: Notion `Geometry Types v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c8157a657c7f796ebcdac
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

Defines shared renderer-neutral geometry primitives and validation used by Image, VectorPath, EraseMask, Connector and other semantic schemas.

## Rules

- Geometry values obey Common Wire numeric rules.
- Normalized rectangles/ranges must remain within their released domain; values extending outside the normalized extent are rejected rather than silently clamped.
- Geometry schema must not contain Skia/runtime/GPU types.
- Leaf validation is semantic and runs before atomic apply.

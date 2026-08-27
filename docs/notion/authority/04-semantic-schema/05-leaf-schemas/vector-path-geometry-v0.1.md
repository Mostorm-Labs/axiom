# VectorPathGeometry v0.1

> Source: Notion `VectorPathGeometry v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c810e83eff3f5645b9703
> Repository status: frozen-by-v1-final-gate

## Role

Defines canonical renderer-neutral VectorPath command/segment geometry used by VectorPath content and `SetVectorPathGeometry`.

## Rules

- Path commands/points are semantic geometry, not SkPath serialization.
- Structural/path validation occurs before apply.
- Unsupported future payload/schema versions follow Common Wire compatibility rules rather than best-effort reinterpretation.
- Renderer compilation to SkPath/mesh/display list is derived and disposable.

## VP-O1 closure

The historical open item `VP-O1` asked how VectorPath command/point content maps to the V1 hard limit `geometry_point_like_elements_per_operation_aggregate = 2,000,000`, including whether a `Vec2` counts as one point-like entry or two scalar entries.

`VP-O1` is now **CLOSED** by the current gap-closure authority:

- `Geometry Aggregate Accounting Contract V1 v0.1`
- Notion page id: `3c94c57a-590c-81e1-aea7-eae7ea8a8c88`
- Repository mirror: `../06-release-safety/geometry-aggregate-accounting-contract-v1-v0.1.md`
- Machine projection: `schema/axiom/v1/canonical/geometry_accounting_v1.yaml`

The closure freezes `Vec2 = 1 point-like geometry atom`, exact VectorPath/Stroke/Dab/Erase carrier weights and 15-Operation aggregation. It does not otherwise supersede or change VectorPathGeometry semantics.
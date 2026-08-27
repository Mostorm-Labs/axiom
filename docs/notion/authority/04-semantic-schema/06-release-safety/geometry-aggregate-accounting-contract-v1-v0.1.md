# Geometry Aggregate Accounting Contract V1 v0.1

> Source: Notion `Geometry Aggregate Accounting Contract V1 v0.1`
> Source page: https://app.notion.com/p/3c94c57a590c81e1aea7eae7ea8a8c88
> Source page id: `3c94c57a-590c-81e1-aea7-eae7ea8a8c88`
> Source status: **CURRENT AUTHORITY — V1 Geometry Aggregate Accounting Closure**
> Repository status: current
> Scope: GT-G1-04-A stateless semantic validation / protocol safety only

## Closure relationship

This is a gap-closure authority, not a supersession of the existing geometry owners:

```text
VectorPathGeometry v0.1 — VP-O1 OPEN
        ↓ closed by
Geometry Aggregate Accounting Contract V1 v0.1
```

It consumes the already-frozen global limit from `Axiom V1 Semantic Hard Limits + Protocol Safety Budget v0.1`:

```text
geometry_point_like_elements_per_operation_aggregate = 2,000,000
```

This authority freezes **how the aggregate is counted**. It does not change the limit value and does not introduce leaf-specific performance ceilings.

## GAA-01 — accounting unit

V1 uses decoded canonical **geometry atoms**:

- one canonical `Vec2` geometry carrier = **1 point-like unit**;
- one independent geometry extent/orientation scalar explicitly listed below = **1 scalar unit**;
- counting must not depend on `sizeof`, protobuf encoded byte length, renderer flattening, SkPath verb count, GPU primitives, or platform memory layout.

Explicitly excluded from this budget:

- brush/sample pressure and pressure curves;
- sample tilt;
- dab opacity;
- color/alpha/paint/stroke style;
- BrushDescriptor smoothing/spacing/blend/resource identity;
- Object transform/placement/size;
- Connector content/routing;
- RichText, Image, and PropertyBag values.

Those values remain subject to their own semantic/wire limits.

## Carrier weights

| Canonical carrier | Units | Rule |
| --- | ---: | --- |
| `MoveTo.point` | 1 | one `Vec2` point-like atom |
| `LineTo.end` | 1 | one `Vec2` point-like atom |
| `QuadTo.control` | 1 | one `Vec2` atom |
| `QuadTo.end` | 1 | one `Vec2` atom |
| `CubicTo.control1` | 1 | one `Vec2` atom |
| `CubicTo.control2` | 1 | one `Vec2` atom |
| `CubicTo.end` | 1 | one `Vec2` atom |
| `ClosePath` | 0 | topology only |
| `StrokeSample.position` | 1 | sample pressure/tilt excluded |
| `DabInstance.center` | 1 | point-like atom |
| `DabInstance.size` | 1 | geometry extent scalar |
| `DabInstance.rotation` | 1 | geometry orientation scalar |
| `DabInstance.opacity` | 0 | appearance scalar |
| `EraseKnot.position` | 1 | point-like atom |
| `EraseKnot.radius` | 1 | geometry extent scalar |
| `EraseCubicSegment.control1` | 1 | point-like atom |
| `EraseCubicSegment.control2` | 1 | point-like atom |
| `FilledPathMask.path` | recursive | use VectorPath accounting |

Consequences:

```text
QuadTo                         = 2 units
CubicTo                        = 3 units
StrokeSample                   = 1 unit
DabInstance                    = 3 units
EraseCubicSegment              = 6 units
```

An `EraseCubicSegment` counts `p0.position + p0.radius + p1.position + p1.radius + control1 + control2`. Equal adjacent endpoint values are **not** deduplicated; each carried occurrence counts independently.

## GAA-02 — composite geometry rules

Counting is based on canonical geometry carriers actually contained in the Operation payload. Do not deduplicate by value or identity and do not recalculate from renderer output.

```text
VectorPathUnits(path)
  = sum(command units)

VectorStrokeUnits(stroke)
  = count(samples)

DabStrokeUnits(stroke)
  = 3 * count(dabs)

SweptCircleMaskUnits(mask)
  = 6 * count(segments)

FilledPathMaskUnits(mask)
  = VectorPathUnits(mask.path)

EraseMaskUnits(mask)
  = selected geometry variant units

ObjectRecordGeometryUnits(object)
  = content geometry units
  + sum(object.eraseMasks geometry units)
```

Shape, Image, RichText, Connector, Sticky and Group content contribute 0 geometry units by themselves.

## Operation aggregation

| Operation | Geometry aggregate |
| --- | --- |
| `InsertObjects` | sum `ObjectRecordGeometryUnits` for every inserted object |
| `DeleteObjects` | 0 |
| `RestoreObjects` | sum `ObjectRecordGeometryUnits` for every restored object |
| `SetPlacements` | 0 |
| `SetTransforms` | 0 |
| `PatchProperties` | 0 |
| `SetObjectSize` | 0 |
| `SetVectorPathGeometry` | `VectorPathUnits(replacement geometry)` |
| `SetImageContent` | 0 |
| `AddStroke` | `ObjectRecordGeometryUnits(new stroke object)` |
| `SplitStrokes` | sum replacement `ObjectRecordGeometryUnits` |
| `AddEraseMasks` | sum newly supplied `EraseMaskUnits` |
| `RemoveEraseMasks` | 0 |
| `EditRichText` | 0 |
| `SetConnectorContent` | 0 |

The budget measures **Operation payload complexity**, not resulting Document geometry. A stateless validator must not read existing ObjectStore geometry to compute this aggregate.

## GAA-03 — overflow and boundary semantics

Use checked unsigned integer accumulation. Any addition/multiplication overflow rejects before comparison. No wrap, saturation, clamp, truncation or partial count is allowed.

```text
aggregate <  2,000,000  → geometry-limit condition satisfied
aggregate == 2,000,000  → geometry-limit condition satisfied
aggregate >  2,000,000  → GEOMETRY_LIMIT_EXCEEDED
arithmetic overflow      → INTEGER_OVERFLOW
```

Passing this limit does not bypass other structural or later stateful validation.

## GAA-04 — A2 WirePreflight vs A3 ownership

- **A3 stateless semantic validation** owns the normative exact geometry aggregate and must use this carrier table.
- **A2 WirePreflight** must reject earlier when raw wire is sufficient to mechanically prove the limit is exceeded.
- A2 must not substitute approximate command-count, byte-count or renderer-derived metrics for the exact semantic accounting rule.
- A2 and A3 must agree on the final geometry-limit verdict for the same structurally decodable payload.
- No ObjectStore/state lookup belongs in this accounting path.

## Golden boundary requirements

The previous `OPEN / blocked` geometry-limit corpus condition is closed by this authority. Independent conformance fixtures must cover:

```text
1,999,999 units → limit condition pass
2,000,000 units → limit condition pass
2,000,001 units → GEOMETRY_LIMIT_EXCEEDED
checked arithmetic overflow → INTEGER_OVERFLOW
```

At least the following constructions must be represented so command-count shortcuts cannot pass accidentally:

1. mixed VectorPath `LineTo / QuadTo / CubicTo`;
2. DabStroke where each dab contributes 3 units;
3. SweptCircle erase where each segment contributes 6 units.

The fixture/compiler must not call the production geometry counter to manufacture expected truth.

## GAA-05 — cross-language determinism

C++, TS and WASM implementations must consume or validate the same machine-readable accounting profile. They may not maintain independent handwritten mappings with merely similar intent.

Accounting must be independent of host word size, protobuf object layout, floating-point geometry evaluation, flatten tolerance, rasterization and GPU representation.

## Non-goals

This authority does not freeze:

- per-VectorPath command ceilings;
- per-Stroke sample/dab ceilings;
- per-mask segment ceilings;
- document-wide geometry limits;
- renderer tessellation/chunk/tile budgets;
- GT-G1-04-B stateful target/reference validation;
- GT-G1-04-C implementation;
- GT-G1-05 atomic apply / ChangeSet behavior.

## Machine-readable projection

The normative human authority is projected to:

```text
schema/axiom/v1/canonical/geometry_accounting_v1.yaml
```

The YAML is a derived machine projection and cannot override this human authority or `protocol_hard_limits_v1.yaml`.
# Axiom ObjectContent V1 Field Table v0.1

> Source: Notion `Axiom ObjectContent V1 Field Table v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c811f981fdebd9d8198ec
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

This authority freezes the boundary between kind-specific `ObjectContent` and generic `PropertyBag` for the V1 object universe. It is downstream of Semantic Schema Gap Closure and upstream of Field Registry, Operation payloads, Reference IDL and generated Proto.

## V1 object-kind reconciliation

The schema-gap work expanded the V1 semantic universe to include the canonical kinds required by product semantics, including Connector, Sticky and Group in addition to the original basic kinds. The integrated Reference IDL release assigns stable ObjectKind IDs and must be used for numeric values.

## Content vs Property rule

Put data in `ObjectContent` when it defines kind identity/structure/geometry/content and cannot safely be treated as a generic independently patchable property. Put small stable appearance/behavior values in `PropertyBag` only when they are published through the Field Registry.

Never put Placement, Transform, RuntimeScene/Tile/Skia/GPU state, Selection/Viewport, sync metadata or platform handles into ObjectContent.

## Mutation-coverage consequence

The field-table review discovered three canonical mutation-coverage gaps in the earlier 12-operation vocabulary. Those gaps were closed downstream; therefore Codex must implement the **released 15-operation vocabulary** from Operation Payload + Reference IDL rather than the historical 12-operation list in the early overview.

## Leaf ownership

Detailed geometry/style/stroke/rich-text/erase encoding is delegated to the corresponding leaf-schema authorities. This table owns the semantic field boundary; leaf schemas own the exact nested representation.

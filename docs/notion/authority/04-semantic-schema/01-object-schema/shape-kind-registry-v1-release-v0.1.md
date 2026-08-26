# ShapeKind Registry V1 Release v0.1

> Source: Notion `ShapeKind Registry V1 Release v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81c0ab80d6a8d6aa0778
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Authority

This page freezes the V1 ShapeKind vocabulary and exact semantic interpretation. It does not expand top-level ObjectKind or add Operations.

## Core rules

- ShapeKind is a registry within ShapeContent, not one top-level ObjectKind per primitive.
- V1 released ShapeContent fields are semantic-required according to the release authority.
- Connector remains semantically distinct from Shape/Line/Arrow-like visuals; attachment/routing semantics must not be encoded by pretending a Connector is an ordinary Shape or VectorPath.
- Future recognition may convert strokes into released V1 shapes, but future shape vocabulary additions require versioned registry evolution.

## Implementation rule

Implement ShapeKind from a released registry artifact and reject/handle unknown versions according to Common Wire Rules. Do not infer kind semantics from renderer-specific Skia primitives.

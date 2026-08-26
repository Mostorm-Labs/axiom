# VectorPathGeometry v0.1

> Source: Notion `VectorPathGeometry v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c810e83eff3f5645b9703
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

Defines canonical renderer-neutral VectorPath command/segment geometry used by VectorPath content and `SetVectorPathGeometry`.

## Rules

- Path commands/points are semantic geometry, not SkPath serialization.
- Structural/path validation occurs before apply.
- Unsupported future payload/schema versions follow Common Wire compatibility rules rather than best-effort reinterpretation.
- Renderer compilation to SkPath/mesh/display list is derived and disposable.

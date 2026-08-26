# Paint / Style Value Types v0.1

> Source: Notion `Paint / Style Value Types v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81039162fddc4ff8bbe3
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

Defines renderer-neutral semantic paint/style value types shared by Field Registry, Shape/Connector appearance and stroke/brush semantics.

## Boundary

Paint/style semantic values describe canonical appearance. `SkPaint`, shader handles, textures, compiled effects and GPU resources are derived runtime state.

Defaults/clear semantics are governed by the V1 Field Registry release, and numeric/color canonicalization follows Common Wire Rules.

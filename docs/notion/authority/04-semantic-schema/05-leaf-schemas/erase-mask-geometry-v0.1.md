# EraseMaskGeometry v0.1

> Source: Notion `EraseMaskGeometry v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c810998a0c9d4250e0c19
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Semantic role

EraseMaskGeometry is object-local, renderer-neutral canonical erase state. It is manipulated through Add/Remove EraseMask Operations.

Runtime may compile masks to vector clips, R8 textures, sparse mask tiles or other acceleration structures, but those are derived representations and never become Document truth.

Validation/version/canonicalization follows Geometry Types and Common Wire Rules.

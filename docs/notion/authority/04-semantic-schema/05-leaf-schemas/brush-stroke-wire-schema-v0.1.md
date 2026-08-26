# BrushDescriptor + StrokeRecord Wire Schema v0.1

> Source: Notion `BrushDescriptor + StrokeRecord Wire Schema v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81fab32ec6c82321aa1f
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Role

Defines canonical semantic + wire representation for BrushDescriptor, StrokeRecord, VectorStrokeData and DabStrokeData.

## Core boundary

StrokeRecord is renderer-neutral canonical content. Raw input events, prediction tail, SkPath, mesh, StrokeChunk, Tile and GPU buffers are not part of this schema.

The released stroke/brush version namespace and supported `(family, version)` pairs are controlled by BrushFamily Registry and the deterministic interpreter release.

## Determinism

Dab/texture-capable brushes use released deterministic seed/interpreter semantics. Host RNG, platform floating behavior or renderer sampling must not change canonical interpretation.

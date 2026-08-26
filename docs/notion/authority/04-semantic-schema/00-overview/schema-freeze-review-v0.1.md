# Schema Freeze Review + V1 Release Candidate Gate v0.1

> Source: Notion `Schema Freeze Review + V1 Release Candidate Gate v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81508e53db145015a2f7
> Snapshot date: 2026-08-24
> Repository status: superseded-by-final-gate

## Historical role

This document was the release-review ledger that initially held the schema at `HOLD / Not Yet V1 RC` while RC blockers were closed. It is retained because it records why the final release authorities exist.

The later `Axiom Semantic Schema V1 Release Candidate Final Gate v0.1` supersedes its HOLD verdict.

## Important closed directions

- Canonical codec is no longer an open Protobuf/FlatBuffers/custom choice: V1 uses **Protobuf + Axiom Canonical Protobuf rules**.
- V1 generated operation payload oneof is locked to tags `1..15`.
- RC-B07 stochastic interpreter closure freezes the V1 deterministic stochastic sequence under the released brush interpreter authority.
- Release-specific Shape, Image, Connector, Brush, Pressure/Tilt, RichText Font and safety authorities close historical gaps in the overview documents.

## Codex rule

Use this document for blocker history and supersession reasoning only. For implementation behavior, prefer the Final Gate and the corresponding leaf/release authority.

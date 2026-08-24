# Axiom V1 Semantic Hard Limits + Protocol Safety Budget v0.1

> Source: Notion `Axiom V1 Semantic Hard Limits + Protocol Safety Budget v0.1`
> Source page: https://app.notion.com/p/3c54c57a590c81d3bb6ef58905e1ebf2
> Snapshot date: 2026-08-24
> Repository status: frozen-by-v1-final-gate

## Purpose

Defines protocol-safety limits shared by Common Wire, Operation Validation, Reference IDL semantic notes, leaf schemas, WirePreflight/Canonical Codec and semantic conformance.

## Principle

Safety limits protect decode/apply complexity and memory use. They are not product-scale targets. In particular, V1 does **not** convert a product document-size goal into a universal semantic object-count ceiling.

## Enforcement layers

- Wire preflight rejects impossible/oversized envelopes before expensive allocation.
- Leaf decoders enforce released collection/string/geometry limits.
- Operation validation enforces payload and atomic-apply budgets.
- Snapshot/document-scale policies remain distinct from per-message protocol safety limits unless explicitly released.

Some product/import chunk sizes remain deferred to V1.1 unless a leaf release authority explicitly freezes them.

## Codex rule

Implement limits from one shared versioned source where possible and test boundary-1/boundary/boundary+1 cases across C++ and TS/WASM runners.

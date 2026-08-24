# Axiom V1 Machine-Readable Semantic Authority

This directory converts the frozen Notion 04 Semantic Schema authority into inputs that implementation and conformance tooling can consume directly.

## Normative stack

The protocol is not defined by `.proto` alone.

1. `docs/notion/authority/04-semantic-schema/**` — human-readable semantic authority.
2. `proto/**` — structural wire schema (next closure step).
3. `canonical/canonical_profile_v1.yaml` — canonical protobuf behavior.
4. `canonical/protocol_hard_limits_v1.yaml` — global protocol safety ceilings.
5. `registry/*.yaml` — released numeric registries and semantic metadata.
6. `descriptor/axiom_v1.descriptor.pb` — generated descriptor lock (derived; generate after proto baseline lands).
7. Golden corpus — executable protocol evidence.

## Already materialized

- Field Registry V1
- ShapeKind Registry V1
- BrushFamily Registry V1
- Operation Registry V1 (15 operations / tags 1..15)
- Canonical Protobuf Profile V1
- Protocol Hard Limits V1
- Toolchain/source-layout baseline

## Closure still required before codegen gate

The 12 Edition-2024 `.proto` source files must be transcribed exactly from the integrated Reference IDL / leaf authorities and compiled. Then generate the descriptor set and lock its fingerprint. Do not invent missing field/tag details from this README or the YAML registries.

## Codex rule

If Markdown authority and a machine-readable artifact disagree, stop and report the conflict. Do not silently choose one or regenerate authority from implementation code. Machine-readable files are frozen inputs once their corresponding closure gate is marked complete.

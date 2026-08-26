# Axiom V1 Machine-Readable Semantic Authority

This directory contains the GT-G1-01 promoted V1 semantic contract inputs. They are
the repository copy selected by authority reconciliation; future changes require an
explicit schema review and must not be inferred from runtime implementation output.

## Normative stack

The protocol is not defined by `.proto` alone.

1. authority reconciliation Evidence — source status, retrieval time and promotion decision.
2. `proto/**` — structural wire schema.
3. `canonical/canonical_profile_v1.yaml` — canonical protobuf behavior.
4. `canonical/protocol_hard_limits_v1.yaml` — global protocol safety ceilings.
5. `registry/*.yaml` — released numeric registries and semantic metadata.
6. `descriptor/axiom_v1.descriptor.pb` — generated descriptor lock (derived in GT-G1-02).
7. Golden corpus — executable protocol evidence.

## Promoted in GT-G1-01

- Field Registry V1
- ShapeKind Registry V1
- BrushFamily Registry V1
- Operation Registry V1 (15 operations / tags 1..15)
- Canonical Protobuf Profile V1
- Protocol Hard Limits V1
- 12 Edition-2024 `.proto` source files

## Closure still required before codec/codegen gate

GT-G1-02 must compile the sources with the reconciled toolchain, generate the descriptor set and lock its fingerprint. It must compare generated DTO/codec behavior against the canonical profile and corpus. Do not invent field/tag details from this README or the YAML registries.

## Codex rule

If a newly retrieved authority and a machine-readable artifact disagree, stop and record the conflict. Do not silently choose one or regenerate authority from implementation code. The generated descriptor and corpus are deferred artifacts, not present in this directory yet.

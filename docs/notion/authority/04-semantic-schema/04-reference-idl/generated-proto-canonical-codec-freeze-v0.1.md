# Generated Proto Baseline + Canonical Codec Freeze v0.1

> Source: Notion `Generated Proto Baseline + Canonical Codec Freeze v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c81c29b93c774869cff99
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Compilable Proto Baseline + Canonical Codec
> Repository status: frozen-by-v1-final-gate

## V1 codec lock

V1 chooses **Protobuf + Axiom Canonical Protobuf**.

The generated baseline is expected to compile and acts as the concrete mapping of the Reference IDL. The Operation payload oneof uses tags `1..15`.

## Canonical codec requirements

Canonical encoding is stricter than “protobuf can decode it”. The wrapper must enforce Axiom rules for:

- field presence/default semantics;
- canonical numeric representation;
- deterministic collection/field treatment where required;
- unknown/unsupported version behavior;
- hard-limit preflight;
- stable canonical bytes across supported language implementations.

## Descriptor lock

The schema descriptor is a compatibility artifact. CI/conformance should detect accidental field/tag/type drift even when generated code still compiles.

## Codex implementation target

Do not hand-author divergent C++ and TS wire models. Generate bindings from the locked proto baseline, then implement Axiom canonical wrappers and semantic validation above generated protobuf APIs.

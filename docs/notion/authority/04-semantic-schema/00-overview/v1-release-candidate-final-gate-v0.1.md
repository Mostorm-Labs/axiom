# Axiom Semantic Schema V1 Release Candidate Final Gate v0.1

> Source: Notion `Axiom Semantic Schema V1 Release Candidate Final Gate v0.1`
> Source page: https://app.notion.com/p/3c54c57a590c813689c3f2b7cdb87625
> Snapshot date: 2026-08-24
> Source status: V1 Release Candidate Schema Lock
> Repository status: frozen

## Gate verdict

The 04 Semantic Schema / Operation Model has reached **V1 Release Candidate Schema Lock**. This is a semantic-protocol release gate, not full Axiom product/runtime GA.

Any later V1 semantic change requires an explicit refreeze rather than an implementation-local change.

## Released authority families

The final release set includes the core semantic schema plus release authorities for:

- field registry/default semantics;
- ShapeKind V1;
- ImageContent canonical geometry/presence;
- Connector referential integrity/anchor contract;
- BrushFamily V1;
- deterministic brush interpreter / PRNG / highlighter blend;
- pressure + tilt canonical input;
- RichText font semantic baseline;
- operation payload + atomic validation;
- Common Wire Rules / OrderKey;
- integrated Reference IDL;
- generated Proto baseline + canonical codec;
- leaf schemas;
- V1 semantic hard limits / protocol safety budget.

## Release-suite identity

The final gate explicitly treats release suites such as `seed-v0.1`, `shape-kind-v1`, `image-content-v1`, `connector-v1`, `brush-family-v1`, `brush-interpreter-v1`, pressure/tilt and RichText release suites as protocol evidence rather than optional examples.

## Codex rule

Implementation must consume the released authority set as a whole. It must not use an older overview statement to override a later release authority. Generated code, validators and codecs must preserve the released IDs/tags/presence/default/canonicalization semantics.

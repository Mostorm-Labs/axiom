# Canonical Codec Implementation Skeleton + Binary Golden Seed v0.1

> Source: Notion `Canonical Codec Implementation Skeleton + Binary Golden Seed v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c8101a8d4e54bd9537e23
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Executable Codec Contract
> Repository status: proposed-freeze

## Executable codec contract

Axiom V1 permits one semantic decode path and one canonical encode path:

`incoming bytes → WirePreflight → Generated DTO Decode → DtoMapper::toDomain → Normalize → Semantic Validate → Domain Value`

`Domain Value → Normalize / Assert Valid → Canonical collection ordering → DtoMapper::toDto → CanonicalWriter → canonical protobuf bytes`

Generated protobuf DTOs are wire DTOs, not semantic domain objects. Protobuf deterministic serialization is not the Axiom canonical writer.

## Core implementation components

- `WirePreflight`: preserves raw-wire evidence that generated parsers may discard, including malformed/truncated wire, unknown fields, duplicate singular fields, multiple oneof members, packed-profile violations and safety budgets.
- `DtoMapper`: mechanical DTO/domain mapping only; no hidden repair, sorting, semantic validation or renderer conversion.
- `SemanticDecoder`: preflight → generated decode → map → normalize → validate.
- `CanonicalWriter`: accepts validated semantic values and owns ascending tags, canonical presence, normalized scalar bits, canonical collection ordering, packed representation and minimal wire primitives.

Canonical assertion is defined as semantic decode → canonical re-encode → byte equality.

## Error contract

Errors carry a stable stage + category. Runtime/library diagnostic strings are non-authoritative. Cross-language verification compares stable machine categories, not implementation-specific exception text.

## Golden authoring trust boundary

The checked-in binary seed follows the Golden Corpus Authoring Rules: expected bytes are derived from human-reviewed semantic/wire authority by verification-only tooling. Production Axiom semantic/apply/canonical encoder must not be the sole generator of its own expected answers.

## Binary Golden Seed v0.1

The frozen positive byte seed is BG-001 through BG-010:

1. Id128
2. OrderKey
3. Vec2
4. Transform2D identity with explicit semantic scalar presence
5. PropertyValue f32 0.5
6. ColorValue including explicit zero channel presence
7. root Placement
8. parented Placement
9. packed DashPattern
10. minimal DocumentSnapshot

Repository binary truth lives under `verification/golden/v1/wire/codec/**/expected/canonical.pb`; the review surface and stable suite identity live in `verification/golden/v1/suites/codec-binary-seed-v0.1.json`.

## Next implementation gates

This snapshot does not claim C++/TS/WASM codec parity yet. Promotion from proposed-freeze requires real production/reference codec evidence including BG-001..010 byte equality, negative BG-N01..N08 stage/category behavior, descriptor/profile mismatch coverage, generated DTO boundary checks, and cross-language differential evidence.

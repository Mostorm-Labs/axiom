# Axiom Reference IDL + Codec Mapping v0.1

> Source: Notion `Axiom Reference IDL + Codec Mapping v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c8161bd08d1dedd37ef5c
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Integrated Reference Schema + Codec Mapping
> Repository status: frozen-by-v1-final-gate

## Role

This is the integrated implementation-facing schema authority. It reconciles ObjectContent, Field Registry, the released **15 Operation** vocabulary, Common Wire Rules, OrderKey and leaf schemas into one Reference IDL.

## Locked identities

The integrated reconciliation publishes stable ObjectKind IDs including:

- `7 Connector`
- `8 Sticky`
- `9 Group`

Earlier kind IDs and the complete table remain part of the integrated generated schema and must not be renumbered by implementation.

OperationKind IDs / `OperationPayload` oneof tags are locked by the integrated schema; the generated Proto release confirms tags `1..15`.

## Codec decision

V1 wire format is Protobuf plus **Axiom Canonical Protobuf** rules. Ordinary protobuf semantic equivalence is not sufficient for canonical-byte equality; Axiom adds canonical presence/order/numeric/unknown-field rules defined by the wire/codec authorities.

## Binding rule

C++, TypeScript and WASM/reference bindings map to the same semantic schema. Generated language APIs are not themselves semantic authority when their language defaults conflict with explicit Axiom presence/default rules.

## Implementation deliverables implied by this authority

- compilable `.proto` baseline;
- generated C++ and TS/reference bindings;
- canonical encoder/decoder wrappers;
- descriptor lock/fingerprint;
- cross-language golden vectors;
- validation before atomic semantic apply.

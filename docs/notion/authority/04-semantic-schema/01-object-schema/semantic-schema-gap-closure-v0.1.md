# Semantic Schema Gap Closure v0.1

> Source: Notion `Semantic Schema Gap Closure v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c8121b6a0fbf4afdcc45f
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — Semantic Schema Gap Closure
> Repository status: frozen-by-v1-final-gate

## Closed gaps

This authority was created to close four V1 blockers:

1. Product Page reconciliation.
2. ConnectorContent / Endpoint / Anchor representation.
3. Connector mutation coverage.
4. Sticky / Group representation.

## Key consequences

- Product Page topology is reconciled with **one Product Page = one Axiom Document**; product page-list metadata is not duplicated as Canvas object-list semantics.
- Connector is a canonical semantic object, not merely a routed VectorPath.
- Endpoint attachment/anchor relation is canonical; routed geometry is derived.
- Sticky and Group receive explicit V1 semantic representation rather than remaining informal UI composites.
- The V1 ObjectKind universe and downstream Operation vocabulary therefore supersede the smaller early overview vocabulary.

## Implementation rule

Do not reconstruct these concepts from product UI conventions. Use the released ObjectContent, Connector contract, Operation payload and Reference IDL authorities.

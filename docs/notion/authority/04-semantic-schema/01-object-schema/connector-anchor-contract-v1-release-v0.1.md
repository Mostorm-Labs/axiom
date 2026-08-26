# Connector Referential Integrity + Anchor Contract V1 Release v0.1

> Source: Notion `Connector Referential Integrity + Anchor Contract V1 Release v0.1`
> Source page: https://app.notion.com/p/3c44c57a590c817384dedc221b023bde
> Snapshot date: 2026-08-24
> Source status: Proposed Freeze — V1 Connector Referential Integrity / Anchor Contract
> Repository status: frozen-by-v1-final-gate

## Semantic ownership

Connector is a canonical semantic object. Endpoint relation and anchor semantics are canonical; routed polyline/path is derived runtime state.

## Endpoint model

The released wire model distinguishes free-point and attached endpoints through a tagged/oneof representation. Attached endpoints carry referential identity to an attachable object and anchor information; free endpoints carry world/local point semantics as defined by the leaf contract.

## Referential integrity

V1 connectability is constrained by released semantic rules rather than arbitrary ObjectId references. Current operation validation identifies V1 connectable objects as Shape v1 / Image v1 / Sticky v1.

Mutation must validate references and anchor compatibility atomically before applying canonical state.

## Implementation rule

- Never persist routed SkPath/polyline as connector truth.
- Do not allow platform/render hit proxies to become endpoint identity.
- Reference validation belongs in semantic operation validation before atomic apply.

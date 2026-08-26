# G0 Verification Foundation — Authority Mapping

This directory maps Codex G0 work packages to repository authority. It does **not** implement G0.

## Source execution model

Notion `G0 Verification Foundation Implementation Plan v0.1` delegates the existing IH-00..IH-15 issue pack as the authoritative execution source, then adds G0 gate evidence aggregation/review. The source issue pack itself contains 16 issues (IH-00..15).

The user's active Codex sequence is referred to as G0-00..G0-17. Because the current Notion source does not publish a literal 18-row G0-00..17 table, `authority-map.yaml` marks the first 16 rows as direct IH mappings and the final two as **derived G0 plan mappings**:

- G0-16 = Gate Report schema/aggregator work from G0 Task 3.
- G0-17 = G0 Gate Review from G0 Task 4.

This distinction is intentional; do not present G0-16/17 as IH-16/17.

## Global authority

Every G0 work package should begin with:

- `docs/notion/authority/10-verification/README.md`
- `docs/notion/authority/08-platform-contract/README.md` when platform-facing
- `docs/notion/authority/07-runtime-data-flow/README.md` when testing runtime ordering/lifecycle/presentation
- `docs/notion/authority/04-semantic-schema/**` and `schema/axiom/v1/**` when semantic/protocol behavior is involved
- existing `verification/**` machine-readable schemas/corpus as read-only verification authority

## Ownership rule

ChatGPT maintains/migrates authority and mappings on this branch. Codex owns production/verification implementation work packages. If implementation discovers an authority contradiction, stop that work package and request refreeze/reconciliation instead of silently changing authority.

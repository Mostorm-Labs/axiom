# 04 Semantic Schema / Operation Model — Repository Authority Index

This directory is the implementation-facing mirror of the Notion `04 Semantic Schema / Operation Model` authority tree.

## Codex reading order

1. `axiom-semantic-schema-spec-idl-v0.1.md`
2. `00-overview/v1-release-candidate-final-gate-v0.1.md`
3. `00-overview/g1-04-a-semantic-authority-closure-gate-v0.1.md`（只适用于 GT-G1-04-A）
4. `01-object-schema/semantic-schema-gap-closure-v0.1.md`
5. `01-object-schema/object-content-v1-field-table-v0.1.md`
6. `01-object-schema/fieldid-propertyvalue-registry-v0.1.md`
7. `01-object-schema/field-registry-v1-release-v0.1.md`
8. `01-object-schema/shape-kind-registry-v1-release-v0.1.md`
9. `01-object-schema/image-content-v1-release-v0.1.md`
10. `01-object-schema/connector-anchor-contract-v1-release-v0.1.md`
11. `01-object-schema/objectkind-version-registry-v1-release-v0.1.md`
12. `02-operation-model/operation-payload-validation-v0.1.md`
13. `02-operation-model/operation-structural-semantics-v1-closure-v0.1.md`
12. `03-wire-contract/common-wire-rules-v0.1.md`
13. `03-wire-contract/order-key-rfc-v0.1.md`
14. `04-reference-idl/reference-idl-codec-mapping-v0.1.md`
15. `04-reference-idl/leaf-schema-reconciliation-v0.1.md`
16. `04-reference-idl/generated-proto-canonical-codec-freeze-v0.1.md`
17. `04-reference-idl/richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md`
18. `04-reference-idl/canonical-codec-golden-authority-closure-v0.1.md`
20. `05-leaf-schemas/semantic-leaf-structural-validation-closure-v1-v0.1.md`
21. `05-leaf-schemas/*`
22. `06-release-safety/semantic-hard-limits-v0.1.md`

## Current G1-04 semantic freeze

The explicit human approval `APPROVE_G1_04_SEMANTIC_FREEZE_V1` published four current
authorities for **GT-G1-04-A only**: Operation structural semantics, the ObjectKind version
registry, stateless leaf structural validation and their composition gate. Their Notion page IDs
and repository paths are the `semantic_schema_04` entries in `docs/notion/manifest.yaml`.

They make A0–A3 implementation readiness reconcilable. They do **not** authorize GT-G1-04-B
(stateful validation and ApplyPlan), GT-G1-04-C (reviewed verification corpus) or GT-G1-05.
The three corresponding YAML projections under `schema/axiom/v1/` are a machine-readable view of
these authorities; neither YAML nor current implementation may override them.

The RichText + Stroke reconciliation matrix is a current defect/reconciliation authority. It
does not supersede the RichText or Brush/Stroke leaf semantic owners; it records the required
machine-projection refreeze and the defect registry that a later GT-G1-02R repair must close.

### RichText / Stroke machine reconciliation order

For the affected machine projection only, read the existing leaf authority first, then the
integration/machine projection chain:

1. `05-leaf-schemas/richtext-wire-schema-v0.1.md`,
   `richtext-font-v1-release-v0.1.md`, `brush-stroke-wire-schema-v0.1.md`,
   `brush-family-registry-v1-release-v0.1.md`, and `pressure-tilt-v1-release-v0.1.md`;
2. `04-reference-idl/leaf-schema-reconciliation-v0.1.md`;
3. `04-reference-idl/generated-proto-canonical-codec-freeze-v0.1.md`;
4. `04-reference-idl/richtext-stroke-semantic-wire-reconciliation-matrix-v0.1.md`.

The final item is a reconciliation/defect authority, not a new leaf semantic owner. Current
machine-refreeze work is authorized only after its required gate is explicitly opened.

## Current GT-G1-02 authority closure

`04-reference-idl/canonical-codec-golden-authority-closure-v0.1.md` is the current case-level semantic authority for `BG-001..BG-010` and `BG-N01..BG-N08`. It closes the semantic projection, exact canonical bytes, canonicality-only outcomes, deterministic wire defect intent, and stable rejection stage/category needed to unblock GT-G1-02 corpus materialization.

The corresponding verification-side authoring contract is `../10-verification/canonical-codec-golden-fixture-authoring-set-v0.1.md`. Both are normative only when registered as current by `docs/notion/manifest.yaml`.

## Authority rule

The current 04 status is **V1 Release Candidate Schema Lock**. Historical draft/open text remains design history only when a later release authority explicitly closes or defers it.

When two snapshots conflict, use this precedence:

`Final Gate / V1 Release authority > release-specific registry/leaf authority > current case-level codec closure > Generated Proto/Reference IDL > overview/history`.

Do not silently redesign semantic contracts while implementing. If code cannot satisfy a locked authority, record an architecture blocker and request a refreeze.

## Implementation boundary

Codex may implement generated C++/TS/protobuf bindings, codec, validation, golden corpus, and semantic core against these contracts. It must not treat RuntimeScene, Tile, Skia/GPU state, Selection/Viewport, or sync transport metadata as canonical schema.

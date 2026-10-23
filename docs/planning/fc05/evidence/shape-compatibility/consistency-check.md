# FC05 Shape compatibility consistency check

Status: READY_WITH_FINDINGS for the modeling contract only. This is an author-side
cross-check, **not independent Control Review**, P20 acceptance or a Gate verdict.
Baseline/source reviewed: 178173a03acd6f458991a69227ce3fdccaaffedd.
Contract: docs/planning/FC05_CONNECTOR_SHAPE_COMPATIBILITY_CONTRACT.md.

## Confirmed

- User scope includes all released Shape tuples, plus Image/1 and Sticky/1.
- Shape/3 is an embedded optional RichText label, not a hierarchy child. Source
  requires label presence; the successor respects this and leaves Sticky's child
  rule separate.
- Triangle vertices and the box-center ray agree with the existing Shape/2 geometry.
  The four ports lie on the actual boundary. 337 exact-rational half-plane versus
  independently calculated vertex/edge intersection checks pass; this is contract
  math evidence, not a product implementation test.
- Existing AutoPerimeter hint presence/domain, +X exact-coincident rule, free-point
  connector-local coordinates, ports 1..4, whole-content replacement and cascade
  delete/restore stay unchanged.
- Label upgrade /1 or /2 -> /3 -> Undo -> Redo retains target ObjectId; the explicit
  allowlist makes each resulting target version connectable.
- Existing schema3 CapabilityRequirementsV1 can represent a composite feature string
  without a new protobuf field or schema container version.

## Findings for the next owners

F1 — Implementation is still V1-only: connector_validation.cpp checks kind_version=1;
SnapshotBootstrapper calls the same validator. Updating UI hit capability alone
cannot close the feature. P31 must include Semantic apply/restore and typed public
consumer routes, with RED preconditions before production mutation.

F2 — Capability order/source gap: snapshot.cpp currently builds an expected list by
appending shape2-triangle-v1 before shape-richtext-label-v1. That is not lexical ASCII
order when both occur. This is implementation reality, not permission to redefine
old framing, regenerate historical bytes or silently relax validation. Before P20
freeze, bind exact successor capability vectors and define preservation/rejection of
legacy inputs. New combinations must carry connector-shape-attachment-v1 and fail
closed for a reader lacking it; old 0..7 enum values alone cannot imply this support.

F3 — Writable/continuation admission: source has no complete FC05 cross-participant
negotiation contract. Publish the local feature capability/readback and enforce known
unsupported writers/replay at existing boundaries; do not invent Sync negotiation.
Exact API inputs and negative fixtures belong to P14/P17/P20 before P31.

F4 — Outside this narrow compatibility model: Sticky width/layout publication,
orthogonal obstacle geometry/clearance/no-route and numeric work budgets are still
unfrozen. The current P20 file is deliberately DRAFT. This check cannot promote it to
FROZEN or certify performance or Windows/Web implementation parity.

## Handoff

The accepted product scope is durable. Independent review of the versioned semantic
successor and the narrow execution-order record is still required before Current
manifest transition/P31. Old Current documents and .aegis state were not edited.
Project-control CLI validation returned VALID and STATE_OK; these results concern
existing manifests, not an FC05 Gate or independent review.

# FC-05 Connector + Sticky design candidate

Status: **SCOPE_CONFIRMED / SUCCESSOR_REVIEW_INPUTS_MATERIALIZED**. This is a proposal, not Current
Authority, a P20 freeze, a P31 package, or implementation permission.
Baseline: `178173a03acd6f458991a69227ce3fdccaaffedd` (FC03 merged into main).

## Stage contract

- Role: P10–P13 modeling; downstream architecture/verification notes are inputs only.
- Authority: user-confirmed first-slice scope plus the source map below.
- Objective: define Connector + Sticky behavior without inventing canonical state.
- Non-goals: curved/manual routes, Connector labels, Group/Frame/selection expansion,
  SDK changes, historical Gate/Golden rewriting, physical PASS or G5 optimization.
- Required analysis: endpoint coordinates and anchors, supported target versions,
  child ownership, mutations/replay, routing failure and performance risks.
- Required output: candidate decisions, authority deltas and unresolved contracts.
- Quality/evidence gate: no conflict silently resolved by production code; known
  baseline tests and current-source findings remain traceable.
- Handoff: resolve the compatibility decision, accept P11/P12/P13, then prepare
  P14–P18 and freeze P20. No automatic downstream authorization.

Notion: [已确认范围与 Shape 兼容合同私有草稿](https://app.notion.com/p/4024c57a590c817ebe90e50dc83c1372)。
旧 Current 页面保持；此草稿不替代正式 Authority。

## Sources and status

| Source | Classification | Relevant boundary |
| --- | --- | --- |
| Human answers in this task | Confirmed product scope | Straight + automatic obstacle-avoiding orthogonal; Sticky fill + RichText + move/resize + History + connectability |
| [FC execution v0.3](https://app.notion.com/p/3f74c57a590c812a8260dcf23b659ed0), mirrored by `.aegis/authorities.json` | Current execution policy | Old order FC04→FC05→FC06; Sticky FC05, advanced Connector FC06 |
| [Connector anchor V1](https://app.notion.com/p/3c44c57a590c817384dedc221b023bde) | Repo frozen V1 contract; Notion body still says Proposed Freeze | Connector-local free points, canonical anchor intent, analytical resolution, V1 applicability, cascade delete |
| [Shape/2 v0.1](https://app.notion.com/p/3f74c57a590c8128b936d7491884ffb7), `.aegis/authorities.json` | Current versioned semantic authority | Triangle attachment explicitly deferred to FC06 |
| [Sticky child invariant](../notion/authority/04-semantic-schema/02-operation-model/hierarchy-parent-child-capability-sticky-child-invariant-v1-authority-closure-v0.1.md) | Current hierarchy authority mirror | Zero or one direct RichText child; other kinds rejected |
| FC02 text contracts and source | Released implementation facts | TextLayout, native IME, RuntimeFacade, Shape/3 embedded label; Sticky needs its separate RichText-child path |
| Scene, Semantic, Runtime and render source at baseline | Implementation reality | Partial support does not prove complete usable Connector/Sticky behavior |

Notion sources were fetched directly during this continuation. The user requests
Connector + Sticky before FC01A/FC04; this is an explicit sequencing request and a
proposal to bring the named Connector subset forward from FC06. Unrelated FC06,
FC04/FC08, FC13 and historical Gate obligations remain separate. No execution
Authority or old acceptance record has been silently rewritten.

## Confirmed first-slice scope

- Connector: straight and automatic obstacle-avoiding orthogonal polylines.
- Curves and manual bend points are deferred.
- Sticky: colored background, editable RichText, move/resize, Undo/Redo, and a
  Connector target.
- Sticky keeps zero or one direct RichText child; an empty Sticky is valid.
- Shared Runtime semantics and public Facade consumption on Windows and Web.

## Connector durable and derived state

Existing `ConnectorContent` contains start/end endpoint variants and routing intent.
The route is derived. Do not add canonical polyline, renderer handle, cache revision,
hover snap, search nodes or screen coordinates.

`FreePointEndpoint.point` is **Connector object-local**, not world space:
`world = connectorWorldTransform * localPoint`. Host input maps physical coordinates
once; Runtime maps the intent into the correct canonical coordinate space.

`AttachedEndpoint` retains target ObjectId plus its AnchorRef. The frozen resolver:

- Present AutoPerimeter hint is a normalized directional preference in the target's
  local box, finite within [0,1]^2, excluding exact (0.5,0.5).
- Absent hint is dynamic auto-facing. The other free endpoint resolves to world;
  the other attached endpoint contributes its target box center. There is no
  recursive perimeter-to-perimeter iteration.
- Convert that reference through the inverse target transform; exact zero direction
  uses +X. Rectangle and ellipse analytical intersections come from canonical
  geometry, never pixels, stroke width or SkPath approximations.
- V1 StablePort IDs are 1=top, 2=right, 3=bottom, 4=left, in a kind/version
  namespace. Unknown IDs fail; a same numeric ID on another version is not proof of
  applicability.
- Straight mode is the segment between resolved endpoints; it does not promise
  obstacle avoidance. Orthogonal mode requires axis-aligned world segments and
  explicit no-route/budget handling, still to be accepted below.

Moving/resizing a target changes its own canonical record. Subsequent anchor/route
refresh adds **no extra** Connector Operation, History or semantic generation.
Deleting a target follows existing cascade-delete closure; Undo restores the whole
resolved preimage, including affected connectors, as one user edit.

## Version compatibility decision (scope accepted)

The validator at baseline accepts Shape/1, Image/1 and Sticky/1 only. Shape/2 Triangle
and Shape/3 labels therefore cannot currently be attached targets. This is encoded
in both `connector_stateful_validation_test.cpp` and the FC01 negative triangle test.
A common user flow could also upgrade a connected Shape/1 to Shape/3 by adding text;
that must never produce an invalid surviving reference or silently detach it.

User-confirmed successor scope: accept the existing released Shape tuples (Shape/1
rectangle/ellipse, Shape/2 triangle, Shape/3 rectangle/ellipse/triangle with label),
retain Image/1 and Sticky/1, and reject unknown tuples. It must define Triangle
perimeter and stable-port geometry, Shape label upgrade/Undo behavior, and old-reader
capability rejection before changing validators. No new wire field is implied.

This would supersede only the named applicability/FC06 deferral, preserving V1 bytes,
V1 historical rejection fixtures, delete policy, hint presence and anchor semantics.
The human scope decision is accepted. The exact successor is materialized in
[Shape compatibility contract](FC05_CONNECTOR_SHAPE_COMPATIBILITY_CONTRACT.md),
pending independent design review. Do not downgrade Shapes or bypass validation.

## Interaction proposals for acceptance

This iteration's concrete successors are recorded in `FC05_STICKY_MUTATION_CONTRACT.md`,
`FC05_ROUTING_CONTRACT.md`, `FC05_PUBLIC_CONSUMER_CONTRACT.md` and
`FC05_CAPABILITY_SUCCESSOR_REVIEW.md`. They are review inputs, not independent
approval, Current Authority or P31 authorization.

Connector: enable its tool or drag a selected object's connection affordance; show a
transient route and nearest eligible perimeter candidate. Runtime owns snap decisions,
source/pointer/generation and commit/cancel. Valid release creates one object; release
on blank keeps a free endpoint. An invalid or stale candidate reports a reason and
cancels instead of silently attaching elsewhere. A tiny click/aborted drag creates
nothing. Reconnecting an end uses one whole `SetConnectorContent` replacement.

Sticky: propose 200×200 canvas units, pale yellow fill (#FFF59D), 12-unit padding,
Body 16 text using the existing admitted font catalog. Text/Select double activation
enters the existing text session; Ink keeps drawing semantics. Empty selected Sticky
shows a derived hint; an empty session creates no text child or History. First native
confirmed nonempty text inserts one RichText child under the Sticky, later edits
reuse it; IME finish/cancel rules from FC02 remain unchanged. Fill uses existing
PropertyBag fill field 0x100, not a painted child object.

The user confirmed fixed font size and interior wrapping, upward growth while
typing, derived ellipsis on manual shrink and selected-only expansion. The successor
proposes parent-derived wrapping and a restricted EditSticky operation for one-plan
text plus geometry publication. This proposed wire extension is subject to semantic
review; it is not a generic transaction, production allocation or P31 authorization.
A single Undo item alone is not proof of atomic apply.

## Routing risks and design inputs

See [library evaluation](FC05_ROUTING_LIBRARY_EVALUATION.md). libavoid is the strongest
external C++ candidate; no production dependency has been selected. The current
candidate for an in-house router is bounded deterministic rectilinear search, not an
already measured performance conclusion.

Scene already has dependency and spatial-index boundaries, but direct compilation
still contains a full rebuild. Also, dependency edges alone are insufficient: an
unattached object moved into a connector's corridor must invalidate that route.
Candidate invalidation therefore needs both endpoint dependencies and spatial route/
obstacle overlap, including old and new obstacle bounds and changes in clearance.
The same derived polyline must feed render, visual bounds/damage, index and hit tests.
Do not let Skia generate a second elbow that differs from Scene geometry.

A no-path/search-budget result must be observable. An obstacle-crossing straight
fallback cannot be reported as a successful avoided orthogonal route. Last-valid
preview is usable only while it remains valid for the current snapshot; input and
cancel processing must remain responsive. The exact fallback presentation and
clearance rules remain P11/P18 inputs.

Performance measurements must separate anchor resolution, candidate lookup, route
search and Scene publication. Use deterministic 100/1,000/10,000 obstacle corpora,
including unrelated-obstacle motion, overlapping targets, dense corridors and
high-degree targets. Record p50/p95, examined nodes/candidates, invalidated routes,
cache hits, allocations, per-pump work and retries on Windows and WASM. Numeric
budgets and acceptance hardware are not yet frozen; no speed claim is made.

## Next unresolved decisions

1. Independently review Shape compatibility and the capability-order legacy finding,
   then materialize the narrow Authority successor.
2. Review the concrete Sticky domain mutation, proposed defaults and guarded
   parent/child History; the overflow behavior itself is confirmed.
3. Review routing geometry, escape/no-route/work limits and public consumer parity.
4. Review the [P20 draft](FC05_P20_VERIFICATION_DESIGN_DRAFT.md) and freeze only
   after successor candidates are accepted. P31 has not been issued.

See the [narrow Authority delta proposal](FC05_AUTHORITY_DELTA_PROPOSAL.md) for the
exact old/new scope and preserved history.

No FC05 product code, SDK lock, Current Authority, historical Gate or Golden changes
are part of this candidate.

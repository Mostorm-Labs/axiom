# FC05 P14–P18 architecture review

Status: **READY_WITH_FINDINGS / INDEPENDENT_REVIEW_PENDING**  
Baseline: `d6a47d53dfc6fa5381e20a528ecceb1e2878fa55`  
Branch: `codex/fc05-architecture-p20`

Role: P14–P18 architecture. Authority: accepted FC05 product scope and the linked
P12/P13 successor candidates, whose pending status is inherited here. Objective:
assign state/commit/route/platform owners. Non-goals: product implementation,
generic transaction or routing subsystem. Required analysis/output: ownership,
flow, public consumer, failure and bounded work contracts. Quality/evidence gate:
upstream candidates must be accepted before this becomes execution authority.
Handoff: author-side review material to P21/P20, not automatic stage promotion.

This is an author-side architecture package. It is not an independent Control
Review, P20 freeze, P31 authorization, implementation result, or Gate verdict.

## Ownership and boundaries

- Semantic owns Connector endpoint intent, Sticky dimensions/placement, child
  identity, object versions, capability requirements and canonical operations.
- Interaction owns pointer/source/generation sessions, snap candidate state,
  crop-like transient route previews, Sticky text sessions and History intent.
- Runtime/Text owns RichText layout, caret/hit geometry, overflow and the
  container-derived layout context. It does not persist a routed polyline.
- Scene owns one derived connector polyline and uses it for render, visual
  bounds, spatial index and hit testing. It must not ask Skia to produce a
  second route.
- Composition/RuntimeFacade owns owner-thread admission, publication and
  platform-neutral receipts. Debug UI and ordinary consumers use this same
  facade; neither may mutate a store or call a private runtime lane.
- Windows and Web collect physical facts and execute returned draw work. They
  do not implement snapping, layout, routing, History, or canonical writes.

## Sticky aggregate and layout

The canonical aggregate is a Sticky/1 parent with zero or one direct RichText
child. An empty Sticky is valid. The child retains its ObjectId, kind/version,
document and any persisted text state; it is never replaced by an embedded Shape
label. The parent owns fill, dimensions, transform and placement.

The first-slice presentation policy is now fixed from the user's choice:

1. text size stays fixed;
2. the parent interior is `width - 2*padding` by `height - 2*padding`;
3. text wraps within the interior and grows upward when confirmed text requires
   more height;
4. a manual shrink keeps the text and size, clips the derived view with an
   ellipsis, and exposes an expand affordance only while the Sticky is selected;
5. the empty selected Sticky shows a derived input hint; an unselected empty
   Sticky shows neither hint nor affordance.

The [Sticky successor](FC05_STICKY_MUTATION_CONTRACT.md) proposes exact defaults,
layout precedence, a guard and the narrow domain operation. These are review
candidates, not implementation-authorized allocations. A parent
container layout context overrides the child's derived wrapping context while
the child is mounted under Sticky. The child's canonical `RichTextContent.layout`
is not rewritten on every parent resize. This prevents viewport/DPI changes from
mutating text bytes and keeps a child reusable in public consumers.

The existing Runtime/TextLayout seam therefore needs a container-context entry
point (or equivalent owner-only override). It must not change the public
RichText schema or create a second text measurer. Caret, selection, hit and
render all consume the same resolved snapshot.

## Sticky mutation and atomicity finding

Current `CanvasEditingRuntime::submit` rejects batches larger than one operation,
and `EditorHistory` stores one forward operation plus inverse operations. Current
History also omits `SetConnectorContent`, and its size expectation omits Sticky.
These are implementation findings, not permission to weaken atomicity.

The required completion contract is:

- the Sticky editor uses the proposed guarded `EditSticky` domain operation for
  confirmed text and parent geometry; manual resize uses its keep-child action;
- a confirmed auto-grow that changes both child text and parent size is one
  owner commit boundary with one History entry and a fully prepared preimage;
- preparation validates child identity, parent dimensions, text guard and the
  resulting layout before any store, generation, ledger or Scene publication;
- any failure leaves both parent and child unchanged; Undo/Redo restores both
  records and preserves their identities;
- no platform or Debug UI sequence of two independent submissions may claim
  atomicity.

The successor proposes OperationKind/tag17 with restricted parent geometry plus
keep/create/edit/remove/restore of the one child in a single PreparedApplyPlan.
Existing low-level SetObjectSize/EditRichText keep their single-object meaning.
No new global Transaction or Registry is proposed; the narrow domain wire
extension is explicitly subject to semantic review before any production edit.
Canonical auto-grow is not silently deferred from the user's requested scope.

## Connector route ownership and failure behavior

Connector content stores endpoint intent and routing mode only. The route provider
receives an immutable scene snapshot, resolved endpoint geometry, obstacle query,
clearance policy and a cancellation/budget token. It returns a derived polyline,
endpoint normals, a deterministic route key and a diagnostic.

Orthogonal output must be axis aligned, use deterministic obstacle ordering and
tie breaks, and avoid the interiors of admitted inflated obstacles. Endpoint
escape is solved before search; rotated/skewed targets are transformed through
the target inverse/world transform rather than approximated by pixels.

`NoRouteInDomain`, `BudgetExceeded`, `InvalidEndpoint` and `Cancelled` are observable
non-success results. A crossing straight line is never reported as an avoided
orthogonal route. A last-valid preview may remain visible only when its snapshot
key still matches; it is discarded on a changed snapshot, loss or cancellation.
The [routing candidate](FC05_ROUTING_CONTRACT.md) specifies conservative AABBs,
8-unit clearance, cardinal escape, finite domains, warning presentation and work
caps. NoRouteInDomain does not claim a global proof over the infinite canvas.
Straight mode remains an explicit non-avoiding mode.

Route invalidation includes attached-target dependencies and unrelated obstacle
motion that intersects the old or candidate corridor. Use both old and new
obstacle bounds. A distant mutation may retain a cache entry only when the
spatial query proves no corridor/clearance overlap.

The same route object feeds render, bounds/damage, index and hit/arrow geometry.
Scene generation changes caused only by route derivation do not create Semantic
operations, History entries or canonical generation changes.

## Platform and public consumer matrix

The shared RuntimeFacade must expose typed Connector and Sticky intents/receipts
for ordinary consumers, including create/update/commit/cancel, selected-object
readback, child/session identity and explicit failure diagnostics. Debug UI uses
those methods only. Windows and Web map their pointer/touch facts to the same
request fields (`source`, `pointer`, `generation`, one coordinate transform).

Both adapters must reject stale/lost/closed writes before canonical apply and
must publish the same route/layout receipt states. They may differ in pointer
capture and native IME plumbing, but not in object identity, operation count,
undo semantics, no-route truth or derived geometry tolerance.

## Capability and compatibility treatment

The existing `connector-shape-attachment-v1` composite capability is required
only when an AttachedEndpoint targets Shape/2 or Shape/3. For a new successor
schema3 snapshot, required IDs are sorted by exact ASCII bytes. For example, a
Shape/3 triangle label plus a standalone RichText/3 and this Connector requires:

```text
connector-shape-attachment-v1
resource-manifest-v1
richtext-fixed-width-v2
richtext-formatting-v3
shape-richtext-label-v1
shape2-triangle-v1
```

Historical legacy schema3 vectors and their evidence remain unchanged. New
accepted writers emit sorted IDs; do not invent a permissive unsorted reader
exception. The [capability review](FC05_CAPABILITY_SUCCESSOR_REVIEW.md) identifies
the unresolved historical-order finding and the exact successor vectors. The
successor must reject an old reader that lacks
the composite capability, even if its old capability enum can read Triangle or
RichText separately. Snapshot bootstrap, typed restore, remote writable admission
and replay continuation all fail closed before publication when the composite is
missing. This is a compatibility decision to review; it is not a source change.

## Engineering evidence inputs (P18)

Measure anchor resolution, obstacle lookup, route search, Scene publication and
retry separately on Windows and Web. Use fixed 100/20, 1,000/200 and 10,000/1,000
obstacle/connector corpora with high-degree targets, dense corridors, unrelated
motion, no-route and continuous drag. Record p50/p95, examined nodes,
invalidated routes, cache hits, allocations and per-pump work. Separate any
DirectSceneCompiler full rebuild from router time.

The routing candidate proposes numeric work limits; these need review before
freeze. No hardware latency threshold is claimed. Timing is corroborative while
work limits, responsive cancellation and truthful exhaustion are blocking
correctness criteria. This distinction closes the draft's unbounded-performance
input without inventing an unmeasured frame-rate guarantee.

## Findings / handoff

1. **F1**: current validator and snapshot bootstrap are V1-only for Connector
   targets; successor tests and reader admission are required.
2. **F2**: current capability construction is not exact ASCII order when Triangle
   and label are both present; legacy bytes must be preserved while successor
   vectors are frozen separately.
3. **F3**: current History and submit seams do not prove Connector support or
   Sticky auto-grow atomicity.
4. **F4**: route invalidation/no-route truth and numeric work budgets are not
   implemented or measured.
5. **F5**: ordinary-consumer typed facade and Windows/Web parity are specified in
   the [public contract candidate](FC05_PUBLIC_CONSUMER_CONTRACT.md), to be accepted
   and implemented before completion of the later product package.

Handoff: an independent reviewer should examine this package with the existing
FC05 design and compatibility contract. On acceptance, update the exact P20
obligations and only then materialize P31. On rejection, repair the owning
model/architecture document rather than coding around it.


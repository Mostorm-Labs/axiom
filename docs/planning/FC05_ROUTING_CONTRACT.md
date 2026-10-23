# FC05 bounded orthogonal routing candidate

Status: **P11/P14–P18_CANDIDATE / REVIEW_PENDING / UNMEASURED**.
Consumes the unchanged anchor mathematics in the Shape compatibility contract.
Role: common derived-route contract. Objective: deterministic useful avoidance,
truthful failure and bounded owner work. Non-goals: global layout, edge bundling,
manual bends, curves, minimal-crossing optimization or G5. Output: rules and work
limits below. Gate: independent geometry oracle and public failure observations,
not source-text assertions. Handoff: P20 candidate after semantic review.

## Obstacles, endpoints and escape

Admitted obstacles are live visible Shape/1,/2,/3, Image/1 and Sticky/1 boxes,
including unconnected ones. Hidden objects are excluded; locked visible objects
remain obstacles. Image decode/opacity does not change its box. Ink, Connector,
RichText (including Sticky children), Group and future kinds are not obstacles
in this slice. No claimed support is inferred from a renderer handle.

Use the world AABB of each admitted canonical transformed box, conservatively
including ellipse and triangle empty corners. Auto/port endpoints still use the
true analytical perimeter. Clearance of route centerline is
`8 + connectorStrokeWidthWorld/2 + obstacleStrokeOutsetWorld` canvas units, using
the admitted stroke geometry/max affine scale; no screen zoom/DPR dependency.
NaN/non-finite geometry returns InvalidGeometry, not an empty-obstacle success.
The simplification is deliberately conservative and may route farther around a
triangle; it does not claim silhouette-tight shortest paths.

At an attached endpoint, enumerate cardinal directions +X,+Y,-X,-Y whose dot
with an analytical outward world normal is positive (at a corner any supporting
outward normal suffices). Each candidate stub goes axis-aligned from the exact
anchor to the corresponding inflated own-box boundary. Exempt only that stub
from its own target's box, never from other objects. The rest of the path treats
source and destination as ordinary inflated obstacles; it cannot pass through
them. Try admissible escape combinations using the same deterministic cost.
Singular target inverse is InvalidEndpoint. A free point inside an obstacle has
NoRouteInDomain; it is not silently moved or attached.

Distinct anchors on the same target may form a self-loop around that target;
the same-point/same-anchor zero-length case returns Degenerate and cannot create
an object through a click. Keep existing canonical legality separate from this
derived status; loading an old degenerate connector does not delete it.

## Search policy and determinism

Proposed implementation is a bounded rectilinear visibility search within existing
Scene ownership, not a new module or dependency. A simple full Hanan-grid Dijkstra
on small fixtures is the independent oracle; production may construct it lazily.
libavoid remains a research alternative requiring separate adoption qualification.

Try endpoint-box domains inflated by 64, 256 and 1024 world units, in that order.
Spatial-query complete admitted obstacles intersecting each domain; never truncate
a query and claim avoidance. Exhausting all domains proves only NoRouteInDomain,
not no mathematical path anywhere on an unbounded canvas. The result includes
the searched final domain. Cap/search failures give BudgetExceeded separately.

Candidate edges connect visible adjacent coordinate intersections; paths cannot
cross the open interior of inflated rectangles. Boundary contact is allowed.
Search uses f64, no fast-math; stable ordering uses full ObjectId bytes and numeric
coordinates. Compare routes by (Manhattan length, bend count, lexicographic complete
vertex sequence by x then y). Enumerate escape alternatives deterministically.
Remove duplicate/collinear interior points before publishing; preserve exact
anchors and terminal stub geometry. For cross-platform near-ties, quantize derived
search coordinate decisions to 1/64 world unit (round obstacle bounds outward);
insert exact endpoint/stub-axis coordinates so no diagonal rounding segment is
introduced. Do not quantize canonical endpoint/transform bytes.

The one immutable result includes status, polyline, terminal directions, searched
domain, connector-content revision, endpoint/obstacle revision key, algorithm/
policy revision and work counters. Render, bounds/damage, spatial index, hit and
arrow all consume it. No downstream renderer computes an independent elbow.
Route-only work changes no Operation/History/canonical generation.

## Bounded owner work and cancellation

These are proposed hard **work** limits for review, not measured latency claims:

| Scope | Limit |
| --- | ---: |
| Obstacles materialized per solve, across all domains (distinct IDs) | 256; observing the 257th is BudgetExceeded |
| Search states/heap entries retained per solve | 8192 each |
| Expanded states per solve across escape/domain attempts | 8192 |
| Segment-obstacle predicates per solve | 65536 |
| Quantum | at most 1024 expansions and 8192 predicates |
| Global router owner-pump work | at most 4 quanta; round-robin canonical jobs with active preview first |
| Live search arenas | 1 active preview + 4 canonical jobs; others remain queued |
| Cached route entries | at most 4096; 256 simplified vertices per result |

An attempt shares the solve's total limits; retries do not reset counters. Overflow
is BudgetExceeded, never a partial success. Query enumeration is bounded too;
count examined candidates separately from returned obstacles. The spatial index
must expose an interruptible bounded query (or qualify an existing equivalent)
before a P31 performance claim. A single unbounded index traversal cannot hide
behind the 256 returned-item cap. Proposed query-node cap is 8192 per solve and
1024 per quantum. Persistent queue is keyed by full Connector ObjectId, retains
one latest dirty job per live Connector, and is cleaned on deletion/loss/close.
Loaded document size remains subject to existing release limits.

Coalesce obsolete pointer moves to the latest solve request while preserving
Down/Up/Cancel order and identity. Check cancellation/staleness at quantum boundaries;
the next owner opportunity handles cancel before another quantum. An optional
4 ms wall-time pump yield may pause work, but must not change the deterministic
final route or restart totals. Record it as a scheduling measurement, not a route
failure. Input/cancel must not wait for all dirty canonical connectors.

## Publication, invalidation and no-route UX

Creation preview and reconnect carry full session/scene identities. On release,
revalidate against current state. Ready commits one InsertObjects or whole-content
SetConnectorContent; stale candidate, unavailable resource geometry, no-route or
budget failure rejects with no canonical edit. Keep the previous connector on a
failed reconnect. New short-click/cancel creates nothing.

After an already accepted target/obstacle move, route failure cannot roll back the
user's target mutation. Publish a current route-status record. For NoRouteInDomain,
BudgetExceeded, InvalidEndpoint or Pending, do not render or hit an old successful
path as current. Show a derived dashed warning chord with endpoint handles while
selected, marked non-avoiding; it is only a diagnostic affordance, not the accepted
orthogonal route. Its hit maps to the Connector and uses its own explicit bounds.
Unselected failed connectors show that same warning style so failure is visible.
The previous frame may remain physically present until the next frame; it is not
an authorized stale geometry readback. Retry only on changed inputs or explicit
user retry; idle failure must not self-trigger infinite frames.

Maintain endpoint dependency edges plus route/search-domain spatial dependencies.
Insertion, removal, visibility, size, transform, clearance/style or parent changes
check both old and new obstacle bounds. Revalidate a cached polyline on a newly
intersecting obstacle even when unrelated by attachment. Include failed-route
searched domains so removing a blocking object can recover. Cache validity never
depends solely on global Semantic generation. Parent/child local transforms must
be composed once before bounds, snap and layout consumers query them.

## Qualification and limits

Freeze native/WASM comparison as equal status and normalized route topology
(ordered H/V runs and obstacle-side choices), coordinate tolerance
`abs(a-b) <= 1e-5 + 1e-7*max(abs(a),abs(b))`, and exact canonical bytes. Include
boundary ties and quantization cases; never treat an obstacle crossing within the
numeric tolerance as acceptable. Exact integer fixtures use exact intersection.

Run 100/20, 1000/200, 10000/1000 corpora and separate anchor/query/search/Scene
costs. Work-limit conformance and truthful failure are blocking correctness
obligations. p50/p95 timing, cache rates and optional libavoid comparisons are
corroborative until hardware-bound thresholds are accepted; no unmeasured claim
of smooth 60 Hz is made. The 256-obstacle cap must be surfaced in UX/receipts, not
presented as universal diagram routing capability.

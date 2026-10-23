# FC-05 Connector + Sticky P20 Verification Design

Status: **DESIGN INPUT — proposed freeze for independent review; not executed and not P31 authorization**.

Baseline: `178173a03acd6f458991a69227ce3fdccaaffedd` (FC03 merge).

This document turns the confirmed first-slice scope into proof obligations. It does
not create a new ObjectKind or schema field and does not decide unresolved Shape
version applicability. Any authority conflict returns to P12/P13/P21 before
implementation.

## Blocking proof obligations

| ID | Requirement / invariant | Failure mode | Oracle and fixture | Evidence / class |
| --- | --- | --- | --- | --- |
| CON-01 | Free endpoints preserve canonical world points; attached endpoints preserve target identity and anchor kind | Derived route or renderer proxy becomes endpoint truth | Canonical snapshot round-trip with both endpoint variants and both routing modes | Semantic codec + snapshot digest; **blocking** |
| CON-02 | Invalid target identity, target kind/version, anchor, or routing value is rejected atomically | Partial Connector is inserted or one endpoint is silently repaired | Stateful validation matrix including missing target, non-connectable target, Shape v2/v3 decision cases, invalid port and invalid enum | Receipt and unchanged store/history/generation; **blocking** |
| CON-03 | Connector has no hierarchy children and target deletion cascades to references | Deleted target leaves dangling connector or illegal child graph | Delete-closure corpus with Shape, Image, Sticky and Connector references | Before/after graph and replay result; **blocking** |
| CON-04 | Straight route resolves both endpoint geometries deterministically | Attached endpoint remains stale or route changes for identical revisions | Fixed target geometry corpus, repeated projection, Windows/Web differential digest | Route key and polyline digest; **blocking** |
| CON-05 | Orthogonal route is deterministic, axis-aligned, and avoids forbidden obstacle interiors | Elbow crosses obstacle, oscillates, or differs by platform | Corridor, corner, overlap and no-path obstacle corpus | Polyline validity, route key and fallback diagnostic; **blocking** |
| CON-06 | Moving a target invalidates only affected Connector routes | Unrelated connectors rebuild or affected route stays stale | Dependency graph corpus with affected/unaffected targets and move/replay sequence | Affected set, cache hit/miss and route digest; **blocking** |
| CON-07 | Derived route refresh never mutates canonical Connector content | Frame refresh increments SemanticGeneration or creates history | Route recomputation under target movement and redraw | Snapshot, generation and operation ledger unchanged; **blocking** |
| CON-08 | Connector hit testing uses derived route and zoom-aware tolerance | Visible connector cannot be selected or nearby object steals hit | Straight, elbow, arrow and near-miss pointer corpus at multiple scales | Hit oracle with expected identity; **blocking** |
| STK-01 | Sticky accepts fill/background and width/height as its own object state | Fill or size stored in unsupported child or resize corrupts text | Property/content validation and size replay vectors | Canonical projection and rejection receipts; **blocking** |
| STK-02 | Sticky has zero or one direct RichText child; second/non-RichText child rejects atomically | Duplicate, orphan or illegal child after retry/replay | Insert, edit, restore, remote apply and duplicate-child corpus | Hierarchy validator and unchanged-state assertion; **blocking** |
| STK-03 | Sticky RichText uses existing text session and stays inside derived bounds | Host constructs second text model or resize fails to reflow | Create/edit/resize/Undo/Redo through RuntimeFacade | Object graph, layout bounds and history receipt; **blocking** |
| STK-04 | Sticky move/resize and Connector attachment replay identically | Parent/child or attached connector diverges after replay | Mixed Sticky + RichText + Connector operation log | Snapshot equality and route digest; **blocking** |
| PLAT-01 | Windows and Web expose the same canonical operations and derived route for one input snapshot | Bridge changes endpoint semantics or route output | C++/WASM projection and route differential corpus | Canonical and route digest equality; **blocking** |
| PLAT-02 | Platform input sessions cancel without canonical mutation and report frame demand | Cancel/lost surface leaves half-created object or misses redraw | Down/move/up/cancel/lost-surface sequences on both hosts | Receipt, ledger and frame-demand trace; **blocking** |

## Performance and routing-library qualification

The route provider reports obstacle candidates, affected Connector count, cache hit
rate, cancellation count, route latency p50/p95 and frame retry count for the same
corpus: 100 obstacles/20 Connectors, 1,000/200, and 10,000/1,000 with one target
move, 10% target moves and continuous drag respectively.

The first implementation may use a small deterministic in-house bounded orthogonal
search behind `ConnectorRouteProvider`. `libavoid` remains an optional benchmark
candidate; it cannot become a hard dependency until its pinned revision,
LGPL/commercial licensing choice, Windows static build, WebAssembly build, ABI and
route-digest parity are independently qualified. ELK/elkjs and Excalidraw remain
reference/experimental inputs, not shared Runtime dependencies.

Until P18 freezes numeric budgets, these metrics are **corroborative**. The blocking
engineering invariant is bounded per-sample work with cancellation and no mandatory
full-document rebuild on every pointer sample.

## Exact execution surfaces

- Semantic: `connector_stateful_validation_test`, codec/snapshot projection tests,
  delete-closure and hierarchy capability tests.
- Scene: dependency invalidation, route determinism, obstacle intersection and hit
  geometry tests using immutable object snapshots.
- Runtime/host: typed RuntimeFacade connector/Sticky commands, input cancellation,
  frame demand and Windows/Web bridge parity tests.
- Performance: standalone route-provider corpus runner; do not mix route latency with
  Scene full-rebuild latency.

## Required decisions before P31

1. Resolve Connector applicability for Shape v2/v3 through P12/P13/P21.
2. Freeze creation gesture, default Sticky fill, padding/empty-state presentation,
   failed-drag behavior and whether Connector labels are in this slice.
3. Freeze orthogonal obstacle boundary/endpoint-gap rules and numeric P18 budget.
4. Convert this proposal to a versioned P31 evidence contract only after independent
   review accepts the decisions above.

No P31, P32, DEV Gate or physical-platform PASS is claimed by this document.

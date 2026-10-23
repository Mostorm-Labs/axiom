# FC-05 P20 verification design draft

Status: **DRAFT / NOT_FROZEN / NOT_EXECUTED**. Baseline:
`178173a03acd6f458991a69227ce3fdccaaffedd`.

This is downstream input from modeling, not a completed P20 stage. The accepted
contracts must determine the final oracles, fixtures, commands and classification.
No proposed target below is claimed to exist or pass.

## Stage contract

Role: P20 design input. Authority: user-confirmed slice and
[modeling candidate](FC05_CONNECTOR_STICKY_DESIGN_CANDIDATE.md).
Objective: detect material failure modes at public and canonical boundaries.
Non-goals: rewriting historical Goldens, Gate decisions or physical outcomes.
Required analysis/output: existing coverage, residual gaps, independent oracle,
fixture, executable command and blocking justification per obligation.
Quality gate: do not freeze undefined geometry, atomicity or performance limits.
Handoff: resolve P11–P18 contracts before turning this draft into a P31 evidence lock.

## Existing independent coverage

- `runtime/semantic/tests/connector_stateful_validation_test.cpp`: V1 reference,
  kind/version, ports, whole-content rejection and store parity. It intentionally
  rejects Shape/2. No new claim of extended-version coverage is made.
- `runtime/semantic/tests/fc01_shape2_apply_test.cpp`: current negative rule for
  Triangle attachment. Preserve the old baseline and historical evidence; add
  versioned successor tests if applicability changes.
- Semantic codec/snapshot and delete-closure tests: canonical endpoint representation,
  no dangling restore and cascade-delete semantics already have coverage.
- `runtime/semantic/src/hierarchy_capability_validation.cpp` and hierarchy tests:
  Sticky allows at most one RichText child, including staged operations.
- Scene dependency/full-incremental tests and render elbow tests cover partial
  projection behavior. They do not prove attached endpoints, avoided paths, spatial
  obstacle invalidation, live Sticky editing or ordinary public-consumer behavior.

Re-running these tests on a second platform does not create an independent oracle.
New tests should close the named gap instead of duplicating already covered logic.

## Proposed obligations and unique detection value

| ID | Requirement and invariant | Failure / impact | Residual gap and oracle | Proposed classification / justification |
| --- | --- | --- | --- | --- |
| CON-01 | Endpoint truth is connector-local free point or target+anchor intent; route points remain derived | Wrong coordinates or persisted route corrupt replay | Independent transformed endpoint fixtures: connector translation/rotation; target scale/skew and hierarchy; canonical bytes remain stable on route refresh | Blocking only for the uncovered coordinate/resolver cases; existing codec round trips are protection/corroborative |
| CON-02 | Accepted target tuples validate uniformly; unsupported readers/versions reject atomically | Connected Shape gains a label and becomes invalid or silently detached | New-version insert/restore/reconnect; Shape/1→/3 label edit and Undo; wrong target/unknown tuple; unchanged ledger on rejected apply | Blocking after successor review; detects a version-transition gap absent from V1 tests |
| CON-03 | Auto hint presence, dynamic center reference and stable ports obey exact resolver | Drift, recursive resolution, wrong-side attachment | Independently calculated rectangle/ellipse fixtures, exact coincident +X, both-attached centers, optional hint inequality, all four ports; Triangle vectors follow the materialized compatibility contract, pending design review | Blocking; a codec check cannot detect wrong resolved coordinates |
| CON-04 | Orthogonal output is axis-aligned and avoids admitted obstacle interiors; no-path is truthful | Crossing route falsely reported valid | Independent segment/interior intersection checker over corridor, corner, nested/overlap, same target and no-path fixtures; budget exhaustion is explicit, never an ordinary avoided-route success | Blocking; no current oracle detects avoided-route correctness |
| CON-05 | Route invalidation includes endpoint dependencies and unrelated obstacle motion | Stale route after an unconnected object blocks it | Move/insert/delete a third obstacle across the old route; check old/new damage and compare a fresh reference solve; unrelated distant changes retain cache when dependency region permits | Blocking for correctness; exact optimization/cache hit target stays corroborative until P18 |
| CON-06 | One derived polyline drives pixels, bounds/index and hit; arrow follows final segment | Visible path cannot be selected, stale damage or arrow mismatch | Polyline segment near/miss hit vectors and measured bounds; an independent pixel probe catches render-vs-Scene divergence at native and WASM boundaries | Blocking; old free-line/elbow unit tests do not cover the integrated path |
| CON-07 | Target mutation creates only its own canonical edit; rerouting adds no mutation | Frame/route refresh adds Operations or History | Snapshot and ledger before target edit, after target edit, and after repeated route refresh; only the last interval has zero SemanticGeneration delta | Blocking for new route/session flow; existing canonical codec coverage is reused |
| CON-08 | Creation/reconnection commits once; abort/stale/loss cancels; delete/Undo retain reference integrity | Half-created object, duplicate edit, dangling relation | Real Facade/owner input sequence, operation/snapshot/history observations, repeat receipt and cascade restore; failure injected before publication | Blocking for new session and History path; existing Semantic closure corpus remains protection |
| STK-01 | Empty Sticky is valid; first confirmed text creates exactly one child | Orphan/duplicate text or empty draft pollution | Public consumer create/edit/cancel/retry and staged duplicate child; independent graph and history counts | Blocking for new Runtime flow; existing hierarchy rejection tests already cover Semantic invariants |
| STK-02 | Sticky parent/child layout and transform obey the accepted resize contract | Text drifts, scales unexpectedly or parent/child publish half changed | Move/resize/replay with retained font size and child identity; failure between preparation and publication; compare native layout/hit/caret geometry | Blocking once width/overflow/atomicity contract is accepted; current source does not establish multi-Operation atomicity |
| STK-03 | Fill, text format, selection and History use public RuntimeFacade | Debug UI works while ordinary consumers cannot use feature | Small ordinary consumer performs same actions as Debug UI and compares typed readback/History/render facts | Blocking; uniquely detects host/private-internal dependency gaps |
| PLAT-01 | Windows/Web consume identical core semantics and cancellation/lifecycle rules | Host owns a second router/text state or permits lost writes | Same serialized intent and object corpus across native/WASM, canonical equality, route topology and quantized coordinate comparison with frozen tolerance | Blocking; shared C++ source alone does not prove host integration |

## Determinism and RED preconditions still to freeze

Route tie-breaking must specify sorted full ObjectId/obstacle ordering, stable costs
and path simplification. Do not require raw floating-point bit equality between
native and WASM without an accepted numeric policy; compare canonical bytes exactly,
route topology exactly and derived coordinates under explicit tolerances.

Known-missing baseline behaviors must be RED through genuine public/Scene seams:
attached endpoint moves, third-obstacle invalidation, avoided-route hit/render parity,
and Sticky first-text/resize. A test that passes the known-incomplete implementation
cannot be used to certify that missing behavior. Preserve observed RED and GREEN
artifacts after the oracles are accepted, not fabricated source-text checks.

## Performance qualification proposal

Use fixed seed/input snapshots for 100 obstacles/20 connectors, 1,000/200 and
10,000/1,000. Include a high-degree target, third-obstacle motion, dense corridors,
no-path and continuous drag. Report the measurement device/compiler/WASM engine,
warmup, sample count, p50/p95, anchor time, spatial lookup, search, Scene publication,
examined nodes, candidates, affected routes, cache hits, allocations and retries.

Separate router work from DirectSceneCompiler full rebuilds. A generation-only
change must not cause every route to be solved again. Candidate budgets must be
chosen from measured workloads before P31; neither “fast” nor a blanket no-full-rebuild
promise is a frozen acceptance threshold today. No-route/budget cancellation and no
canonical side effects are correctness obligations; timing data is corroborative
until a numeric budget and reference device are accepted. Optional libavoid benchmark
results are research, not a mandatory extra proof artifact or production dependency.

## Remaining freeze inputs

Shape applicability scope is accepted; the [compatibility contract](FC05_CONNECTOR_SHAPE_COMPATIBILITY_CONTRACT.md) defines Triangle geometry/ports and a composite reader capability, pending review. Remaining freeze inputs: Sticky width/
overflow/child transaction policy; snap/cancel/defaults; obstacle geometry, clearance,
endpoint escape and no-path presentation; routing order/cost/tolerance/work budget;
exact fixture identities and executable CMake/CTest/WASM command names. No Gate,
physical PASS or P31 authorization is produced by this draft.

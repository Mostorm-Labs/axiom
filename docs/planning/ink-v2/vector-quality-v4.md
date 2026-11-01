# Round/marker quality revision 4

User instruction: fix the audited fat-start/pointed-end and rough outline.
Repository github.com/Mostorm-Labs/axiom; anchor de27416190ec6285f05eedce410a3050043228fe
(ancestor), branch codex/sdk-shell-axis-bridge. Existing Runtime/Ink owns signals
and geometry; BrushCommitAdapter -> OperationEngine owns canonical mutation;
Skia consumes shared disposable contours. No SDK entrypoint/schema change.

New package revision 4 binds config v2, algorithm 4 and signal 3 for round/marker.
Earlier revisions retain their interpretation and encoding. Persist normalized
centerline and quantized effective pressure, not fitted/expanded contours/dabs.
Replay never filters or simulates pressure again. Existing tuning DTOs and codec
fields remain; unknown evaluator/signal tuples reject.

Start pressure uses the first up-to-ten normalized points (bounded startup raw
window 128), and the pinned PF distance recurrence. Early prefix is replaceable
until startup resolves. Stationary up does not create a speed/pressure sample;
small real terminal motion may recover pressure. Taper defaults to zero, caps on.
Streamline normalizes position; smoothing filters paired outline anchors at
size*smoothing separation followed by midpoint quadratic boundary segments.
Round contours follow the PF direction/cap approach; marker uses ellipse support
at its captured axis/flatness. Paired side anchors and bounded startup are explicit
adaptations, not a claim of all-input exact PF pixel parity.

Incremental contour compiler retains bounded anchor/neighbor context. Stable
chunks carry disposable contour patches; a fork compiles the replaceable tail.
Canonical/replay uses that same compiler. Adjacent patch pairs overlap by one segment to avoid AA seams. A trailing patch
is deferred until its neighbor exists; each pair is drawn exactly once across
all chunk/range boundaries. White scalar coverage unions in the Skia surface;
paint opacity is applied once. No full-history
per-append signal or contour evaluation. Sub-.25-unit positional changes with
less than .01 pressure change coalesce in the replaceable carrier, bounding dense
input copies without storing expanded curves. Timed pressure is resolved per raw
observation before normalization; explicit device pressure preserves endpoints,
including two-point input, and a tap does not synthesize diagonal geometry. PF oracle code/goldens and old resource
identities are unchanged.

EXECUTION_CLOSURE_CONTRACT
Required: endpoints pressure oracle, default zero taper, continuous boundary and
round caps; preview/canonical/ranged tile parity, persistence, prediction exclusion,
batching, work-count bound; matching native and Web builds; actual Chrome SDK/demo
checks and screenshots; focused CTest plus full CTest with known baseline failures
listed, types/boundary/diff checks. No publication/commit/push authorized this turn.
Forbidden: dependency/descriptor locks, old corpora, other material redesign,
platform geometry or performance subsystem redesign.
Blocking: new focused regressions or build/browser startup failure. Existing
22 baseline failure names remain separately recorded; real touch/Windows hardware
and visual approval are corroborative. Continue to local-review-ready or explicit
environment/authority/test blocker. No Gate/P34/release PASS.

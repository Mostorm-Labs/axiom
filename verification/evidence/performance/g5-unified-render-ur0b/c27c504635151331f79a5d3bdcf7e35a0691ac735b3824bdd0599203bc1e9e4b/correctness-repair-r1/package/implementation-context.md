# Offline correctness harness repair

Start at reconciled df214581; the f651523a cursor is its ancestor. Preserve the
accepted initialization repair, all recorded inputs and all CLOSED windows.

Runtime owns selection, snapping, geometry and semantic commits. The root SDK
facade exposes public state and opaque snapshots. The correctness driver obtains
public fixture state and pixels; the unchanged native oracle supplies independent
CPU reference/AA. Cold transfer and initialization provenance belong to the
verification harness, not product APIs or renderer semantics.

Current proof flow: worker init/state + raw native ownership -> initialization
basis -> decoder/canonical proof -> Python repair/resume admission. Keep that full
flow in the design preflight when introducing a second exact creation profile.
The Python reader is not in this work order's source allowance; do not bypass or
rewrite it to force admission. A real dependency/scope conflict returns the exact
blocker to control with evidence, rather than silently expanding the package.

Otherwise run the real old HUD RED, implement the fixed live subject/HUD recipe,
cold bounded batching, stages and exact additive creation profile; execute all
12 oracles and 17 invariant recompilations. Do not make OFF0 green by assumption.
No browser runs, builds, extra native probes, commit/push or Gate/P34 PASS. The
two existing malformed-snapshot native checks are allowed only as specified.

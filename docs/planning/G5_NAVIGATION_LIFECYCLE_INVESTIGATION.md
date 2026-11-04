# Long-line selection and navigation lifecycle investigation

User report: intermittent stutter while navigating after selecting a long line;
possible long-lived-page contribution. Execution base: `391bb8a6699b23c5458d6b21bbd6b64afb55b7fb`
in `codex/geometry-navigation-integration`, repository `Mostorm-Labs/axiom`.

This slice investigates existing owners and corrects measurement. DOM input goes
through the root Web SDK into Runtime camera/selection; Render owns disposable
navigation coverage; the provider submits pixels. No SDK API, render policy,
semantic state, dependency, existing Gate record or user document is changed.

Required closure: preserve the interactive page; use the same built SDKs and
real WebGL2 to distinguish input dispatch, RAF timing, display submissions and
canonical completions; exercise repeated selection/pan, touch, mixed objects,
idle and lifecycle interruption; retain exact source/artifact evidence and
report unresolved physical-platform conditions. A reproduction is not a fix.

## Finding

A healthy RAF cadence can coexist with several hundred milliseconds without
any new default-framebuffer draw. Checking RAF or browser long tasks alone misses
this failure. Exploratory mixed-scene wheel runs reached roughly 200–330 ms;
a cold 16K-point touch case reached roughly 512 ms. These are submission gaps,
not input-to-photon measurements or product thresholds.

The code provides a concrete mechanism:

1. `CanvasEditingRuntime::serviceNavigationPresentation` accepts the new camera
   and keeps requesting opportunities while navigation work remains pending.
2. `SkiaRenderer::renderNavigation` permits retained-image feedback only when the
   old image fully covers the new viewport. At unchanged scale, a nonzero pan
   reveals an uncovered edge, so that fallback is unavailable.
3. `SkiaNavigationTiles::render` submits composition only after `coverage(...)`
   succeeds for the whole viewport. Query/build and GPU backpressure can leave
   coverage incomplete over several opportunities. These return accepted work
   with `presented=false`; this is not a blocking input callback.
4. Input/RAF therefore continue while the last displayed camera remains fixed.
   Warm coverage can remove the gap; crossing another cold frontier can restore
   it. This explains a cache-dependent occurrence without proving it is the
   exact episode observed in the existing IAB page.

This is an identified presentation-continuity bottleneck. Do not weaken complete
coverage checks, remove GPU backpressure, raise synchronous work budgets or issue
early canonical receipts merely to improve a frame counter. A subsequent fix
needs bounded coverage/frontier preparation plus a continuity oracle that checks
actual submissions under cold, eviction and fast-pan conditions.

The deterministic mixed-scene control reproduces 267–333 ms gaps with **no
selection** as well. Selection is not necessary for this mechanism. In the two
60-second idle cases, RAF callback and draw counts stayed exactly unchanged and
the browser JS heap fell after collection; there was no evidence of a continuously
running idle loop. This bounded observation does not rule out longer-lived,
input-history, GPU memory or system-level effects.

## Measurement correction

The old harness could accept a receipt from selection during a subsequent input
loop and compute a negative settlement time. A failing unit test captures that
case. Both browser waiting and summary now require a completion observed after
input ends. The root SDK `onFrame` receipt exposes a count, not a camera identity:
even a post-input callback alone does not prove that exact camera pose. Evidence
retains this distinction explicitly.

## Reproduction tool

`verification/runners/g5/navigation_lifecycle.mjs` consumes a copied root SDK
package and the existing sampling fixture. Default cases use a 636x914 logical
surface, DPR2, mixed image/table/sticky/brush content, public property readbacks,
four wheel events per RAF or two touch contacts, interrupted selection, six
repeated routes, 60 seconds of page idle and CDP frozen/active recovery.

RAF/GL wrappers forward real calls. The idle interval schedules no sampler RAF
or page timer, so a stopped count measures actual SDK inactivity. No private
WASM access or consumer debug API is introduced. Public property readbacks do
not reproduce the full Demo DOM/layout cost. CDP freezing does not establish
physical trackpad, OS sleep/lock, GPU eviction or hours-long leak behavior.
The current fixture counts canonical-canvas default-framebuffer draws separately
from preview work. Deterministic UUIDs bind the selection-control fixtures; the
initial runs retain their original explicit source hashes and random brush seeds.
They are independent reproductions, not matched pixel comparisons across builds.

Run from the repository root, without simultaneous builds:

```sh
node verification/runners/g5/navigation_lifecycle.mjs \
  out/g5-prepared-install/js out/g5-navigation-lifecycle/off 60000 6

AXIOM_NAV_MODES=wheel AXIOM_NAV_SELECTED=0 \
node verification/runners/g5/navigation_lifecycle.mjs \
  out/web-sampling/sdk/web out/g5-navigation-lifecycle/unselected 0 3
```

Historical matrix artifacts remain preserved. New results live separately in
`verification/evidence/performance/g5-navigation-lifecycle/`.

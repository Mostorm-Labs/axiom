# Mac trackpad pinch: navigation feedback correction

Repository: Mostorm-Labs/axiom. Execution base:
`b86cc92a38b408dcdb0d071983af77464f77fd0a`. This is a corrective slice under
G5 v0.3 and `G5_WORLD_NAVIGATION_TILES.md`, authorized by the user's report of
continued Mac trackpad pinch stutter. Existing shared Runtime camera/input and
semantic commit paths remain the owners; Render owns disposable navigation
coverage; Web adapts presentation. No public SDK API/ABI, brush geometry,
dependency lock, semantic authority or historical evidence changes.

Three causes were reproduced, with failing tests before implementation:

1. DPR2 at 800×500, just above scale 1, selected 77 target and 24 parent cells.
   Their conservative three-allocation leases exceed the 32MiB navigation
   budget. The old path rejected navigation and ran full Direct rendering in
   the gesture opportunity. Feedback now coarsens its private scale level until
   both planes fit. It still refuses an impossible budget, and approximate
   feedback never qualifies canonical pixels.
2. Cold world tiles held the previous display until complete coverage. A retained
   exact image now supplies immediate camera feedback only when its transformed
   bounds cover the entire requested viewport. Outside that coverage, world
   tiles remain mandatory; a zoom-out cannot reuse a cropped viewport image.
   Provider/view/Scene/semantic/metrics/surface stamps are still checked. Same
   camera fallback is dirty-gated, and no canonical submission is created.
3. Web placed disposable navigation presents in its single canonical GPU fence
   slot. Delaying a real fence caused only 1 of 24 continuous camera updates to
   draw. Navigation-only presents now rely on ordered provider flush/submit
   without a CPU completion fence. Canonical and ARC preview work retain their
   fences, exact identity matching and completion semantics. There is at most
   one owner opportunity per RAF; this introduces no worker or private queue.

The browser regression deliberately delays completion polling without faking
success. After the fix it observes 24/24 default-framebuffer draw opportunities,
zero navigation-only fences/polls, no canonical receipt during navigation and
an exact receipt after idle. It uses the public root JS SDK with matching WASM.

Diagnostic Chrome/ANGLE Metal Apple M2 observations, 64 strokes × 16 samples,
800×500 DPR2, 120 synthetic ctrl-wheel events at RAF cadence:

| Run | Default-framebuffer draw opportunities | Largest draw gap | Max RAF CPU | Exact receipt after input |
| --- | ---: | ---: | ---: | ---: |
| delivery, zoom in then out | 119/119 | 18ms | 6.3ms | 1198.9ms |
| delivery, zoom out then in | 117/119 | 50ms | 18ms | 1947.2ms |

These are single engineering diagnostics, with other regression processes
running during parts of the session; not controlled speedup ratios, GPU timers,
physical trackpad/input-to-photon evidence, or a 60/120Hz guarantee. GL draw
submissions are distinct from monitor presentation and canonical `onFrame`.
The earlier staged candidate had a 133.5ms draw gap; the original DPR2 diagnostic
was interrupted after 15s of profiling. Those candidate observations are retained
for diagnosis, not presented as a controlled before/after benchmark.

Remaining limits: newly exposed cold world coverage can still take multiple
opportunities; idle prewarming/overscan and GPU workload backpressure remain
future work. Exact CPU compact-coverage settlement still takes roughly 1.2–1.95s
in these runs. This patch removes proven gesture stalls, not that exact-render
cost. Unsupported content still uses the existing exact fallback. Navigation
images never issue CanonicalVisible or retire ARC.

Verification: native G5 components 25/25; actual WebGL cache/material/coverage
probe passes; root SDK external browser, pending-input/resize/context-loss
handoff, Shell/Axis and demo pass; navigation browser regression passes. Runtime
boundary and workspace checks pass. Full CTest is 214/236: all 22 failure names
match the prior evidence, including G3HeadlessCanvasDemo and FC02Text timeout.
The failure list is in the evidence bundle. Node workspaces retain C8's exact
source generator `spawnSync git ENOBUFS` failure (semantic CLI 137/138); the two
SDK Node checks pass. No old fixture/authority was rewritten to hide failures.

Evidence: `verification/evidence/performance/g5-pinch-feedback/`. Its source
SHA256 binds the dirty implementation and artifact hashes bind the actual SDK.
BuildInfo is regenerated from this execution base (dirty), not represented as
a clean release. The independent interactive copy is served at
http://127.0.0.1:4188/example.html; the existing user tab was not reloaded.
Engineering classification: PASS_WITH_FINDINGS. Physical platform evidence
remains BLOCKED_EVIDENCE; no official G5/P34 PASS is issued.

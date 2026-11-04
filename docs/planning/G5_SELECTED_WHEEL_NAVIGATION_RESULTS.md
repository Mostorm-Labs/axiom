# Selected-object wheel navigation correction

Scope: `codex/g5-navigation-chunk-reuse`, based on
`cd6f0e1c`. This is an engineering regression result for the user-reported
selected-image trackpad slowdown, not G5/P34 platform acceptance.

Mac trackpad pan arrives through DOM `wheel`, and pinch arrives through
`ctrl-wheel`. The root Web SDK forwards these to existing Runtime camera
commands. These events have no touch contacts, so `viewportClaimed()` is false.
The selection-overlay guard therefore rejected navigation feedback whenever an
object was selected and sent camera changes to exact canonical preparation.
Image decoding/SkImage caches could remain warm while this heavier route ran.
Earlier contact-pinch tests did not cover this path.

Runtime now recognizes a camera generation differing from the last presented
exact frame for this eligibility decision. Existing Scene, surface, metrics,
ARC handoff and active-edit guards remain. Navigation remains feedback only.
Renderer records the content snapshot before selection chrome, retaining it
across selection frames. Runtime refreshes selection geometry at the navigation
camera; both tile composition and complete-viewport retained feedback use the
same lightweight chrome drawing helper. Old handles are neither scaled from
the snapshot nor drawn twice. No public JS/DLL API or semantic format changes.

Regression proof:

- Restoring the original eligibility guard makes `G5Navigation` fail with
  `selected wheel navigation rejected`.
- Omitting selection geometry refresh fails the current-camera chrome assertion.
- Native pixel comparison checks retained and tile feedback chrome against
  Direct, with one canonical submission only. Repeated pan/zoom preserves
  selection and document generation and subsequently reaches exact settle.
- Nine related tests pass: C0CanvasInteractionCoordinator, G5RenderCache,
  G47ImageResource, G47TextPipeline, G3DirectReference,
  FC01ASelectionInputIntegration, SDKPublicSelectionInput, FC03ImageFacade,
  G5Navigation. Runtime boundary checker passes (117 contract files).
- Matching Release WASM/JS SDK builds. Existing Chrome WebGL2 DPR2 cadence
  regression passes with deliberately delayed completion polls.
- The new root-SDK browser corpus creates two 512-input long lines, a rectangle
  and a 2048x1024 image displayed at 480x240, then alternates selection three
  times. Pan and both zoom directions update the display on all 216 sampled
  wheel opportunities, preserve selection/document state and create no
  canonical receipt during navigation. Exact settle follows afterward.

In the final synthetic Chrome run, selected-state RAF P95 is 17.6–19.4 ms,
with a cold maximum of 43.7 ms and warm maxima of 19.8–21.6 ms. Exact receipt
arrives approximately 165–167 ms after the input loop; post-settle idle frames
are excluded. These are single diagnostic observations, not physical
input-to-photon, controlled speedup ratios or a zero-jank guarantee.

Full repository CTest was attempted in the targeted host build: 8/239 ran,
230 were not run because their executables had not been built, and
SDKPublicObjectIdentityProcessIsolation failed because its child executable was
also absent. Required additional selection/image/text targets were then built
and the nine related checks above passed. This is not a complete full-suite
PASS; the complete failure inventory is retained in the evidence summary.

Evidence: `verification/evidence/performance/g5-selected-wheel-navigation/`.
The current official demo is served at `http://127.0.0.1:4203/example.html`.
Existing user canvas was not reloaded. Physical trackpad, Android and Windows
evidence remains `BLOCKED_EVIDENCE`. Other object-family coverage and prepared
source budget work remain separate optimization tasks.

Resolved implementation design (private owner paths only)

Keep existing valid UR2a document/Scene/History/work ownership. No SDK, semantic,
GPU algorithm, resource budget or canonical receipt changes.

1. Cold worker: extract the existing before/Undo/Redo capture transaction into
   one executable helper. Persist each available stage (bytes, snapshot and
   metadata) before checking any predicate. Keep mismatch observations; classify
   each stage with the existing independent native geometry/AA oracle. A failing
   assertion must not prevent export. This is correctness work outside timing.
2. Navigation: Runtime derives a current policy FramePlan from the same current
   synchronized FrameState, DocumentBackground and canonical composition defaults.
   Pass it through a private Render overload. Render validates the FramePlan,
   identity, clip/metrics/target and background/transparent/isolation policy;
   existing complete source/order/body/resource equality and conservative mask
   damage still decide reusable regions. Old captured frame identity is immutable.
   The current policy is not a force-compatible flag or an exact completion.
   Existing world tile jobs retain useful same-basis progress across camera
   changes; changed/missing regions must finish before complete feedback presents.
3. Brush display: do not hide retained ARC in deferred partial Down. After an
   actually accepted full-viewport current-Scene transient replacement, suppress
   only the previously committed Brush contours represented by that replacement.
   Keep their logical identities until the existing strict canonical receipt.
   Filter suppressed contours from later preview rendering and acknowledgements;
   new contours remain visible. Invalid/stale/failed feedback cannot transfer
   display ownership. Reset/prune suppression with existing session/lifecycle
   owners, never synthesize CanonicalVisible.
4. Tests: execute old-behavior cold retention, current-Scene bootstrap and actual
   composed overlay visibility RED. Spies forward real visibility. Align only
   the named old sweep test using an actual qualified initial Brush handoff.
   Preserve all original sample/frame/Up assertions and strict completion tests.
5. Build with locked Store/Skia artifacts. Preserve4283 and all old windows.
   Run only UR2R1-C1/C2 after native/runner checks and source/artifact freeze,
   retain every failure and stop at terminal result with a new normal Demo.

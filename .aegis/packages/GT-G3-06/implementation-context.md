# GT-G3-06 implementation context

G3-05 is closed and integrated into canonical `main` at
`3136e754b9bddf3d36efbd1f0cdda534b189ac56`. Render Core already owns typed
`ViewId`, `FrameId`, `SurfaceGeneration`, `MetricsGeneration`, immutable
`SurfaceMetrics`/`FrameState`, an immutable `FramePlan`, a renderer-neutral
backend submission seam, and the private deterministic Headless Skia backend.

G3-06 adds only renderer-neutral surface-generation lifecycle and presentation
correlation. A per-view live snapshot supplies the current generation-bound
surface metrics without owning a native handle. A matching frame may enter
`PresentSubmitted`; it may become `Presented` only after exact
`PlatformQualified` feedback for the same frame and the still-live surface and
metrics generations. Resize/rebind or loss makes older feedback stale and must
not mutate semantic/RuntimeScene state.

This is not the full `CanonicalVisible`/Arc handoff. Do not implement coverage
manifests, Arc tokens or preview clearing, platform-specific proof primitives,
native Windows/Web/Apple/Android surfaces, a scheduler, or a final surface ABI.
Those remain successor packages.

First action: add the frozen field-complete G3SurfacePresentation test and CMake
target, run the required missing-boundary RED, and persist it. Then implement the
smallest value-based lifecycle/tracker state machines, execute the generation
A/B and feedback negative corpus, run every predecessor matrix including the
locked published Headless SDK consumer, and materialize exact evidence under
`.aegis/results/GT-G3-06/`.

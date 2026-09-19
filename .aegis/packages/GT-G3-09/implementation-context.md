# GT-G3-09 implementation context

G3-08 is closed and integrated at `d3e15490db44732884f6146624ad3186feed78c2`.
The production Render Core already owns renderer-neutral frame state,
SurfaceLifecycle, PresentationTracker, and the private Skia backend. This WP
adds only the Windows reference-host boundary and must not move semantic scene
ownership into the platform adapter.

First incomplete action: add the portable adapter contract test and prove RED
before adding the adapter implementation. On non-Windows hosts, the Windows
runner target is intentionally absent; the required Windows build/smoke is
hosted evidence and is produced by the package-authorized G3-09 Windows
workflow. Package v0.2 supersedes v0.1 because v0.1 required hosted Windows
evidence without authorizing its deterministic CI producer.

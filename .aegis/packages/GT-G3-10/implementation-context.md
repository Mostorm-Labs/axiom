# GT-G3-10 implementation context

G3-09 is closed and integrated at `d018ddbaf2afadd9974b66f5a0c200fd2aa2b478`;
the canonical reconciliation state is on `main@1aafd5dcd99a641db2e1285a6c67efd8d7c0c409`.
The production Render Core already owns renderer-neutral FrameState,
SurfaceLifecycle, PresentationTracker and the private backend seam. This WP
adds only the Web reference-host boundary and must not move semantic scene
ownership into the host or bridge.

The existing `verification/packages/platform-harness-web` is a verification
adapter and evidence consumer; it is not the production Web host. The new host
must be thin, own DOM/WebGL2 facts and forward lifecycle/input observations to
the production Render Core boundary.

First incomplete action: add the portable Web host contract test and prove RED
before adding the host implementation. On non-Emscripten hosts the Web target
is intentionally unavailable; the required Web build/browser smoke is hosted
evidence and must be recorded as an explicit environment result when absent.

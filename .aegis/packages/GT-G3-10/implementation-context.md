# GT-G3-10 implementation context

G3-09 is closed and integrated at `d018ddbaf2afadd9974b66f5a0c200fd2aa2b478`;
the canonical reconciliation state is on `main@1aafd5dcd99a641db2e1285a6c67efd8d7c0c409`.
The production Render Core already owns renderer-neutral FrameState,
SurfaceLifecycle, PresentationTracker and the private backend seam. This WP
adds the private WebGL2 backend target binding plus the Web reference-host
boundary; the host must not move semantic scene ownership into the host or
bridge.

The WebGL2 backend reuses the existing Skia canonical draw policy through a
private helper in `runtime/render/src/skia_headless_backend.cpp`. That file is
authorized only for this internal extraction; the command validation, draw
ordering and pixel semantics remain unchanged. This avoids copying policy into
the Web host while keeping the renderer-neutral public seam free of Skia types.

The existing `verification/packages/platform-harness-web` is a verification
adapter and evidence consumer; it is not the production Web host. The new host
must be thin, own DOM/WebGL2 facts and forward lifecycle/input observations to
the production Render Core boundary. Canonical draw-command policy remains in
`runtime/render`, not in the host.

First incomplete action: add the portable Web host contract test and prove RED
before adding the host implementation. On non-Emscripten hosts the Web target
is intentionally unavailable; the required Web build/browser smoke is hosted
evidence and must be recorded as an explicit environment result when absent.

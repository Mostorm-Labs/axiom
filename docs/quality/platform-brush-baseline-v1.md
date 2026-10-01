# Platform Brush Baseline v1

This baseline verifies two independent properties before Debug UI work:

1. Android, Web, and Windows normalize the same pointer corpus into the same
   canonical trace, viewport transitions, content coordinates, and BrushSession
   outline/replay digest.
2. Each platform reports the renderer, surface, readback, CPU-copy, present,
   and viewport-transform path actually used by the Ink Playground.

## Host oracle

Build `axiom_platform_brush_baseline_runner`, compute the SHA-256 of
`verification/fixtures/platform-brush-baseline-v1/fixture.json`, and run:

```text
axiom_platform_brush_baseline_runner host <fixture-sha256> observation.json
```

This checks common normalization, multi-contact viewport ownership, content
coordinates, BrushPackage resolution, preview/seal, and a real BrushSession
replay. It deliberately emits `BLOCKED_EVIDENCE`: it did not execute a platform
input API or display surface.

## Platform evidence

Each real adapter writes the common observation format to:

```text
<observations>/android/observation.json
<observations>/web/observation.json
<observations>/windows/observation.json
```

The observation must identify the built APK/WASM/native executable and must use
`PHYSICAL`, `EMULATOR`, or `HOSTED` reality. A static profile or reference
adapter uses `STATIC_ONLY` and cannot satisfy the three-platform result.

The platform export serializes the observation from that platform host's own
runtime trace. It does not substitute the host runner's trace. Therefore an
ordinary freehand smoke run that did not inject the exact fixture is emitted as
`BLOCKED_EVIDENCE`; automation must inject the fixed corpus through
MotionEvent/JNI, PointerEvent/WASM, or WM_POINTER before collecting a passing
observation.

Compare and materialize the evidence bundle with:

```text
node verification/tools/platform_brush_baseline.mjs compare \
  --observations <observations> \
  --output <evidence-directory>
```

With no platform artifacts, `scaffold --output <directory>` creates the exact
expected directory and returns exit code 20 with `BLOCKED_EVIDENCE`.

## Current render-path baseline

- Android: MotionEvent/JNI → common runtime → Skia raster → RGBA readback →
  JNI byte array → Bitmap → Canvas.
- Web: coalesced PointerEvent → WASM/common runtime → Skia Ganesh → WebGL2
  canvas framebuffer.
- Windows: WM_POINTER history → common runtime → Skia raster → RGBA/BGRA copy
  → `SetDIBitsToDevice`/GDI.

These paths are observations, not target architecture declarations. This
baseline does not authorize Android ANativeWindow/EGL or Windows GPU migration.

## Local availability

The common host oracle and Android NDK target are locally buildable. A complete
three-platform verdict additionally requires a real Android run, a browser WASM
run built with the locked Emscripten/Web Skia toolchain, and a Windows native
run. Missing any of those artifacts is an expected `BLOCKED_EVIDENCE`, not a
test PASS.

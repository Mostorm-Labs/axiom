# RenderTarget boundary v1

## What is unified today

Input is already converging at the common runtime boundary:

```text
Android MotionEvent / Web PointerEvent / Windows pointer history
    -> PlatformPointerBatch
    -> PlatformInteractionIngress
    -> PointerSampleBatch + PointerKey(generation)
    -> CanvasInteractionCoordinator
    -> InkEngine / BrushSession / ViewportInteractionController
```

The platform differences are at acquisition and ABI boundaries:

- Android receives framework-dispatched `MotionEvent` samples, including
  history, tool, pressure, and Android pointer ids; JNI maps them to a stable
  source/pointer identity. A normal app cannot read raw `/dev/input/event*`
  HID data through JNI without privileged/device-specific access.
- Web receives `PointerEvent` and optional coalesced events; browser
  `pointerId` and CSS/device-pixel coordinates require DOM-to-canvas mapping.
- Windows receives `WM_POINTER`/mouse history and native screen/client
  coordinates; native pointer ids and capture/loss are platform-owned.

The runtime owns phase, generation, lifecycle, viewport claim, cancellation,
view-to-content conversion, and brush semantics. Platform code must not own a
second gesture arbiter or BrushSession state machine.

## What was missing on output

`SurfaceLifecycle` already guarded view/surface/metrics generations, but it did
not describe or select a concrete output target. Existing `SkiaInkBackend` and
`WebGlSurfaceBackend` each created their own concrete surface privately. That
made CPU/raster, GPU/window, offscreen, debug, and recording targets implicit
rather than switchable runtime capabilities.

## RenderTarget model

`runtime/render/include/canvas/render/render_target.hpp` introduces the
profile/lifecycle layer, while `skia_surface_provider.hpp` and
`skia_renderer.hpp` make the drawing boundary explicit:

```text
RenderTargetInfo
  profileId / kind / backend / format / metrics / capabilities
RenderTargetSwitchboard
  registerProfile -> switchTo -> resize/loss generation
RenderTargetProvider (platform-owned realization seam)
SkiaRenderer
  FramePlan/BrushRenderPoint -> SkCanvas draw calls
SkiaSurfaceProvider
  acquire -> SkSurface -> release -> present/readback
```

The runtime semantic layer does not receive `HWND`, `ANativeWindow`, DOM canvas,
EGL, or native context handles. Providers own those handles and expose only a
`SkSurface` during an acquired frame, plus `RenderTargetInfo` and lifecycle
transitions. `SkiaRenderer` is the only common drawing adapter; WebGL, GDI,
Canvas and future D3D are provider/present realizations, never peer renderers.
This is the part worth borrowing from Skia Viewer:
window/context/surface creation, resize, loss, present, and target switching;
not Viewer’s application or scene architecture.

Recommended profiles:

| Profile | Owner | Purpose |
|---|---|---|
| `cpu-raster` | locked Skia raster adapter | deterministic reference/golden/readback |
| `gpu-window` | Android/Web/Windows provider | actual display surface |
| `gpu-offscreen` | platform/backend provider | screenshot, comparison, tile/debug passes |
| `recording` | test/debug provider | command capture and replay |

The current implementation registers/switches metadata and lifecycle; the
locked Skia raster provider is wired through `SkiaRenderer`, while WebGL wraps
its framebuffer as a Skia provider. It does not claim that Android
ANativeWindow/EGL or Windows GPU migration is complete.
That remains a separate platform realization task.

## Why this matters for baseline verification

Input parity alone proves only the left side of the pipeline. The baseline must
also bind each observation to a target profile and report renderer, surface
kind, readback/copy/present counters, and viewport-transform application. A
missing real provider or fixed corpus run remains `BLOCKED_EVIDENCE`.

## Skia Viewer comparison

Skia Viewer is a useful reference for:

1. selecting backend/context/surface profiles;
2. handling resize and device/surface loss;
3. separating draw content from present/screenshot/benchmark paths;
4. switching CPU/GPU/offscreen targets for differential validation.

Axiom is not a Viewer extension: its semantic document, operation/history,
runtime scene, interaction ownership, and BrushSession remain above this
render-target boundary.

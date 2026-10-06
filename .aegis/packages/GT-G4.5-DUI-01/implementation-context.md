# GT-G4.5-DUI-01 Implementation Context

## Trusted repository baseline
- repository: `Mostorm-Labs/axiom`
- task anchor: `518b9b7a66924ef2ea96af4cfa11fdce4b0d721b`
- required relation: execution start must be this revision or a descendant.

## Current repository reality at the anchor
- `apps/axiom_canvas_demo/platform/windows/windows_canonical_surface_host.cpp` provides the current generation-bound Windows canonical reference host. It is not yet the final product GPU presentation implementation.
- `arc/platform/windows/windows_backend.cpp` provides the actual current Windows Arc preview presenter: a host-owned independent `WS_POPUP` layered window with `WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW`, DIB/GDI drawing and `UpdateLayeredWindow`. It is hit-test transparent and non-activating.
- `verification/native/platform/windows/src/windows_surface_host.cpp` already provides a native Windows verification surface using D3D12 WARP + DXGI flip-discard and generation/loss controls.
- `apps/ink_playground/platform/windows/main.cpp` is the current real Windows ink/input/Arc integration host and evidence producer.
- `AXIOM_BUILD_VERIFICATION` already gates native verification hooks/hosts.
- Dear ImGui is not present in `deps.lock.json` at the anchor and no ImGui target is present in CMake. P31 freezes `ocornut/imgui` v1.92.9b at commit `f1cc2ae15e53a861a874c3034aae6798fde194ab`; WP00 must add this exact identity to the repository dependency lock/resolver.
- Skia must be consumed from the locked `r1-full-v1` SDK; do not rebuild it.

## Target architecture flow

OS/Host input
→ InputCaptureGate
→ Debug UI or Canvas owner latch
→ Runtime/Arc normal paths

Runtime/Arc/Platform owners
→ immutable Diagnostics snapshots / Telemetry
→ DebugUiContext
→ Common ImGui panels
→ ImGuiSkiaRenderer
→ DebugControllerSurface

Debug actions
→ RuntimeFacade for product-safe controls
or
→ typed DebugControl queue
→ owner safe point
→ receipt
→ next snapshot

## First incomplete action
Run repository identity/package binding/evidence preflight, then implement `DUI-WP00` dependency/build scaffold. Do not start panel implementation before dependency and OFF-build boundaries are stable.

## Critical implementation notes
- Prefer existing `apps/ink_playground` Windows host for the first integrated Debug Controller because it already combines Windows pointer handling, Ink runtime and Arc::Windows.
- Reuse `verification/native/platform` instead of creating a second harness.
- Treat the current Arc layered-window backend as the exact physical presenter profile for initial SiblingOverlay qualification. Future direct GPU Arc presenter requires requalification.
- The initial active snapshot channel may remain `MutexCopy` until `PinnedTripleSlotSpsc` passes P20 promotion evidence.
- Trace and async GPU timing may be implemented capability-gated/non-default; if enabled, their conditional blocking evidence becomes required.

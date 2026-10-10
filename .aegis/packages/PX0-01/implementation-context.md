# PX0-01 - Contract Boundary Cleanup

PX0-01 begins from the independently reviewed PX0-00 result:

- repository: `github:Mostorm-Labs/axiom`
- reviewed task anchor: `25e5bcead21976f906333582ea4c0a80df7b2e20`
- approved Spec: `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`
- approved Plan: `docs/superpowers/plans/2026-10-10-px0-debug-control-plane-baseline-implementation.md`
- PX0-00 reconciliation: `docs/engineering/px0-debug-control-plane-baseline-reconciliation.md`

## Current flow that must be preserved

The starting implementation has two important DUI-M1 additions that predate PX0 execution and must not be dismantled:

```text
RuntimeFacade : RuntimeDiagnostics
        ^
        |
CanvasRuntimeFacadeAdapter
  - CanvasControlBinding
  - CanvasControlService
  - mountedCanvasTargets()
  - brushPresets()
  - submitCanvasControl()
        ^
        |
   +----+----+
   |         |
Windows    Web
Runtime    Runtime
Facade     Facade
```

`CanvasRuntimeFacadeAdapter` is accepted implementation reality and the common product-control lane for typed Canvas controls. PX0-01 changes the interface boundary above it; it does not replace the adapter/service.

The current Debug UI also has DUI-M1 `DebugPanelModel`. That stays in place until PX0-04. The flat DebugSnapshot and Windows/Web local snapshot assembly stay in place until PX0-02.

## Target flow for this package

```text
                   product-safe
Product/Debug UI --------------------> RuntimeFacade
                                         ^
                                         |
                              CanvasRuntimeFacadeAdapter
                                         ^
                                   +-----+-----+
                                   |           |
                                Windows       Web
                                Facade        Facade
                                   |           |
                                   +-- separately implements --> IAxiomDiagnostics

Axiom internal observation ------------> IAxiomDiagnostics (read-only)
Arc observation -----------------------> IArcDiagnostics
Platform observation ------------------> IPlatformDiagnostics
Telemetry -----------------------------> ITelemetry
```

RuntimeFacade no longer inherits diagnostics capability. The same concrete platform adapter may implement both capabilities via separate inheritance.

## Required implementation details

1. Split existing core product-control types from `runtime_facade.hpp` into `product_control.hpp`.
2. Move/group product state into `runtime_state.hpp` with identity/tool/camera/history/selection sub-structures.
3. Keep existing product enum numeric values stable.
4. Correct the current helper conflation:
   - `setBrush()` fills `brushId/brushRevision`, not `toolId`;
   - `setEraser()` fills `eraserId`, not `toolId`;
   - `toolId` is reserved for `kSetTool`.
5. Windows currently uses `request.toolId` to update its temporary selectedTool for Brush/Eraser. Migrate that presentation bookkeeping to explicit brush/eraser identity or current CanvasControl state. Do not change product behavior.
6. Keep `CanvasRuntimeFacadeAdapter` using `CanvasControlService`; allow only mechanical include/compile changes there unless a test demonstrates a directly required contract adjustment.
7. Keep `RuntimeDiagnostics`/the `IAxiomDiagnostics` alias as a read-only owner contract if that minimizes source churn, but RuntimeFacade must not derive from it.
8. Add a typed `FeatureDiagnosticsSnapshot` seam with empty/default successor payloads; do not design Shape/RichText/Connector/Snap/Image diagnostics fields here.
9. Extend Platform diagnostics with the currently resolved canonical Surface mode and Telemetry with the already-observed `sampleHz/frameMs/queueAgeMs` values needed by later PX0-02 assembly. Source these values from existing owner/HUD truth only.
10. Do not move `buildDebugSnapshot` / `debugSnapshot`, remove `DebugCommand`, or build Workbench/PanelRegistry in this package.

## Existing files that are implementation reality, not new Authority

- `apps/ink_playground/common/canvas_runtime_facade_adapter.hpp/.cpp`
- `debug_ui/include/canvas/debug_ui/panel_model.hpp`
- `debug_ui/src/panel_model.cpp`
- `apps/ink_playground/tests/dui_m1_runtime_facade_test.cpp`
- `apps/ink_playground/tests/dui_m1_view_controls_test.cpp`

Preserve their valid behavior. If an implementation conflict is genuinely semantic rather than mechanical, fail closed instead of redesigning the approved PX0 contracts.

## First incomplete action

Run repository/package/anchor preflights, record the actual starting revision, perform the required ImplementationDesignPreflight, then add the focused RED contract test before touching production code.

# PX0-00 - Debug Control Plane Baseline Reconciliation

PX0-00 is a deliberately read-only architecture/implementation reconciliation before PX0-01 is allowed to mutate RuntimeFacade, Diagnostics, DebugSnapshot, Debug UI, or platform hosts.

## Trusted baseline

- Repository: `github:Mostorm-Labs/axiom`
- Approved PX0 control revision: `a077aff9ec7c64cd587a0c2643d283e135bba8dd`
- Earlier implementation-reality baseline used while drafting the Spec: `9760a8ad910aaa1d1232c9cf24723e4d311d9816`
- Approved Spec: `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`
- Approved Plan: `docs/superpowers/plans/2026-10-10-px0-debug-control-plane-baseline-implementation.md`

## Inspect at the actual starting revision

Inspect these paths without modifying them:

- `runtime/foundation/include/canvas/runtime/runtime_facade.hpp`
- `runtime/foundation/include/canvas/runtime/diagnostics.hpp`
- `runtime/foundation/include/canvas/runtime/debug_control.hpp`
- `runtime/foundation/include/canvas/runtime/telemetry.hpp`
- `runtime/foundation/include/canvas/runtime/surface_debug_control.hpp`
- `debug_ui/include/canvas/debug_ui/controller.hpp`
- `debug_ui/include/canvas/debug_ui/panels.hpp`
- `debug_ui/include/canvas/debug_ui/snapshot.hpp`
- `debug_ui/include/canvas/debug_ui/command.hpp`
- `debug_ui/src/controller.cpp`
- `debug_ui/src/snapshot.cpp`
- `debug_ui/src/command.cpp`
- `debug_ui/src/windows_host.cpp`
- `apps/ink_playground/platform/windows/main.cpp`
- `apps/ink_playground/platform/web/bridge.cpp`
- `apps/ink_playground/platform/web/index.html`
- relevant G4.7 files/commits that changed RuntimeFacade, Debug UI integration, or host diagnostics after the earlier implementation-reality baseline.

## Required answers

1. Does `RuntimeFacade` still inherit `RuntimeDiagnostics` at the actual starting revision?
2. Does Windows still assemble `DebugSnapshot` locally (for example `buildDebugSnapshot` or equivalent)?
3. Does Web still assemble `DebugSnapshot` locally (for example `debugSnapshot` or equivalent)?
4. Have newer G4.7/runtime/debug-ui changes materially invalidated any approved PX0 Spec or implementation-plan assumption?

## Reconciliation record

Create only:

`docs/engineering/px0-debug-control-plane-baseline-reconciliation.md`

The record must include:

- `task_id: PX0-00`
- `stage: P32_IMPLEMENTATION`
- repository provider/full name
- execution ref
- actual starting revision
- task anchor and ancestry result
- exact inspected-path inventory
- the four required answers with file/symbol support
- any authority/implementation drift findings
- final `status: READY` or an exact `BLOCKED_*` status
- `next_action: PX0-01 Contract Boundary Cleanup` only when READY

Do not fix drift. A material conflict is the result, not an invitation to redesign PX0 in code.

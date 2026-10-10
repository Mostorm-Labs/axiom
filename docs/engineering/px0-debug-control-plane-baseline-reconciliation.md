# PX0-00 Debug Control Plane Baseline Reconciliation

- `task_id: PX0-00`
- `stage: P32_IMPLEMENTATION`
- `status: READY`
- `execution_ref: codex/px0-00-baseline-reconciliation`
- `repository: github:Mostorm-Labs/axiom`
- `actual_starting_revision: 8e77bf92d306c5674ecbb2bd29beaae51d078ae9`
- `task_anchor: a077aff9ec7c64cd587a0c2643d283e135bba8dd`
- `task_anchor_relation: ancestor`
- `task_anchor_check: PASS`
- `package_materialization_ref: b5d9d835f36f2399fc25f6b391c7248c3f0735db`
- `package_binding_preflight: PASS`

## Execution and ancestry

PX0-00 was executed in the independent worktree at
`C:/Users/Chado/.codex/worktrees/px0-00-baseline-reconciliation/canvas`.
The worktree started from the already-pushed DUI implementation revision
`048b5073db8fa72db42bb2458d84764fc3e894c2` and then merged the exact PX0 package
materialization `b5d9d835f36f2399fc25f6b391c7248c3f0735db` without resetting or
discarding either history. The resulting actual starting revision for the
reconciliation delta is `8e77bf92d306c5674ecbb2bd29beaae51d078ae9`.

`git merge-base --is-ancestor a077aff9ec7c64cd587a0c2643d283e135bba8dd 8e77bf92d306c5674ecbb2bd29beaae51d078ae9` exits `0`.
The repository remote resolves to `https://github.com/Mostorm-Labs/axiom.git`.

The package-local bindings were verified before mutation:

| file | SHA-256 | result |
| --- | --- | --- |
| `.aegis/packages/PX0-00/authority.lock.json` | `6fe6262f7ea2a666703926cf9f18572b2e20ea3e385eed3c2bb2e4ef4cad0d80` | PASS |
| `.aegis/packages/PX0-00/verification.lock.json` | `a5136a78df43179323341107c8faa8ef04b783c68aff5c3fb23cc85e7312ac9f` | PASS |
| `.aegis/packages/PX0-00/execution-contract.json` | `3c244ae93240d3f070b50125554ae3ab7cf72d7c35f8a0ee493079f547b47c85` | PASS |
| `.aegis/packages/PX0-00/implementation-context.md` | `865e9c2346ed6a6f8152d6cfdf58abe344a978c91062486e275de430dd7a9998` | PASS |
| `.aegis/packages/PX0-00/evidence-contract.json` | `4747d59bfd66f9fffbbac8749b323fb0904dc377063532a26579fe27488617fd` | PASS |

## Inspected implementation reality

All paths required by `implementation-context.md` are present at the actual
starting revision. The relevant observations are:

| area | path | exact observation |
| --- | --- | --- |
| Runtime contract | `runtime/foundation/include/canvas/runtime/runtime_facade.hpp` | `RuntimeFacade` is declared at line 131 and still derives from `RuntimeDiagnostics`; it exposes `readRuntimeState`, `submitProductControl`, typed tool/brush/eraser/selection/history/camera entry points, and canvas-control adapters. |
| Diagnostics base | `runtime/foundation/include/canvas/runtime/diagnostics.hpp` | `DiagnosticsProvider`, `ArcDiagnostics`, and `PlatformDiagnostics` remain separate read-only provider contracts. |
| Engineering commands | `runtime/foundation/include/canvas/runtime/debug_control.hpp` | `AxiomDebugControl` owns enqueue/receipt for engineering-only commands. |
| Telemetry | `runtime/foundation/include/canvas/runtime/telemetry.hpp` | `Telemetry::readTelemetry` remains a separate provider seam. |
| Surface control | `runtime/foundation/include/canvas/runtime/surface_debug_control.hpp` | `PlatformDebugControl` owns surface-mode requests, receipts, and safe-point processing. |
| Debug controller | `debug_ui/include/canvas/debug_ui/controller.hpp` | The controller consumes immutable snapshots and an optional `RuntimeFacade`; it does not own Runtime state. |
| Panel declarations | `debug_ui/include/canvas/debug_ui/panels.hpp` | Panel capability descriptors and availability are derived from `DebugSnapshot`. |
| Snapshot schema | `debug_ui/include/canvas/debug_ui/snapshot.hpp` | `DebugSnapshot` is the structured panel input and includes generation, surface, control receipt, history, input, and metric fields. |
| Command schema | `debug_ui/include/canvas/debug_ui/command.hpp` | Debug-only commands use a typed command/receipt lane. |
| Common controller | `debug_ui/src/controller.cpp` | `buildImGuiPanels` renders tabbed Overview, Canvas/Selection, RuntimeFacade, Brush/Eraser, Input, Surface, and Diagnostics content; RuntimeFacade buttons submit typed product calls. |
| Snapshot channel | `debug_ui/src/snapshot.cpp` | `MutexCopySnapshotChannel` publishes and reads immutable snapshot copies. |
| Command implementation | `debug_ui/src/command.cpp` | The bounded command queue owns debug-command admission and receipts. |
| Windows host | `debug_ui/src/windows_host.cpp` | `WindowsDebugUiHost::frame` refreshes the snapshot, builds common panels, and refreshes after a submission; it receives a RuntimeFacade pointer rather than owning Runtime state. |
| Windows application host | `apps/ink_playground/platform/windows/main.cpp` | `WindowsRuntimeFacade` implements diagnostics and product controls; `buildDebugSnapshot` still assembles the platform snapshot locally and publishes it before the host frame. |
| Web application host | `apps/ink_playground/platform/web/bridge.cpp` | `WebRuntimeFacade` implements diagnostics and product controls; `debugSnapshot` still assembles the platform snapshot locally before `buildImGuiPanels` renders it. |
| Web page | `apps/ink_playground/platform/web/index.html` | The Web host surface is present and remains a platform presentation entry point. |

## Required answers

1. **Does `RuntimeFacade` still inherit `RuntimeDiagnostics`?** Yes. The
   declaration is `class RuntimeFacade : public RuntimeDiagnostics` in
   `runtime_facade.hpp`. This is explicitly part of the approved PX0 starting
   reality; PX0-00 records it and does not change it.

2. **Does Windows still assemble `DebugSnapshot` locally?** Yes. The function
   `buildDebugSnapshot(State&)` in `apps/ink_playground/platform/windows/main.cpp`
   populates the snapshot and `paint`/initialization publish it through the local
   `debugSnapshots` channel. RuntimeFacade state is read into that projection,
   but ownership has not yet moved to a common assembler.

3. **Does Web still assemble `DebugSnapshot` locally?** Yes. The function
   `debugSnapshot(Host&, DebugUiState&)` in
   `apps/ink_playground/platform/web/bridge.cpp` builds the snapshot before the
   common ImGui panel builder is called.

4. **Have newer G4.7/runtime/debug-ui changes materially invalidated the
   approved PX0 assumptions?** No. The descendants `1faeacd3` (DUI M1 runtime
   controls) and `048b5073` (Windows preview diagnostics/regression handling)
   add typed RuntimeFacade controls, common panel tabs, host adapters, and
   preview diagnostics while preserving the approved ownership direction:
   RuntimeFacade remains the product-control seam, debug commands remain typed,
   snapshots remain read-only projections, and Windows/Web remain composition
   roots. They are implementation inputs for PX0-01/PX0-02, not a new authority
   or a contradictory second scene/control model.

## Authority and scope conclusion

No material authority conflict was found. The observed duplication of snapshot
assembly and the continued `RuntimeFacade : RuntimeDiagnostics` inheritance are
known PX0 migration targets, not reasons to redesign PX0-00. The accepted DUI
commit is therefore retained as implementation reality and is not silently
promoted over the PX0 design authority.

The PX0-00 mutation boundary was respected: the only new path in the execution
delta is this reconciliation record. No Runtime, Debug UI, application host,
test, build, package, or product behavior file was changed by PX0-00.

## Next action

`PX0-01 Contract Boundary Cleanup` may begin from this exact pushed result after
independent Control Review. PX0-00 does not implement PX0-01, issue a Gate PASS,
or authorize Shape/RichText/Connector/Snap/Image work.

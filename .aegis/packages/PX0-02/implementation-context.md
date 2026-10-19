# PX0-02 - Structured Snapshot + Common Assembler

PX0-02 begins from the independently reviewed PX0-01 result:

- repository: `github:Mostorm-Labs/axiom`
- reviewed task anchor: `05451bbf7141f515612bf7942445d625d0419a92`
- approved Spec: `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`
- approved Plan: `docs/superpowers/plans/2026-10-10-px0-debug-control-plane-baseline-implementation.md`

## Current flow

The starting implementation still has a flat transport and two semantic assemblers:

```text
Windows State/Host ----------------> buildDebugSnapshot(State&)
RuntimeFacade/Diagnostics ---------/          |
Arc/Platform/Telemetry -----------------------+--> flat DebugSnapshot
                                               -> Capability[]
                                               -> controller/panel_model/windows_host

Web Host/Runtime ------------------> debugSnapshot(Host&, DebugUiState&)
                                               |
                                               +--> flat DebugSnapshot
                                                    Capability[]
```

The current flat snapshot also carries the last Product/Surface receipt values used by the existing tabs.

PX0-01 is already closed. RuntimeFacade is product-only, IAxiomDiagnostics is separate/read-only, RuntimeStateSnapshot is structured, Platform diagnostics exposes resolved Surface mode, and Telemetry exposes sampleHz/frameMs/queueAgeMs. Do not reopen those contracts.

## Target flow

```text
RuntimeFacade --------------------\
IAxiomDiagnostics -----------------\
IArcDiagnostics --------------------+--> DebugSnapshotAssembler
IPlatformDiagnostics ---------------/          |
ITelemetry ------------------------/           +--> structured DebugSnapshot
DebugActivitySource --------------/                 - stamp
                                                    - coherence
                                                    - product
                                                    - axiom
                                                    - arc
                                                    - platform
                                                    - telemetry
                                                    - activity
```

Windows and Web compose provider lifetimes, but common debug_ui code owns all domain mapping.

## Frozen snapshot shape

```cpp
enum class DebugAvailability : std::uint8_t {
  kAvailable,
  kDegraded,
  kUnavailable,
  kUnsupported,
  kError,
};

enum class SnapshotCoherence : std::uint8_t {
  kCoherent,
  kMixedGeneration,
  kStale,
};

template <typename T>
struct DiagnosticSection final {
  DebugAvailability availability = DebugAvailability::kUnsupported;
  T value{};
};

struct DebugSnapshot final {
  DebugSnapshotStamp stamp{};
  SnapshotCoherence coherence = SnapshotCoherence::kStale;
  DiagnosticSection<canvas::runtime::RuntimeStateSnapshot> product{};
  DiagnosticSection<canvas::runtime::AxiomDiagnosticsSnapshot> axiom{};
  DiagnosticSection<canvas::runtime::ArcDiagnosticsSnapshot> arc{};
  DiagnosticSection<canvas::runtime::PlatformDiagnosticsSnapshot> platform{};
  DiagnosticSection<canvas::runtime::TelemetrySnapshot> telemetry{};
  DebugActivitySnapshot activity{};
};
```

`DebugSnapshotStamp` retains the existing generation/sequence/snapshot/time/frame identity fields and adds `documentRevision`.

There is no top-level `Capability[]`, canonicalRevision, previewRevision, selectedTool, selection, Surface, telemetry, receipt, or other domain field after migration.

## Activity sequencing resolution

The approved Plan sketches `DebugSnapshotSources::activity` as `DebugActivityLog*`, but `DebugActivityLog` is itself a PX0-03 deliverable. PX0-02 must not implement it early.

Resolve that compile-order dependency with a read-only source contract in `activity.hpp`:

```cpp
struct DebugActivitySnapshot final {
  std::optional<canvas::runtime::ProductControlReceipt> productControl;
  std::optional<canvas::runtime::SurfaceModeReceipt> surfaceControl;
};

class DebugActivitySource {
 public:
  virtual ~DebugActivitySource() = default;
  [[nodiscard]] virtual DebugActivitySnapshot readActivity() const noexcept = 0;
};
```

`DebugSnapshotSources` uses `const DebugActivitySource* activity`. PX0-03 may make the real bounded `DebugActivityLog` implement this interface and expand the snapshot with bounded entries.

Windows should expose its existing `lastProductReceipt/lastSurfaceReceipt` through a tiny source adapter. Web currently has no equivalent last-receipt snapshot behavior, so a null Activity source is valid.

## Coherence algorithm

For one `capture()` call:

1. increment assembler-local `snapshotSequence` once;
2. if RuntimeFacade is missing, capture other available providers once and publish `kStale`;
3. otherwise read leading RuntimeStateSnapshot;
4. read Axiom, Arc, Platform, Telemetry, and Activity sources once;
5. read trailing RuntimeStateSnapshot;
6. compare only the frozen Product identity fields: runtimeGeneration, documentGeneration, documentRevision, viewGeneration, surfaceGeneration;
7. when equal, publish the trailing Product state and `kCoherent`;
8. when unequal, repeat steps 3-6 once;
9. if the second attempt is stable, publish the second trailing Product state as `kCoherent`; otherwise publish the second attempt as `kMixedGeneration`.

No global Runtime lock, provider mutation, unbounded retry, sleep, or blocking rendezvous is permitted.

Stamp values are finalized from the published attempt:
- `snapshotSequence`: assembler-local capture number;
- `sequence`: telemetry.sequence when Telemetry is Available, else snapshotSequence;
- `monotonicTimeNs`: common steady_clock;
- `frameId`: telemetry.canonicalFrames when Telemetry is Available, otherwise platform.presentCount when Platform is present;
- runtime/document/documentRevision/view/surface identity: published Product projection;
- legacy `generation`: published surfaceGeneration.

## Legacy panel mapping during PX0-02

Do not redesign the UI. Keep `PanelCapability`, `DebugPanel`, tabs, and `buildImGuiPanels` until later packages.

The old panel availability maps temporarily as:

```text
Overview    -> Telemetry section
Input       -> Arc section
Canvas      -> Platform section, Available only
ArcPreview  -> Arc section
Surface     -> Platform section (Available or Degraded is inspectable)
Brush       -> Product section
Telemetry   -> Telemetry section
Inspection  -> Unsupported until later Inspect workspace migration
```

Current UI values map as:
- Runtime identity/history/selection/tool/brush/eraser -> Product;
- canonical revision and camera/interaction details -> Axiom;
- preview revision/active pointers/input batches/handoffs/presenter -> Arc;
- Surface generations/mode/present/lost/available -> Platform;
- sampleHz/frameMs/queueAge/input-to-preview -> Telemetry;
- last Product/Surface receipt -> Activity.

A small common helper may derive the current legacy brush/eraser/selection tool presentation from Product.tool + Product.selection; do not recreate platform-local snapshot mapping to preserve `selectedTool`.

## Platform migration

Windows:
- add common assembler ownership to State/composition;
- keep WindowsRuntimeFacade, WindowsArcDiagnostics, WindowsPlatformDiagnostics, WindowsTelemetry;
- add only a tiny Windows DebugActivitySource adapter over existing last receipts;
- remove `buildDebugSnapshot(State&)`;
- all paint/snapshot-refresh publication calls use assembler.capture();
- keep WindowsDebugUiHost APIs/setSnapshotRefresh and active-pointer throttling unchanged until PX0-06.

Web:
- keep WebRuntimeFacade, WebPlatformDiagnostics, WebTelemetry;
- add a narrow WebArcDiagnostics over the observations previously read directly by `debugSnapshot()`: previewPresentCount, hud.batch, pendingHandoffCount, previewActive; activePointerCount remains 0 unless an already-existing authoritative Host query exists;
- DebugUiState owns those providers plus common assembler;
- remove `debugSnapshot(Host&, DebugUiState&)`;
- render passes call assembler.capture();
- keep Web ImGui/Skia/input lifecycle unchanged.

## First incomplete action

Run repository/package/anchor and binding preflights, perform the required ImplementationDesignPreflight, add the focused PX0DebugSnapshot RED oracle, then continue through the full frozen closure contract unless a terminal blocker is found.

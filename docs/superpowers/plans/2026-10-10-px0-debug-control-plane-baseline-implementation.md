# PX0 Debug Control Plane Baseline Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Converge the existing Axiom Debug UI into a structured, cross-platform engineering control plane with separated Product/Diagnostics/Debug ownership, common snapshot assembly, explicit View/Camera controls, stable feature-extension slots, and preserved Windows/Web qualification behavior.

**Architecture:** RuntimeFacade remains the only product-safe mutation/state boundary; Diagnostics remain read-only; AxiomDebugControl and PlatformDebugControl own engineering-only mutation. A common DebugSnapshotAssembler and DebugControlRouter feed a six-workspace ImGui workbench, while Windows/Web hosts become thin presentation/input adapters.

**Tech Stack:** C++20, CMake/Ninja, Dear ImGui, Skia, Win32, WebAssembly/Emscripten, Python/pytest structural contract tests.

**Spec:** `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`

## Global Constraints

- Execution starts from a repository revision that is a descendant of `9760a8ad910aaa1d1232c9cf24723e4d311d9816` and must first pass PX0-00 reconciliation.
- Preserve `SemanticDocument` as sole canonical truth and Operation as the only canonical mutation unit.
- Do not change stable Runtime C ABI as incidental PX0 work.
- Product behavior must route through RuntimeFacade/product-safe typed capabilities; engineering-only behavior must route through explicit DebugControl owners.
- Diagnostics are read-only; Debug UI panels never receive SemanticDocument, RuntimeScene, SpatialIndex, SkSurface, InkPlaygroundHost, or owner-interface pointers.
- Do not add new `debug_ui::DebugCommandKind` values. Retire the generic Debug UI command queue after its remaining callers are migrated; owner safe-point queues remain valid.
- Do not implement Shape authoring, TextEditSession/IME authoring, Connector semantics, Snap semantics, Image product import, final Product UI, docking, raw Document editing, full Scene tree browsing, GPU profiling, or a Brush parameter laboratory.
- Top-level workspaces are exactly Dashboard, Control, Inspect, Runtime, Performance, and Scenarios.
- Persistent UI controls reflect resolved owner/runtime state after request/receipt; local ImGui widget state is never canonical truth.
- View/Camera baseline is Pan, Zoom At, Fit Content, Fit Selection, and Fit Primary Object. PX0 does not define Reset View or 100% zoom semantics.
- Windows and Web physical Debug UI paths are closure requirements; Android/iOS Debug UI parity is not a PX0 blocker.
- Existing input-capture sequence latching must remain intact so Debug-owned input never leaks into the Canvas and hidden Debug UI never consumes Canvas input.

## Review Focus

1. **Mixed-generation capture during active mutation:** assembler must produce a visible `MixedGeneration`/`Stale` snapshot rather than blocking Runtime or presenting inconsistent data as coherent. Covered by PX0-02 snapshot tests.
2. **Queued engineering command changes state after admission:** Activity must advance from Pending/Queued to the owner's terminal receipt without trusting local widget state. Covered by PX0-03 router/activity tests.
3. **Debug overlay input visibility/capture transitions:** hide/show, pointer terminal/cancel, and keyboard ownership must not leak into Canvas input. Covered by PX0-06 structural tests and PX0-07 physical validation.
4. **Selection-dependent camera controls:** Fit Selection and Fit Primary Object must be disabled/rejected by context and must never silently fall back to Fit Content. Covered by PX0-03 router tests and PX0-05 workbench tests.
5. **Missing/unsupported provider:** one absent diagnostics provider or unsupported future feature must not prevent other workspaces from rendering or enable unsupported controls. Covered by PX0-02 assembler tests and PX0-04 registry/workbench tests.

---

## File Structure Locked by This Plan

### Runtime contracts

- Create `runtime/foundation/include/canvas/runtime/product_control.hpp` — core product command/receipt types and selection-pointer request types currently embedded in `runtime_facade.hpp`.
- Create `runtime/foundation/include/canvas/runtime/runtime_state.hpp` — structured product-safe Runtime state projection.
- Create `runtime/foundation/include/canvas/runtime/feature_diagnostics.hpp` — typed built-in feature diagnostics aggregate with PX0 availability only; future feature WPs populate payloads.
- Modify `runtime/foundation/include/canvas/runtime/runtime_facade.hpp` — product-safe RuntimeFacade only; no Diagnostics inheritance.
- Modify `runtime/foundation/include/canvas/runtime/diagnostics.hpp` — read-only Axiom/Arc/Platform diagnostics and structured Axiom diagnostics.
- Modify `runtime/foundation/include/canvas/runtime/surface_debug_control.hpp` — add read-only receipt lookup needed for queued Surface activity observation.

### Debug UI core

- Modify `debug_ui/include/canvas/debug_ui/snapshot.hpp` — structured immutable snapshot envelope, availability, coherence.
- Create `debug_ui/include/canvas/debug_ui/snapshot_assembler.hpp` / `debug_ui/src/snapshot_assembler.cpp` — common cross-platform capture/coherence.
- Create `debug_ui/include/canvas/debug_ui/activity.hpp`.
- Create `debug_ui/include/canvas/debug_ui/activity_log.hpp` / `debug_ui/src/activity_log.cpp`.
- Create `debug_ui/include/canvas/debug_ui/control_router.hpp` / `debug_ui/src/control_router.cpp`.
- Create `debug_ui/include/canvas/debug_ui/workspace.hpp`.
- Create `debug_ui/include/canvas/debug_ui/panel.hpp`.
- Create `debug_ui/include/canvas/debug_ui/panel_registry.hpp` / `debug_ui/src/panel_registry.cpp`.
- Create `debug_ui/include/canvas/debug_ui/ui_session_state.hpp`.
- Create `debug_ui/include/canvas/debug_ui/workbench.hpp` / `debug_ui/src/workbench.cpp`.
- Create `debug_ui/include/canvas/debug_ui/builtin_panels.hpp`.
- Create `debug_ui/include/canvas/debug_ui/imgui_skia_renderer.hpp` / `debug_ui/src/imgui_skia_renderer.cpp`.
- Create panel implementation files under `debug_ui/src/panels/` and `debug_ui/src/panels/features/ink.cpp`.
- Modify `debug_ui/include/canvas/debug_ui/controller.hpp` / `debug_ui/src/controller.cpp` — orchestration only.
- Delete `debug_ui/include/canvas/debug_ui/panels.hpp` after migration.
- Delete `debug_ui/include/canvas/debug_ui/command.hpp` and `debug_ui/src/command.cpp` after generic-queue callers/tests are removed.
- Preserve `debug_ui/include/canvas/debug_ui/debug_command_queue.hpp` and `surface_debug_queue.hpp` because these are owner-side safe-point reference queues, not the retired generic UI queue.

### Platform/application convergence

- Modify `debug_ui/include/canvas/debug_ui/windows_host.hpp` / `debug_ui/src/windows_host.cpp` — host presentation/input only, using one DebugController.
- Modify `apps/ink_playground/platform/windows/main.cpp` — common controller composition; remove platform snapshot assembly; route temporary toolbar product mutations through RuntimeFacade.
- Modify `apps/ink_playground/platform/web/bridge.cpp` — common controller composition; remove Web-local snapshot assembly and direct panel construction.
- Modify `apps/ink_playground/platform/web/index.html` only where required to preserve Debug UI sizing/input and route temporary controls through the existing product bridge.
- Modify `debug_ui/CMakeLists.txt` and `apps/ink_playground/CMakeLists.txt` for focused tests/new sources.

### Tests

- Create `debug_ui/tests/runtime_contract_test.cpp`.
- Create `debug_ui/tests/snapshot_assembler_test.cpp`.
- Create `debug_ui/tests/control_router_test.cpp`.
- Create `debug_ui/tests/panel_registry_test.cpp`.
- Create `debug_ui/tests/workbench_state_test.cpp`.
- Refactor `debug_ui/tests/debug_ui_test.cpp` to retain only contracts not moved to focused tests.
- Update `apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py`.
- Update `apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py`.
- Update `apps/ink_playground/tests/windows_debug_ui_selection_contract_test.py`.

---

# Work Package PX0-00 — Baseline Reconciliation

### Task 1: Reconcile the execution baseline before code changes

**Files:**
- Create: `docs/engineering/px0-debug-control-plane-baseline-reconciliation.md`
- Read only: all files listed in the File Structure section above plus current branch history.

**Interfaces:**
- Consumes: approved Spec at `docs/superpowers/specs/2026-10-10-px0-debug-control-plane-baseline-design.md`.
- Produces: a durable reconciliation record containing exact execution revision, ancestor check against `9760a8ad910aaa1d1232c9cf24723e4d311d9816`, current interface/file inventory, and any stop condition.

- [ ] **Step 1: Verify repository identity and ancestry**

Run:

```bash
git remote -v
git rev-parse HEAD
git merge-base --is-ancestor 9760a8ad910aaa1d1232c9cf24723e4d311d9816 HEAD
```

Expected: repository is `Mostorm-Labs/axiom` and the ancestor command exits 0.

- [ ] **Step 2: Inventory current implementation reality**

Record exact file/commit state for:

```text
runtime/foundation/include/canvas/runtime/runtime_facade.hpp
runtime/foundation/include/canvas/runtime/diagnostics.hpp
runtime/foundation/include/canvas/runtime/debug_control.hpp
runtime/foundation/include/canvas/runtime/surface_debug_control.hpp
debug_ui/include/canvas/debug_ui/{controller,panels,snapshot,command}.hpp
debug_ui/src/{controller,snapshot,command,windows_host}.cpp
apps/ink_playground/platform/windows/main.cpp
apps/ink_playground/platform/web/bridge.cpp
apps/ink_playground/platform/web/index.html
```

The record must explicitly answer whether RuntimeFacade still inherits RuntimeDiagnostics, whether Windows/Web still assemble DebugSnapshot locally, and whether any newer G4.7 changes alter the approved Spec assumptions.

- [ ] **Step 3: Fail closed on material conflict**

If current implementation/authority materially contradicts the approved Spec, stop PX0 implementation and return the exact conflicting symbol/file/revision. Do not reinterpret the Spec inside Codex.

- [ ] **Step 4: Write the reconciliation record**

The file must contain `status: READY` only when no material conflict exists; otherwise `status: BLOCKED_*` with the blocker.

- [ ] **Step 5: Commit the reconciliation record**

```bash
git add docs/engineering/px0-debug-control-plane-baseline-reconciliation.md
git commit -m "docs: reconcile PX0 debug control plane baseline"
```

**Codex WP boundary:** PX0-00 is read-only except for the reconciliation record. It may not modify product/runtime/debug code.

---

# Work Package PX0-01 — Contract Boundary Cleanup

### Task 2: Split product command and product state contracts out of RuntimeFacade

**Files:**
- Create: `runtime/foundation/include/canvas/runtime/product_control.hpp`
- Create: `runtime/foundation/include/canvas/runtime/runtime_state.hpp`
- Modify: `runtime/foundation/include/canvas/runtime/runtime_facade.hpp`
- Create: `debug_ui/tests/runtime_contract_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct RuntimeIdentityState final {
  std::uint64_t runtimeGeneration;
  std::uint64_t documentGeneration;
  std::uint64_t documentRevision;
  std::uint64_t viewGeneration;
  std::uint64_t surfaceGeneration;
};

struct ToolState final {
  std::uint32_t toolId;
  std::uint32_t brushId;
  std::uint32_t brushRevision;
  std::uint32_t eraserId;
};

struct CameraState final {
  float scale;
  float translationX;
  float translationY;
};

struct HistoryState final {
  bool canUndo;
  bool canRedo;
};

struct SelectionState final {
  bool enabled;
  std::uint32_t selectedObjectCount;
  std::uint64_t primaryObject;
  std::uint64_t snapCandidateCount;
};

struct RuntimeStateSnapshot final {
  RuntimeIdentityState identity;
  ToolState tool;
  CameraState camera;
  HistoryState history;
  SelectionState selection;
};
```

- `product_control.hpp` retains the current core enums/requests/receipts and typed RuntimeFacade helper methods retain their current semantics.

- [ ] **Step 1: Write the failing RuntimeFacade boundary test**

Test assertions:

```cpp
static_assert(!std::is_base_of_v<canvas::runtime::RuntimeDiagnostics,
                                 canvas::runtime::RuntimeFacade>);
static_assert(std::is_default_constructible_v<canvas::runtime::RuntimeStateSnapshot>);
```

Also instantiate a RuntimeFacade probe that implements `readRuntimeState()` and `submitProductControl()` without implementing diagnostics.

- [ ] **Step 2: Build the focused test and verify failure**

Configure once if needed:

```bash
cmake -S . -B out/px0-host-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCANVAS_BUILD_POC01=OFF \
  -DCANVAS_BUILD_INK_PLAYGROUND=OFF \
  -DAXIOM_BUILD_DEBUG_UI=ON \
  -DAXIOM_DEBUG_UI_PROFILE=full \
  -DBUILD_TESTING=ON
cmake --build out/px0-host-debug --target axiom_debug_ui_runtime_contract_tests
```

Expected: FAIL because RuntimeFacade still inherits RuntimeDiagnostics and split headers do not yet exist.

- [ ] **Step 3: Implement the split contracts**

`runtime_facade.hpp` must include `product_control.hpp` and `runtime_state.hpp` and define:

```cpp
class RuntimeFacade {
 public:
  virtual ~RuntimeFacade() = default;
  virtual ProductControlReceipt submitSelectionPointer(
      const SelectionPointerRequest&) noexcept;
  virtual RuntimeStateSnapshot readRuntimeState() const noexcept = 0;
  virtual ProductControlReceipt submitProductControl(
      const ProductControlRequest&) noexcept = 0;
  // existing setTool/setBrush/setEraser/setSelectionMode/undo/redo/
  // setCamera/panBy/zoomAt/fitToContent/fitToSelection/fitToObject helpers
};
```

Do not change command semantics or numeric enum values.

- [ ] **Step 4: Build and run the focused test**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_runtime_contract_tests
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0RuntimeFacadeContract
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add runtime/foundation/include/canvas/runtime/product_control.hpp \
        runtime/foundation/include/canvas/runtime/runtime_state.hpp \
        runtime/foundation/include/canvas/runtime/runtime_facade.hpp \
        debug_ui/tests/runtime_contract_test.cpp debug_ui/CMakeLists.txt
git commit -m "refactor(runtime): separate product facade contracts"
```

### Task 3: Separate Axiom diagnostics from RuntimeFacade and establish feature diagnostics seam

**Files:**
- Create: `runtime/foundation/include/canvas/runtime/feature_diagnostics.hpp`
- Modify: `runtime/foundation/include/canvas/runtime/diagnostics.hpp`
- Modify: `apps/ink_playground/platform/windows/main.cpp`
- Modify: `apps/ink_playground/platform/web/bridge.cpp`
- Modify: `debug_ui/tests/runtime_contract_test.cpp`

**Interfaces:**
- Produces read-only `IAxiomDiagnostics` independently of RuntimeFacade.
- `IAxiomDiagnostics` provides:

```cpp
virtual AxiomDiagnosticsSnapshot readDiagnostics() const noexcept = 0;
virtual FeatureDiagnosticsSnapshot readFeatureDiagnostics() const noexcept;
```

- `FeatureDiagnosticsSnapshot` has typed built-in sections for Ink, Shape, RichText, Connector, Snap, and Image. PX0 may use empty/default payloads for unsupported successor features; it does not invent feature semantics.

- [ ] **Step 1: Extend the failing contract test**

Assert that a concrete object can implement `RuntimeFacade` and `IAxiomDiagnostics` as two independent bases, and that `readFeatureDiagnostics()` defaults successor features to unsupported/default state.

- [ ] **Step 2: Verify test failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_runtime_contract_tests
```

Expected: FAIL because diagnostics are not yet separated/typed.

- [ ] **Step 3: Implement read-only diagnostics contracts and migrate Windows/Web facades**

Make Windows/Web concrete runtime adapters derive from both interfaces where they currently serve both roles:

```cpp
class WindowsRuntimeFacade final
    : public canvas::runtime::RuntimeFacade,
      public canvas::runtime::IAxiomDiagnostics { ... };
```

and the equivalent Web class.

Update all product-state construction to the nested `RuntimeStateSnapshot` fields. Do not change host behavior.

- [ ] **Step 4: Run contract and existing Debug UI tests**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_tests axiom_debug_ui_runtime_contract_tests
ctest --test-dir out/px0-host-debug --output-on-failure -R "DUI20DebugUiContracts|PX0RuntimeFacadeContract"
```

Expected: PASS.

- [ ] **Step 5: Run structural platform tests**

```bash
python -m pytest \
  apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py \
  apps/ink_playground/tests/windows_debug_ui_selection_contract_test.py \
  apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py -q
```

Expected: PASS after updating only assertions invalidated by the intentional interface split.

- [ ] **Step 6: Commit**

```bash
git add runtime/foundation/include/canvas/runtime/feature_diagnostics.hpp \
        runtime/foundation/include/canvas/runtime/diagnostics.hpp \
        apps/ink_playground/platform/windows/main.cpp \
        apps/ink_playground/platform/web/bridge.cpp \
        debug_ui/tests/runtime_contract_test.cpp \
        apps/ink_playground/tests/*debug_ui*_test.py
git commit -m "refactor(runtime): separate debug diagnostics ownership"
```

**Codex WP boundary:** PX0-01 may reorganize existing product/diagnostics contracts only. It may not add Shape/Text/Connector commands or alter stable Runtime C ABI.

---

# Work Package PX0-02 — Structured Snapshot + Common Assembler

### Task 4: Replace the flat DebugSnapshot capability bag with structured sections

**Files:**
- Modify: `debug_ui/include/canvas/debug_ui/snapshot.hpp`
- Modify: `debug_ui/src/snapshot.cpp`
- Create: `debug_ui/include/canvas/debug_ui/activity.hpp`
- Create: `debug_ui/tests/snapshot_assembler_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Produces:

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
  DiagnosticSection<canvas::runtime::RuntimeStateSnapshot> product;
  DiagnosticSection<canvas::runtime::AxiomDiagnosticsSnapshot> axiom;
  DiagnosticSection<canvas::runtime::ArcDiagnosticsSnapshot> arc;
  DiagnosticSection<canvas::runtime::PlatformDiagnosticsSnapshot> platform;
  DiagnosticSection<canvas::runtime::TelemetrySnapshot> telemetry;
  DebugActivitySnapshot activity;
};
```

- [ ] **Step 1: Write failing snapshot-shape tests**

Assert:

- no global `Capability[]` member is needed;
- each section carries independent availability;
- default snapshot is bounded/default-safe;
- `MutexCopySnapshotChannel` still round-trips the full structured snapshot.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_snapshot_tests
```

Expected: FAIL against the flat snapshot.

- [ ] **Step 3: Implement the structured envelope**

Remove `Capability` / `CapabilityState` from the live snapshot. Preserve only fields that belong in `DebugSnapshotStamp`.

- [ ] **Step 4: Run tests**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_snapshot_tests
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0DebugSnapshot
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/include/canvas/debug_ui/snapshot.hpp \
        debug_ui/include/canvas/debug_ui/activity.hpp \
        debug_ui/src/snapshot.cpp debug_ui/tests/snapshot_assembler_test.cpp \
        debug_ui/CMakeLists.txt
git commit -m "refactor(debug-ui): structure debug snapshot domains"
```

### Task 5: Add the common DebugSnapshotAssembler and coherence classification

**Files:**
- Create: `debug_ui/include/canvas/debug_ui/snapshot_assembler.hpp`
- Create: `debug_ui/src/snapshot_assembler.cpp`
- Modify: `debug_ui/tests/snapshot_assembler_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
struct DebugSnapshotSources final {
  const canvas::runtime::RuntimeFacade* runtime = nullptr;
  const canvas::runtime::IAxiomDiagnostics* axiom = nullptr;
  const canvas::runtime::IArcDiagnostics* arc = nullptr;
  const canvas::runtime::IPlatformDiagnostics* platform = nullptr;
  const canvas::runtime::ITelemetry* telemetry = nullptr;
  const DebugActivityLog* activity = nullptr;
};

class DebugSnapshotAssembler final {
 public:
  explicit DebugSnapshotAssembler(DebugSnapshotSources sources);
  [[nodiscard]] DebugSnapshot capture() const;
};
```

Capture uses common `std::chrono::steady_clock` time and an assembler-local snapshot sequence. Frame ID comes from platform/canonical telemetry where available rather than a platform-specific schema builder.

- [ ] **Step 1: Write coherence tests**

Create fakes where the first and trailing RuntimeState identity reads are:

1. identical -> `kCoherent`;
2. different once, stable on one retry -> `kCoherent`;
3. different on both attempts -> `kMixedGeneration`.

Also assert a null Arc provider yields `arc.availability == kUnsupported` while Product/Axiom/Platform sections still populate.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_snapshot_tests
```

Expected: FAIL because assembler does not exist.

- [ ] **Step 3: Implement capture with at most one retry**

Do not take a global Runtime lock. Do not call platform-host snapshot builders.

- [ ] **Step 4: Run tests**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_snapshot_tests
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0DebugSnapshot
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/include/canvas/debug_ui/snapshot_assembler.hpp \
        debug_ui/src/snapshot_assembler.cpp \
        debug_ui/tests/snapshot_assembler_test.cpp debug_ui/CMakeLists.txt
git commit -m "feat(debug-ui): add common snapshot assembler"
```

**Codex WP boundary:** PX0-02 does not redesign ImGui layout. It only creates the structured observation path.

---

# Work Package PX0-03 — Control Router + Activity

### Task 6: Add bounded Activity Log and normalized control states

**Files:**
- Create: `debug_ui/include/canvas/debug_ui/activity_log.hpp`
- Create: `debug_ui/src/activity_log.cpp`
- Create: `debug_ui/tests/control_router_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Produces:

```cpp
enum class DebugControlOwner : std::uint8_t {
  kProduct,
  kAxiomDebug,
  kPlatformDebug,
};

enum class DebugActivityState : std::uint8_t {
  kPending,
  kApplied,
  kRejected,
  kUnsupported,
  kUnavailable,
  kStaleGeneration,
  kExpired,
  kQueueFull,
  kFailed,
};

struct DebugActivityEntry final {
  std::uint64_t sequence = 0;
  std::uint64_t requestId = 0;
  DebugControlOwner owner = DebugControlOwner::kProduct;
  std::string action;
  DebugActivityState state = DebugActivityState::kPending;
  std::uint64_t runtimeGeneration = 0;
  std::uint64_t documentGeneration = 0;
  std::uint64_t surfaceGeneration = 0;
  std::uint64_t frameId = 0;
};

class DebugActivityLog final {
 public:
  explicit DebugActivityLog(std::size_t capacity = 64);
  void record(DebugActivityEntry entry);
  [[nodiscard]] DebugActivitySnapshot snapshot() const;
  [[nodiscard]] std::size_t size() const noexcept;
};
```

- [ ] **Step 1: Write failing bounded-log tests**

Assert capacity 2 retains the two newest entries and preserves request IDs/states.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_control_router_tests
```

- [ ] **Step 3: Implement bounded log**

Use a bounded deque/vector; no unbounded trace/history.

- [ ] **Step 4: Run tests**

```bash
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0ControlRouter
```

Expected: PASS for activity tests.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/include/canvas/debug_ui/activity_log.hpp \
        debug_ui/src/activity_log.cpp debug_ui/tests/control_router_test.cpp \
        debug_ui/CMakeLists.txt
git commit -m "feat(debug-ui): add bounded control activity log"
```

### Task 7: Add DebugControlRouter for product, Axiom-debug, and Surface owners

**Files:**
- Create: `debug_ui/include/canvas/debug_ui/control_router.hpp`
- Create: `debug_ui/src/control_router.cpp`
- Modify: `runtime/foundation/include/canvas/runtime/surface_debug_control.hpp`
- Modify: `debug_ui/tests/control_router_test.cpp`

**Interfaces:**
- Produces:

```cpp
class DebugControlRouter final {
 public:
  DebugControlRouter(canvas::runtime::RuntimeFacade* runtime,
                     canvas::runtime::AxiomDebugControl* axiomDebug,
                     canvas::runtime::PlatformDebugControl* platformDebug,
                     DebugActivityLog* activity);

  void beginFrame(const DebugSnapshot& snapshot) noexcept;
  void refreshReceipts() noexcept;

  canvas::runtime::ProductControlReceipt setTool(std::uint32_t toolId) noexcept;
  canvas::runtime::ProductControlReceipt setBrush(
      std::uint32_t brushId, std::uint32_t revision) noexcept;
  canvas::runtime::ProductControlReceipt setEraser(std::uint32_t eraserId) noexcept;
  canvas::runtime::ProductControlReceipt setSelectionMode(bool enabled) noexcept;
  canvas::runtime::ProductControlReceipt undo() noexcept;
  canvas::runtime::ProductControlReceipt redo() noexcept;

  canvas::runtime::ProductControlReceipt panBy(float dx, float dy) noexcept;
  canvas::runtime::ProductControlReceipt zoomAt(
      float anchorX, float anchorY, float scaleDelta) noexcept;
  canvas::runtime::ProductControlReceipt fitToContent() noexcept;
  canvas::runtime::ProductControlReceipt fitToSelection() noexcept;
  canvas::runtime::ProductControlReceipt fitPrimaryObject() noexcept;

  canvas::runtime::AxiomDebugCommandReceipt submitAxiom(
      canvas::runtime::AxiomDebugCommandKind kind,
      std::uint64_t value = 0) noexcept;

  canvas::runtime::SurfaceModeReceipt setCanonicalSurfaceMode(
      canvas::runtime::SurfaceMode mode) noexcept;
};
```

`PlatformDebugControl` gains:

```cpp
virtual SurfaceModeReceipt receipt(std::uint64_t requestId) const noexcept;
```

with a default Unsupported result for owners that do not retain receipts.

- [ ] **Step 1: Write failing routing/context tests**

Tests must prove:

- `setBrush` calls RuntimeFacade only;
- `ForceFullRedraw` calls AxiomDebugControl only;
- Surface mode calls PlatformDebugControl only;
- Fit Selection without a selected object returns Rejected locally and does not call Fit Content;
- Fit Primary Object without primary object returns Rejected and does not call RuntimeFacade;
- queued Axiom/Surface receipt later changes Activity from Pending to Applied after `refreshReceipts()`;
- stale generation/queue full/expired states map distinctly into Activity.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_control_router_tests
```

Expected: FAIL because router/receipt lookup do not exist.

- [ ] **Step 3: Implement router and receipt refresh**

Retain the current command deadline convention of current snapshot sequence + 120 for Debug UI-originated owner queues unless PX0-00 finds a newer frozen value.

- [ ] **Step 4: Run tests**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_control_router_tests
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0ControlRouter
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/include/canvas/debug_ui/control_router.hpp \
        debug_ui/src/control_router.cpp \
        runtime/foundation/include/canvas/runtime/surface_debug_control.hpp \
        debug_ui/tests/control_router_test.cpp
git commit -m "feat(debug-ui): route controls through owning contracts"
```

### Task 8: Retire the generic Debug UI command queue

**Files:**
- Delete: `debug_ui/include/canvas/debug_ui/command.hpp`
- Delete: `debug_ui/src/command.cpp`
- Modify: `debug_ui/include/canvas/debug_ui/controller.hpp`
- Modify: `debug_ui/tests/debug_ui_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Consumes: DebugControlRouter from Task 7.
- Preserves: `debug_command_queue.hpp` and `surface_debug_queue.hpp` owner-side queues.

- [ ] **Step 1: Add a structural test/compile assertion that no Debug UI core header exposes `DebugCommandKind`**

- [ ] **Step 2: Verify it fails while `command.hpp` is still included**

- [ ] **Step 3: Remove generic queue usage and delete the two files**

- [ ] **Step 4: Build and run all Debug UI tests**

```bash
cmake --build out/px0-host-debug
ctest --test-dir out/px0-host-debug --output-on-failure -R "DUI|PX0"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add -A debug_ui
git commit -m "refactor(debug-ui): retire generic ui command queue"
```

**Codex WP boundary:** PX0-03 may add routing/observation infrastructure only; no new product feature semantics.

---

# Work Package PX0-04 — Workbench Shell + Panel Registry

### Task 9: Add stable workspace, slot, registry, and UI-session contracts

**Files:**
- Create: `debug_ui/include/canvas/debug_ui/workspace.hpp`
- Create: `debug_ui/include/canvas/debug_ui/panel.hpp`
- Create: `debug_ui/include/canvas/debug_ui/panel_registry.hpp`
- Create: `debug_ui/src/panel_registry.cpp`
- Create: `debug_ui/include/canvas/debug_ui/ui_session_state.hpp`
- Create: `debug_ui/tests/panel_registry_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Produces exactly six `WorkspaceId` values.
- Produces explicit `PanelSlot` values for core sections and stable feature slots.
- Produces:

```cpp
struct DebugPanelContext final {
  const DebugSnapshot& snapshot;
  DebugControlRouter& controls;
  DebugUiSessionState& ui;
};

struct PanelContribution final {
  WorkspaceId workspace;
  PanelSlot slot;
  int order;
  bool (*visible)(const DebugSnapshot&) noexcept;
  void (*render)(DebugPanelContext&);
};

class PanelRegistry final {
 public:
  bool add(PanelContribution contribution);
  [[nodiscard]] std::span<const PanelContribution> contributions(
      WorkspaceId workspace) const noexcept;
};
```

- [ ] **Step 1: Write failing registry tests**

Assert:

- exactly six top-level workspace IDs;
- a synthetic contribution can register into Control/Feature and Inspect/Object without modifying Workbench;
- invalid workspace/slot pairing is rejected;
- contribution ordering is deterministic.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_panel_registry_tests
```

- [ ] **Step 3: Implement registry/session types**

No static global registration, reflection, or plugin discovery.

- [ ] **Step 4: Run tests**

```bash
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0PanelRegistry
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/include/canvas/debug_ui/{workspace,panel,panel_registry,ui_session_state}.hpp \
        debug_ui/src/panel_registry.cpp debug_ui/tests/panel_registry_test.cpp \
        debug_ui/CMakeLists.txt
git commit -m "feat(debug-ui): add workbench extension registry"
```

### Task 10: Split renderer from controller and add the six-workspace shell

**Files:**
- Create: `debug_ui/include/canvas/debug_ui/imgui_skia_renderer.hpp`
- Create: `debug_ui/src/imgui_skia_renderer.cpp`
- Create: `debug_ui/include/canvas/debug_ui/workbench.hpp`
- Create: `debug_ui/src/workbench.cpp`
- Create: `debug_ui/include/canvas/debug_ui/builtin_panels.hpp`
- Create: `debug_ui/tests/workbench_state_test.cpp`
- Modify: `debug_ui/include/canvas/debug_ui/controller.hpp`
- Modify: `debug_ui/src/controller.cpp`
- Delete: `debug_ui/include/canvas/debug_ui/panels.hpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- `DebugController` becomes:

```cpp
struct DebugControllerContext final {
  canvas::runtime::RuntimeFacade* runtime = nullptr;
  const canvas::runtime::IAxiomDiagnostics* axiom = nullptr;
  const canvas::runtime::IArcDiagnostics* arc = nullptr;
  const canvas::runtime::IPlatformDiagnostics* platformDiagnostics = nullptr;
  const canvas::runtime::ITelemetry* telemetry = nullptr;
  canvas::runtime::AxiomDebugControl* axiomDebug = nullptr;
  canvas::runtime::PlatformDebugControl* platformDebug = nullptr;
};

class DebugController final {
 public:
  explicit DebugController(DebugControllerContext context);
  [[nodiscard]] bool buildImGuiFrame();
  [[nodiscard]] DebugSnapshot snapshot() const;
  [[nodiscard]] DebugUiSessionState& uiState() noexcept;
};
```

- `DebugWorkbench` owns header/sidebar/content/footer layout and renders the selected workspace from PanelRegistry.
- `ImGuiSkiaRenderer` moves unchanged rendering semantics out of controller.

- [ ] **Step 1: Write failing workbench-state tests**

Assert workspace selection is UI-local and changing it does not touch fake RuntimeFacade/DebugControl call counts.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_workbench_state_tests
```

- [ ] **Step 3: Move ImGuiSkiaRenderer without changing its Skia behavior**

Preserve A8 font atlas and `SkBlendMode::kModulate` behavior.

- [ ] **Step 4: Implement Workbench + controller orchestration**

Controller order is:

```text
capture snapshot
-> router.beginFrame(snapshot)
-> router.refreshReceipts()
-> capture/attach latest activity as required
-> render workbench
```

No panel may receive raw owner pointers.

- [ ] **Step 5: Run C++ and structural tests**

```bash
cmake --build out/px0-host-debug
ctest --test-dir out/px0-host-debug --output-on-failure -R "DUI|PX0"
python -m pytest apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py -q
```

Expected: PASS after structural assertions are updated to the new renderer/controller files.

- [ ] **Step 6: Commit**

```bash
git add -A debug_ui apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py
git commit -m "refactor(debug-ui): establish engineering workbench shell"
```

**Codex WP boundary:** PX0-04 proves extension architecture only. Do not migrate all current feature controls until PX0-05.

---

# Work Package PX0-05 — Existing Capability Migration

### Task 11: Migrate Dashboard, Control, Inspect, Runtime, and Performance panels

**Files:**
- Create: `debug_ui/src/panels/dashboard.cpp`
- Create: `debug_ui/src/panels/control.cpp`
- Create: `debug_ui/src/panels/inspect.cpp`
- Create: `debug_ui/src/panels/runtime.cpp`
- Create: `debug_ui/src/panels/performance.cpp`
- Create: `debug_ui/src/panels/features/ink.cpp`
- Modify: `debug_ui/include/canvas/debug_ui/builtin_panels.hpp`
- Modify: `debug_ui/CMakeLists.txt`
- Modify: `debug_ui/tests/workbench_state_test.cpp`
- Modify: `debug_ui/tests/control_router_test.cpp`

**Interfaces:**
- Consumes only `DebugPanelContext`.
- Core panel registrations are explicit through functions declared in `builtin_panels.hpp`.

Required control/state mapping:

```text
Control / Tools       -> active tool, selection mode, eraser mode
Control / Ink         -> Vector/Marker/Chalk/Membrane + resolved revision
Control / View        -> camera state; Pan; Zoom At; Fit Content; Fit Selection; Fit Primary Object
Control / History     -> Undo/Redo
Inspect / Selection   -> selection count + primary object + selection mode
Inspect / Interaction -> active pointer/input owner summaries already available
Runtime / ARC         -> preview revision/active pointer/input batches/handoff/preview active
Runtime / Surface     -> resolved mode/generations/size/DPR/available/present/lost + Surface controls
Runtime / Advanced    -> supported AxiomDebugControl actions
Performance           -> telemetry/live metrics + Reset Rolling Metrics
```

- [ ] **Step 1: Add control-state tests**

Assert:

- Undo/Redo enablement derives from product snapshot;
- Fit Selection is disabled with zero selected objects;
- Fit Primary Object is disabled with no primary object;
- Reset View / 100% controls do not exist;
- unsupported Trace/GPU Timing remain represented as Unsupported;
- persistent mode controls render from resolved snapshot after submission, not a local static variable.

- [ ] **Step 2: Verify failure**

```bash
cmake --build out/px0-host-debug --target axiom_debug_ui_workbench_state_tests
```

- [ ] **Step 3: Implement panel registrations and controls**

Do not add brush size/opacity mutation unless PX0-00 found an already-approved product-safe contract. Otherwise render those rows Unsupported/read-only as specified.

- [ ] **Step 4: Run tests**

```bash
cmake --build out/px0-host-debug
ctest --test-dir out/px0-host-debug --output-on-failure -R "DUI|PX0"
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/src/panels debug_ui/include/canvas/debug_ui/builtin_panels.hpp \
        debug_ui/CMakeLists.txt debug_ui/tests
git commit -m "feat(debug-ui): migrate existing controls into workbench"
```

### Task 12: Add Scenarios workspace without semantic bypass

**Files:**
- Create: `debug_ui/src/panels/scenarios.cpp`
- Modify: `debug_ui/include/canvas/debug_ui/builtin_panels.hpp`
- Modify: `debug_ui/tests/workbench_state_test.cpp`

**Interfaces:**
- Scenarios may register only sanctioned existing scenario/fixture actions.
- If no safe Reset/Empty/Ink Baseline seam exists, show them as Unsupported; do not add a Document/Scene mutation API.

- [ ] **Step 1: Write test that unsupported scenario controls do not call RuntimeFacade or any raw owner**

- [ ] **Step 2: Verify failure**

- [ ] **Step 3: Implement the Scenarios shell and only the sanctioned actions found in PX0-00**

- [ ] **Step 4: Run workbench tests**

```bash
ctest --test-dir out/px0-host-debug --output-on-failure -R PX0Workbench
```

Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/src/panels/scenarios.cpp \
        debug_ui/include/canvas/debug_ui/builtin_panels.hpp \
        debug_ui/tests/workbench_state_test.cpp
git commit -m "feat(debug-ui): add bounded scenarios workspace"
```

**Codex WP boundary:** PX0-05 migrates only already-authorized capabilities. No new Shape/RichText/Connector/Snap/Image behavior.

---

# Work Package PX0-06 — Host Convergence + Temporary Toolbar Cleanup

### Task 13: Make WindowsDebugUiHost a thin presentation/input host

**Files:**
- Modify: `debug_ui/include/canvas/debug_ui/windows_host.hpp`
- Modify: `debug_ui/src/windows_host.cpp`
- Modify: `apps/ink_playground/platform/windows/main.cpp`
- Modify: `apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py`
- Modify: `apps/ink_playground/tests/windows_debug_ui_selection_contract_test.py`

**Interfaces:**
- WindowsDebugUiHost exposes one controller setter:

```cpp
void setController(DebugController* controller) noexcept;
```

- It no longer exposes setters for RuntimeFacade, diagnostics providers, telemetry, AxiomDebugControl, PlatformDebugControl, or a platform-owned snapshot-refresh callback.
- It retains InputCaptureGate sharing and overlay/presentation lifecycle.

- [ ] **Step 1: Update structural tests first**

Assert:

- no `setRuntimeFacade` / `setDiagnostics` / `setArcDiagnostics` / `setPlatformDiagnostics` / `setTelemetry` / `setAxiomDebugControl` / `setPlatformDebugControl` on WindowsDebugUiHost;
- `setController` exists;
- `buildDebugSnapshot` no longer exists in Windows main;
- InputCaptureGate sharing remains.

- [ ] **Step 2: Run tests and verify failure**

```bash
python -m pytest \
  apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py \
  apps/ink_playground/tests/windows_debug_ui_selection_contract_test.py -q
```

Expected: FAIL against the old host.

- [ ] **Step 3: Compose DebugController in Windows main and thin the host**

The composition root owns provider objects and DebugController. The host only invokes controller frame construction between `ImGui::NewFrame` and `ImGui::Render`.

- [ ] **Step 4: Route temporary Windows toolbar product mutations through RuntimeFacade**

Replace direct product mutations in `selectWindowsTool`/toolbar handlers with calls to the concrete RuntimeFacade product controls. Do not route G4.7 qualification-only text scenario menus through new product APIs in PX0; they remain qualification harness behavior unless separately authorized.

- [ ] **Step 5: Run tests**

```bash
python -m pytest \
  apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py \
  apps/ink_playground/tests/windows_debug_ui_selection_contract_test.py \
  apps/ink_playground/tests/windows_debug_ui_encoding_contract_test.py -q
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add debug_ui/include/canvas/debug_ui/windows_host.hpp \
        debug_ui/src/windows_host.cpp \
        apps/ink_playground/platform/windows/main.cpp \
        apps/ink_playground/tests/windows_*debug_ui*_test.py
git commit -m "refactor(windows): thin debug ui host and unify product controls"
```

### Task 14: Converge Web on the common controller and RuntimeFacade path

**Files:**
- Modify: `apps/ink_playground/platform/web/bridge.cpp`
- Modify: `apps/ink_playground/platform/web/index.html`
- Modify: `apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py`

**Interfaces:**
- Web `DebugUiState` owns/uses the common DebugController.
- Web bridge contains no platform-local `debugSnapshot(...)` assembler and does not call `buildImGuiPanels`.
- Existing exported temporary toolbar functions remain source-compatible where practical, but their mutation implementation must enter RuntimeFacade rather than direct Host product mutation.

- [ ] **Step 1: Update Web structural tests**

Assert:

- `DebugController` is used;
- `buildImGuiPanels` and local `debugSnapshot(` are absent from Web bridge;
- independent Debug canvas/toggle/input mapping remains;
- temporary tool/brush mutation enters the RuntimeFacade adapter.

- [ ] **Step 2: Verify failure**

```bash
python -m pytest apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py -q
```

Expected: FAIL against the old bridge.

- [ ] **Step 3: Implement common controller composition**

Keep Web canvas/context ownership and ImGui input event mapping unchanged.

- [ ] **Step 4: Build Web physical target**

```bash
cmake --preset g4-5-web-debug-ui-physical
cmake --build --preset g4-5-web-debug-ui-physical --target axiom_ink_playground_web
```

Expected: build succeeds.

- [ ] **Step 5: Run structural test**

```bash
python -m pytest apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py -q
```

Expected: PASS.

- [ ] **Step 6: Commit**

```bash
git add apps/ink_playground/platform/web/bridge.cpp \
        apps/ink_playground/platform/web/index.html \
        apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py
git commit -m "refactor(web): converge debug ui on common controller"
```

**Codex WP boundary:** PX0-06 converges host/control paths. Do not remove temporary toolbar presentation entirely; final retirement belongs to PX7 or a separately authorized successor.

---

# Work Package PX0-07 — Closure / Evidence

### Task 15: Complete extension proof and architecture-boundary tests

**Files:**
- Modify: `debug_ui/tests/panel_registry_test.cpp`
- Modify: `debug_ui/tests/runtime_contract_test.cpp`
- Modify: `debug_ui/tests/snapshot_assembler_test.cpp`
- Modify: `debug_ui/tests/control_router_test.cpp`
- Modify: `debug_ui/tests/workbench_state_test.cpp`
- Modify: `debug_ui/tests/debug_ui_test.cpp`
- Modify: `debug_ui/CMakeLists.txt`

**Interfaces:**
- Extension proof is compile/test-only and must not introduce Shape product behavior.

- [ ] **Step 1: Add the synthetic feature contribution proof**

Register one synthetic contribution in Control/Feature and one in Inspect/Object or Inspect/Interaction. Assert both are discoverable/renderable through registry contracts without changes to Workbench core or platform hosts.

- [ ] **Step 2: Add boundary regressions**

Tests must pin:

- RuntimeFacade is independent of RuntimeDiagnostics;
- no panel context has owner/internal pointers;
- no global Capability[] is required;
- generic DebugCommand is absent;
- one missing provider does not block other snapshot sections;
- Fit Selection/Object do not fall back;
- Activity is bounded;
- mixed generation is visible.

- [ ] **Step 3: Run complete native Debug UI suite**

```bash
cmake --build out/px0-host-debug
ctest --test-dir out/px0-host-debug --output-on-failure -R "DUI|PX0"
```

Expected: all PASS.

- [ ] **Step 4: Run all structural platform contracts**

```bash
python -m pytest \
  apps/ink_playground/tests/web_imgui_debug_ui_contract_test.py \
  apps/ink_playground/tests/windows_imgui_skia_debug_ui_contract_test.py \
  apps/ink_playground/tests/windows_debug_ui_selection_contract_test.py \
  apps/ink_playground/tests/windows_debug_ui_encoding_contract_test.py -q
```

Expected: all PASS.

- [ ] **Step 5: Commit**

```bash
git add debug_ui/tests debug_ui/CMakeLists.txt apps/ink_playground/tests
git commit -m "test(debug-ui): close PX0 architecture contracts"
```

### Task 16: Run Windows/Web physical closure and record evidence

**Files:**
- Create: `docs/quality/evidence/px0-debug-control-plane/README.md`
- Add only existing-project evidence artifacts/log references required by the current Gate process.

**Interfaces:**
- Consumes exact result revision produced by Tasks 1–15.
- Produces a human-readable evidence index; official Gate verdict remains owned by later Gate review.

- [ ] **Step 1: Build Windows hosted target on Windows**

From a Windows environment with the locked Skia SDK available:

```bash
cmake -S . -B out/px0-windows-debug-ui -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCANVAS_BUILD_POC01=OFF \
  -DCANVAS_BUILD_RENDER=ON \
  -DCANVAS_BUILD_ARC=ON \
  -DCANVAS_BUILD_INK_PLAYGROUND=ON \
  -DAXIOM_BUILD_DEBUG_UI=ON \
  -DAXIOM_DEBUG_UI_PROFILE=full \
  -DCANVAS_SKIA_SDK_ROOT="$env:AXIOM_SKIA_WINDOWS_SDK_ROOT" \
  -DBUILD_TESTING=ON
cmake --build out/px0-windows-debug-ui --target axiom_ink_playground_windows
```

Expected: build succeeds.

- [ ] **Step 2: Physically validate Windows**

Record PASS/FAIL observations for:

```text
show/hide
workspace navigation
Vector/Marker/Chalk/Membrane
Object/Partial Eraser
selection mode
Undo/Redo
Pan
Zoom At
Fit Content
Fit Selection
Fit Primary Object
Surface Platform/CPU/GPU controls where supported
ARC diagnostics visibility
Surface diagnostics visibility
telemetry visibility
Activity request/receipt updates
hidden Debug UI does not consume Canvas input
Debug-owned pointer/keyboard sequence does not leak to Canvas
```

Do not claim PASS for a control marked Unsupported by the approved Spec/current owner.

- [ ] **Step 3: Build and serve Web hosted target**

```bash
cmake --preset g4-5-web-debug-ui-physical
cmake --build --preset g4-5-web-debug-ui-physical --target axiom_ink_playground_web
python -m http.server 8000 --directory out/g4-5-web-debug-ui-physical/apps/ink_playground
```

Open the hosted page in a browser and perform the same applicable workbench/control/input-isolation checks.

- [ ] **Step 4: Record exact result identity and evidence locations**

`README.md` must record exact result revision, build commands, platform/build identities, which controls were Available vs Unsupported, and where physical artifacts/logs reside. It must not issue the official Gate PASS verdict.

- [ ] **Step 5: Commit the evidence index**

```bash
git add docs/quality/evidence/px0-debug-control-plane
git commit -m "evidence: record PX0 debug control plane qualification"
```

**Codex WP boundary:** PX0-07 is closure/evidence only. Do not begin PX1 Shape implementation inside this package.

---

# Codex Work Package Split

The implementation executor should receive PX0 as eight sequential, separately reviewable packages:

| WP | Authorized result | Primary files | Entry dependency | Stop condition |
| --- | --- | --- | --- | --- |
| PX0-00 | exact baseline reconciliation | reconciliation doc only | approved Spec | any material authority/reality conflict |
| PX0-01 | RuntimeFacade/Diagnostics boundary cleanup | runtime/foundation + adapter migrations | PX0-00 READY | stable ABI or semantic redesign required |
| PX0-02 | structured snapshot + common assembler | debug_ui snapshot/assembler | PX0-01 | platform-specific schema becomes necessary |
| PX0-03 | router + activity + generic queue retirement | debug_ui control core | PX0-02 | product feature semantics required |
| PX0-04 | six-workspace shell + registry/extension proof foundation | debug_ui workbench/core | PX0-03 | docking/plugin framework required |
| PX0-05 | migration of existing controls, including View/Camera | debug_ui panels | PX0-04 | missing product-safe contract for a requested mutation |
| PX0-06 | Windows/Web host convergence and temporary toolbar routing | platform hosts/bridges | PX0-05 | requires product UI redesign |
| PX0-07 | contract closure + Windows/Web physical evidence | tests/evidence only | PX0-06 | implementation defect or environment blocker |

## Required review points between WPs

- After PX0-01: verify no accidental Runtime C ABI change and Product Shell can consume RuntimeFacade without diagnostics.
- After PX0-02: verify snapshot ownership/coherence before UI work begins.
- After PX0-03: verify every mutable action has exactly one owner path and no new generic DebugCommand exists.
- After PX0-04: verify a synthetic feature can extend Control + Inspect without modifying Workbench.
- After PX0-05: verify View/Camera, Surface, history, Brush/Eraser, selection, Runtime actions, and telemetry match the approved control inventory.
- After PX0-06: verify Windows/Web hosts contain presentation/input realization only and temporary toolbar product mutation goes through RuntimeFacade.
- After PX0-07: return exact result revision/materialized ref and evidence refs for independent Gate review; do not self-issue P34 PASS.

## Plan Self-Review Result

- **Spec coverage:** all three approved architecture layers, detailed control-surface contract, WP ordering, Windows/Web host convergence, and closure requirements map to explicit tasks.
- **Scope:** PX0 remains one subsystem (Debug Control Plane) and is appropriately represented by one plan split into eight execution WPs.
- **Type consistency:** RuntimeFacade/product state/diagnostics names used by later tasks are defined in PX0-01; DebugSnapshot types are defined in PX0-02; router/activity types in PX0-03; panel/workbench types in PX0-04.
- **Review Focus coverage:** each of the five high-risk cases has an owning test task.
- **YAGNI check:** no Shape/RichText/Connector/Snap/Image product semantics, dynamic plugin system, docking, public ABI redesign, or new persistence behavior is authorized.

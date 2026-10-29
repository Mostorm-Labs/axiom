# Phase 2 property source audit at `eecee8f2`

This is a read-only source inventory, not an implementation or test result. The canonical property store is `ObjectRecord.properties`; the existing Scene summary reads the published `RuntimeSceneRecord.properties`, using `semantic::propertyAppliesToObjectKind` and `semantic::propertyDefaultValue`. The public `SelectionSummary` has aggregate slots only for 1/2/3. `CanvasRuntimeFacade::Binding::apply` rejects the existing selected-object mutation controls for multiple roots, and `CanvasEditingRuntime::patchSelectedProperty` requires exactly one. Consequently, none of the rows below currently has a proved end-to-end multi-selection projection, typed skipped-root receipt, or batch command.

The existing semantic payload is `PatchPropertiesOp`; the validator checks applicability and type in `operation_state_validator.cpp`. The D07 `prepareUndoableCommand`/`EditorHistory` path can derive inverse patches from real preimages, but its use for each Phase 2 batch remains unverified. Locked/denied/stale admission and the ability to represent per-target outcomes must be bound by the corrected P31.

| Field | Registry applicable kinds / canonical default | Existing read and command observation | Multi-root disposition at subject |
| --- | --- | --- | --- |
| Visible `0x1` | 1–9 / `true` | Summary slot 1; single-root `kSelectedVisible` → `patchSelectedProperty` | Not implemented end to end; Facade rejects multiple roots |
| Locked `0x2` | 1–9 / `false` | Summary slot 2; single-root `kSelectedLock` / unlock → `patchSelectedProperty` | Not implemented end to end; explicit unlock and failure rules need proof |
| Opacity `0x3` | 1,2,3,4,7,8,10 / `1.0` | Summary slot 3; single-root selected appearance reads opacity | Not implemented end to end; Facade rejects multiple roots |
| Blend mode `0x4` | 1,2,3,4,7,8 / normal | Semantic property/default exist; no dedicated slot in `SelectionSummary` | Provider / projected value / control mapping unbound |
| Fill `0x100` | 1,3,8,10 / no fill | Single selected appearance reads solid fill; semantic field supports richer typed value | Multi-root projection and typed mutation / receipt unbound |
| Stroke `0x101` | 1,3,7,8 / no stroke | Single selected appearance reads solid stroke; current setter preserves cap/join/dash | Multi-root projection and Connector-specific behavior unbound |
| Connector start `0x200` | 7 / none | Semantic field/default exist; no dedicated summary slot | Provider / command / receipt unbound |
| Connector end `0x201` | 7 / none | Semantic field/default exist; no dedicated summary slot | Provider / command / receipt unbound |

Sources: `schema/axiom/v1/registry/field_registry_v1.yaml`; `runtime/semantic/src/normalizer.cpp:14`; `runtime/semantic/src/validator.cpp:354`; `runtime/semantic/src/operation_state_validator.cpp:31`; `runtime/interaction/src/canvas_selection_summary.cpp:21`; `runtime/interaction/src/canvas_editing_runtime.cpp:1941`; `runtime/interaction/src/undoable_command.cpp:120`; `runtime/interaction/src/editor_history.cpp:155`; `runtime/composition/src/canvas_runtime_facade.cpp:24` and `:183`. Registry and semantic validation establish meaning/applicability, not a feature provider or a usable UI control.

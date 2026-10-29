# Current source-owner bindings — proposed, not approved

- Hierarchy/pose: `runtime/semantic/src/container_topology_plan.cpp` and `container_operation_validation.cpp` own the existing topology operation; `runtime/interaction/src/canvas_section_commands.cpp` contains Section-specific construction, pose and guards that can be studied without treating Section actions as Group commands.
- Selection and scope: `runtime/interaction/src/selection_session.cpp`, `canvas_selection_summary.cpp`; current summary explicitly reports Group/Ungroup NotImplemented.
- History: `runtime/interaction/src/undoable_command.cpp` and `editor_history.cpp` already build inverse and reserve existing History. Preserve D07 prepared commit/Scene behavior and known OOM status.
- Product ingress: `runtime/foundation/include/canvas/runtime/runtime_facade.hpp`, `runtime/composition/src/canvas_runtime_facade.cpp`, `runtime/composition/include/canvas/runtime/runtime_composition.hpp`. Existing typed `SectionIntent` cannot by itself be assumed to represent Group; bind the concrete Group intent and receipt against the accepted ABI constraint before authorizing source edits. Do not put hierarchy mutation in the UI.
- Delivery: existing Windows and Web Minimal Hosts plus Debug UI need actual producer/consumer test bindings; exact files and admission path require inspection.
- Existing regressions: `FC08SectionConsumer`, `FC01SelectedActions`, `FC03ImageFacade`, `FC05ConnectorFacade`, `FC05ConnectorDragPreview` and selection MSE cases run through their real owners. No test runner for GROUP-01..06 is presently bound; do not count a source-only or zero-match check as proof.

The proposed arithmetic corpus independently computes transformed translations: W=(0,2,-2,0,100,200) times local translations (10,20) and (30,40) gives world translations (60,220) and (20,260). Accepted P12 defines the numeric error budget. The candidate is not authoritative until reviewed and frozen.

# GT-G4.6-A — Entry Reconciliation + Editing Presentation

## Anchor
- repository: `Mostorm-Labs/axiom`
- anchor: `cc985b174cafc5d5dd743bd66701cfe117278de8`
- required relation: ancestor

## Confirmed reusable seams at anchor
- `runtime/interaction/include/canvas/interaction/selection_state.hpp`
- `runtime/interaction/include/canvas/interaction/transform_session.hpp`
- `runtime/render/include/canvas/render/frame_state.hpp`
- `runtime/render/include/canvas/render/render_view_runtime.hpp`
- `runtime/foundation/include/canvas/runtime/runtime_facade.hpp`
- existing Debug UI/telemetry and Ink Playground verification infrastructure

## First action
Resolve exact composition/render files that own per-view transient overlay at the observed descendant. If the accepted seam is absent or conflicts with Authority, stop with AUTHORITY_CONFLICT/MISSING_REQUIRED_INPUT rather than inventing a second scene tree.

## Scope intent
This package converts existing G4 interaction semantics into production editor presentation. It does not redesign selection, transform, history, input, brush, Arc, or canonical document semantics.

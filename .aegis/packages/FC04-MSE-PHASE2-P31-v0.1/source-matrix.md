# Phase 2 Source Matrix — Starting Point at 6bcf2d4

This is a source-bound starting inventory, not a claim that end-to-end property editing already exists. The executor must fill every owner column from the actual worktree and preserve exact blob identities.

## Existing read observations

- Selection root normalization and summary: runtime/interaction/src/selection_session.cpp and runtime/foundation/include/canvas/runtime/selection_summary.hpp.
- Current SelectionSummary already has aggregates for FieldId 1/2/3 (Visible, Locked, Opacity) and fields applicableRoots/knownRoots, Uniform/Mixed/Partial/Unavailable state, but not per-root typed reason details for every field.
- CanvasEditingRuntime::patchSelectedProperty currently rejects unless selectedObjectCount()==1; therefore the existing primary-only path is not sufficient for D06-B.
- CanvasEditingRuntime::selectedObjectRecord similarly returns only when exactly one object is selected.
- RuntimeFacade has existing readSelectionSummary, readRuntimeState and typed CanvasControl request/receipt paths. Do not add a virtual RuntimeFacade method or change public ABI.
- CanvasControlSnapshot currently contains selectedObjectCount, selectedObject, selectedAppearance, selectedLocked and selectedVisible, but those are not proof of a coherent multi-root property projection.
- operation_state_validator.cpp and the frozen Field Registry establish declared semantic applicability/type validation only; they do not establish property read, capability, write and History completeness.
- EditorHistory and the D07 explicit Intent path are available in this source cursor, but all Phase 2 use must be proved by tests.

## Property matrix required from P32

For each row, add: read owner/API, applicability owner, capability/reason projection, command/control request, validation, preimage/inverse, History unit, changed/skipped per-target result, test IDs, and final disposition.

| Property | FieldId | Declared kinds | Starting disposition |
|---|---:|---|---|
| Visible | 0x00000001 | Shape, Image, VectorPath, RichText, VectorStroke, DabStroke, Connector, Sticky, Group | Requires full owner audit; selection summary has aggregate slot |
| Locked | 0x00000002 | Shape, Image, VectorPath, RichText, VectorStroke, DabStroke, Connector, Sticky, Group | Requires full owner audit; unlocking policy must remain safe |
| Opacity | 0x00000003 | Shape, Image, VectorPath, RichText, Connector, Sticky, Section | Requires full owner audit; selection summary has aggregate slot |
| BlendMode | 0x00000004 | Shape, Image, VectorPath, RichText, Connector, Sticky | Requires full owner audit |
| FillStyle | 0x00000100 | Shape, VectorPath, Sticky, Section | Requires full owner audit; subset policy applies |
| StrokeStyle | 0x00000101 | Shape, VectorPath, Connector, Sticky | Requires full owner audit; subset policy applies |
| Connector start/end decoration | 0x00000200 / 0x00000201 | Connector only | Connector-specific owner and readback required |

If any property cannot be safely supported without new public ABI or new canonical semantics, mark it UNAVAILABLE_WITH_REASON only if the accepted product scope permits deferral; otherwise stop with AUTHORITY_CONFLICT. Do not silently omit it or falsely claim Phase 2 closure.

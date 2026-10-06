# GT-G4.6-D — Debug UI Control Surface + Selection Host Integration

This package consumes the G4.6-A editing presentation seams. It adds the
product-facing control surface needed to exercise them from the Windows
playground without creating a second runtime authority.

The selection tool is a product control request. A selection click is routed to
the existing scene hit-test and SelectionSession. It must not enter the brush
session path and must not create a canonical operation. Selected-object chrome
is transient per-view presentation and is rendered from the existing
EditingOverlay seam.

The ImGui panel is reorganized as capability-aware tabs. Each tab is a consumer
of immutable snapshots and typed owner controls. Unsupported capabilities are
shown as unavailable rather than represented by fake state.

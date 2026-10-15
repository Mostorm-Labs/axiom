# FC-00X Interaction Internalization - P31 implementation context
## Execution identity
Repository: Mostorm-Labs/axiom
Execution branch: codex/fc00x-interaction-internalization-p31
Task anchor: 965612f6a37e102e94c2d2fa1120ff9d9a9e1565 (ancestor)
Previous package: .aegis/packages/FC-00X-RUNTIME-MIN-CLOSURE-P31-v0.4/package.json@f2fcc0e3152337cf88caee7ba7afa0306389d6da
Previous implementation: 6282d0a2f31bb480d51b50f1b6286f41191180fc
Previous durable result branch head: 965612f6a37e102e94c2d2fa1120ff9d9a9e1565
Previous findings: full graph BLOCKED_ENVIRONMENT from existing MSVC standard-header failures; physical host PHYSICAL_PENDING; no implementation defect and no P36 repair required.
This is preparation only and authorizes no P32 mutation until Control Review accepts the package.

## Current flow
Windows/Web native adapter -> PlatformInteractionIngress/RuntimeAppBinding -> Host InputRouter/CanvasInteractionCoordinator/InteractionSessionManager -> Creation/Brush/Selection/Transform/Viewport/Eraser sessions -> OperationEngine/EditorHistory/Scene publication -> RuntimeFrameCoordinator/PreviewSurfaceController/CanonicalHandoff -> platform acquire/present and Control/Debug readback.

## Known internalization boundary
apps/ink_playground/common/ink_playground_host.cpp::acceptPlatformBatch and its selectionPointer, creationPointer, eraser and Brush lifecycle helpers still make shared business decisions.
Windows main.cpp still selects behavior and owns preview worker/gates, HUD and presentation plumbing.
Web bridge.cpp still requires exact Up/Cancel/pressure/pointer provenance audit.

## O0-O5
O0 freezes owner/port and verification maps.
O1 preserves PointerKey -> SessionId -> Preview/commit identity, source loss and late-generation fencing, pressure and provenance.
O2 moves existing Session lifecycle decisions to Runtime owners and preserves accepted eraser semantics.
O3 reuses OperationEngine/History/Scene and frame/preview/canonical owners.
O4 makes Windows/Web/Control/Diagnostics consume one Runtime path, fixes Web field fidelity and removes independent HUD.
O5 proves a minimal non-Playground Host and adapter fixtures and cleans obsolete business entry points.

## Non-goals
No new semantic objects or operations, no canonical serializer/golden change, no public C ABI, no full Editor extraction, no POC-06/Arc revival, no Android build/APK, no G5 work, and no mutation to Authority/Gates/Evidence/Integrations or historical FC/PX0/G4 results. Use locked Semantic/Skia prebuilt SDKs only.

## First action after package acceptance
Run RepositoryIdentity, PackageBindingPreflight and EvidenceContractPreflight, then O0 ImplementationDesignPreflight and the frozen RED oracle before any production change. If the old oracle passes, stop as VERIFICATION_DESIGN_DEFECT.

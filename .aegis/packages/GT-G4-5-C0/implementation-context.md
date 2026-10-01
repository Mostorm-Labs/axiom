# GT-G4-5-C0 implementation context

## Purpose

Move remaining interaction orchestration and viewport transform/navigation semantics from app/platform code into runtime/interaction without changing accepted G4.4 behavior.

## Trusted starting point

Repository: Mostorm-Labs/axiom
Execution ref: codex/g4-5-runtime-interaction-consolidation
Task anchor: 4c4a2a727ff6a61374f6b216de7fa4e51c85ba7c
Anchor relation: exact ancestor

This anchor already descends from main@518b9b7a66924ef2ea96af4cfa11fdce4b0d721b and includes the accepted R01 perfect-freehand result.

## Existing semantics to preserve

Do not redesign MultiContactCoordinator or TwoFingerViewportGesture.

AutoIntent is the current product guarantee.
MultiInk and GesturePriority remain deferred non-blocking seams.
P34-NB-01 third-plus contact behavior remains accepted and unchanged.

## Current ownership leak

InkPlaygroundHost owns contact caches, coordinator instance, viewport gesture state, committed viewport transform and view-to-content transform.

Web index.html additionally owns trackpadScale/trackpadTranslation and wheel/browser-gesture viewport accumulation.

## Target

Add CanvasInteractionCoordinator and ViewportInteractionController in runtime/interaction.

The coordinator returns typed routing actions/results. The composition root applies those actions to InkEngine/PreviewModel/InteractionRuntime but does not re-decide semantics.

Web normalizes OS/browser navigation events into ViewportNavigationSample. Common runtime owns cumulative transform.

## First incomplete action

Validate package/hash/anchor bindings, run ImplementationDesignPreflight, add and run the frozen RED oracles against the old ownership, then execute the complete closure contract until READY_FOR_D0_PACKAGE_REVIEW or an explicit terminal blocker.

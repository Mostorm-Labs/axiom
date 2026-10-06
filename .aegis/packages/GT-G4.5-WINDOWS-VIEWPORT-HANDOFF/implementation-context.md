# GT-G4.5-WINDOWS-VIEWPORT-HANDOFF Implementation Context

## Trusted baseline

- Repository: Mostorm-Labs/axiom
- Execution branch: `codex/gt-g4-5-windows-viewport-handoff`
- Task anchor: `77b40bef01627f75aa72db8519bed5e4ba8cca7f`
- Parent result: `GT-G4.5-FINAL-VECTOR-CLOSURE`, READY_FOR_G4_5_10_PHYSICAL_VERIFICATION

## Observed failure boundary

Windows starts the common host with deferred platform presentation. The common
viewport navigation path updates camera and retained preview transform but skips
Canonical presentation in deferred mode. The Windows render pump also gates
Canonical work on active pointer state, so a pinch can remain visually stale.

After the pinch settles, a first stroke can leave the transient preview visible
until a later input event if the deferred Canonical frame and matching
CanonicalVisible receipt are not completed in the same render cycle.

## Required ownership

`Runtime/InkPlaygroundHost` owns viewport invalidation and Canonical frame
eligibility. The Windows host owns scheduling only. `CanonicalVisible` remains
the handoff authority; pointer-up and later pointer-down must not retire or
resurrect preview independently.

## First executable actions

1. Run repository identity, package hash, anchor, and evidence preflights.
2. Record the ImplementationDesignPreflight and RED oracle against the current
   task anchor.
3. Add failing focused tests before production mutation.
4. Implement the smallest host scheduling change that passes those tests.
5. Build, run focused and regression suites, materialize exact evidence, and
   return `READY_FOR_CONTROL_REVIEW` or an explicit blocker.

## Preserved behavior

BrushEngine, AutoIntent semantics, multi-contact ink behavior, Vector/DAB
rendering, CanonicalVisible identity, Arc lifecycle, Debug UI, Undo/Redo, and
the Windows D3D12/Skia surface-provider architecture remain unchanged.

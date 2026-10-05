# GT-G4.5-FINAL-VECTOR-CLOSURE Implementation Context

## Trusted baseline
- repository: Mostorm-Labs/axiom
- task anchor: e73dc2c4fe9ea5e509b31fe7c72363d53859fee8
- parent implementation branch: codex/gt-g4-5-web-debug-ui-physical
- execution branch: codex/gt-g4-5-final-vector-closure

## What is already complete and must be preserved
- BrushPackage -> ResolvedBrushState -> BrushSession -> BrushExecutionSnapshot / BrushStrokeRecord / BrushVectorOutput semantic path.
- Pinned Perfect-Freehand correctness path and current full-history evaluator.
- Complete BrushPreviewDelta contour per revision and one retained transient contour per active session.
- Windows D3D12/Skia canonical provider and independent D3D12/Skia transient preview provider.
- CanonicalCommitted / canonical presentation / CanonicalVisible / matching-session preview retirement.
- Web Debug UI physical host, Debug UI architecture, and Undo/Redo.

## First incomplete implementation boundary
Preview and canonical/headless Vector Brush raster paths independently rebuild the same closed SkPath from BrushVectorOutput.outline. The current behavior is correct but duplicated and can drift.

Implement one shared Brush outline -> SkPath helper and route both qualification paths through it without changing the current moveTo/lineTo/close/fill morphology.

## Qualification closure
After the refactor:
1. prove Perfect-Freehand final geometry remains deterministic and chunking-independent;
2. prove package-driven parameterized semantics using existing real profiles;
3. qualify the same helper/corpus on headless/Web/Android/macOS;
4. rerun exact Windows automated preview/canonical handoff qualification;
5. export workload/regression/evidence bundle;
6. return READY_FOR_G4_5_10_PHYSICAL_VERIFICATION.

## Explicit non-goals
Do not implement live brush sliders, incremental PF compute optimization, Space diagnostic, new Arc semantics, Windows surface redesign, Debug UI redesign, Undo/Redo changes, DAB expansion, or G5 optimization.

## First action
Run repository identity + package/hash/anchor preflights, record the actual starting revision, run ImplementationDesignPreflight, then implement the shared SkPath helper. Continue through automated qualification/evidence materialization until terminal success or an explicit frozen blocker.

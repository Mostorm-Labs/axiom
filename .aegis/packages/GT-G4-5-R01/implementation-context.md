# GT-G4-5-R01 implementation context

## Purpose

Freeze an exact perfect-freehand reference corpus and implement a pure C++ one-shot geometry kernel that matches the pinned source. This task intentionally stops before Skia or any platform integration.

## Trusted starting point

Repository: Mostorm-Labs/axiom
Execution ref: codex/g4-5-pf-reference-conformance
Task anchor: 52acd2fb9274b201dd4eee478e49ac35937ed3cf
Anchor relation: ancestor

The interrupted 09V P32 branch at a02ebdb1d811cfbe6e22295ddcacb66cedec49b9 is not the execution base. Do not cherry-pick its triangle-strip geometry, Windows GDI Arc work, semantic validator change, or renderer changes.

## External reference

Repository: steveruizok/perfect-freehand
Commit: 176e00f2399f4969e1b0965c5921d96a3e50ce9f

Read reference.lock.json first for exact required blob SHAs and known issue identities. Fetch only the pinned commit. Verify source blobs before using upstream output as expected data.

## R0 first

Before production C++ kernel mutation:
1. verify upstream source bindings;
2. retain MIT attribution;
3. materialize corpus-plan.json to concrete input/options fixtures;
4. run the pinned upstream implementation to produce StrokePoint and outline outputs;
5. record generator/toolchain provenance;
6. materialize known-limitations registry and PF_REFERENCE_FROZEN evidence;
7. add R1 tests against the intended API and observe RED.

Expected output must not be computed by the C++ implementation under test.

## R1 target

Pure renderer-neutral double-precision one-shot API:
VectorStrokeInput -> conditioned VectorStrokePoint[] -> ordered VectorStrokeOutline.

Keep intermediate StrokePoint state test-visible. Match the pinned source structure closely enough that deviations can be localized by stage.

No Skia, no renderer, no BrushRuntime integration, no canonical schema/version work, no incremental optimization.

## First incomplete action

Resolve repository/package/hash bindings, run ImplementationDesignPreflight, execute R0 source/corpus freeze, then RED-oracle preflight. Continue through the full closure contract until READY_FOR_R2_PACKAGE_REVIEW or an explicit terminal blocker.

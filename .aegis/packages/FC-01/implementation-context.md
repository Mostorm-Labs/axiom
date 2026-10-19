# FC-01 — P31 Implementation Context (frozen)

## Execution identity

- Repository: `Mostorm-Labs/axiom`
- Trusted FC-00 task anchor: `2942c63b375366ed977b834aa0a48fe2fb551c6e` (must be an ancestor)
- Accepted FC-01 Authority/Golden control subject: `62a99c439e39636088c55e43cf7d76963bfe20eb`
- P31 package branch: `control/fc-01-p31-materialized-execution-package`
- Intended P32 branch: `codex/fc-01-creation-tools-p32`; create FROM THE EXACT P31 MATERIALIZATION COMMIT, not `main` or an old FC-00 head.
- PR #146 holds the accepted control ancestor (base `codex/fc-00-fc-track-authority-p31`); keep Draft/unmerged during P32. Git ancestry, not PR merge state, is the trust boundary.
- Control Review: https://app.notion.com/p/3f74c57a590c8123ba7fcf4ddb4c951f
- DEV gate: `FC-01-DEV` (development only). FC-13/PX0/G4.6/G4.7 PHYS remains deferred and independently owned.

## Progressive load and preflight

Read `package.json`, then this context, then `execution-contract.json`. Load `authority.lock.json`, `verification.lock.json` and `evidence-contract.json` as needed. Validate all package-local SHA-256 references and the pinned golden manifest/expected/wire Git blobs. Confirm origin/repository identity **before** resolving the package or task anchor. Confirm the accepted control revision is an ancestor of P32 starting revision, then perform PackageBindingPreflight and EvidenceContractPreflight.

The first incomplete step is **the ImplementationDesignPreflight and old-source `FC01-RED-01`**. Before modifying production code, reproduce `PlatformInteractionIngress::sourceLost(sourceA)` erroneously cancelling sourceB on old anchor and verify the independent B-alive oracle fails RED; old unexpected GREEN means Verification Design defect, not license to modify Expected.

## Confirmed code ownership at FC-00 anchor

- `runtime/control/`: CanvasControlService queue/receipt, current adapter calls `processPending` inline; new typed CreatePreset and selected commands/Readback not implemented or qualified.
- `runtime/foundation/`: existing CanvasToolKind Ink/Select/Eraser/Pan; do not alias BrushPresetRef or modify public R1 ABI without separate authority.
- `runtime/input/`, `runtime/interaction/`: GestureArbiter/PointerKey/Interaction ports and EditorHistory exist; sourceLost source isolation bug, no common CreateSession or complete FC-01 History inverse.
- `runtime/semantic/`: OperationEngine typed apply + Replay path lack independently proven universal structural/version admission; new Shape/2 tuple/reader profile not implemented.
- `runtime/scene/`, `runtime/render/`: existing SceneQuery/Skia and transient chrome; precise Connector hit mask and Shape2 derived render path are missing.
- `apps/ink_playground/common/`, `platform/windows/`, `platform/web/`, `platform/android/`, `debug_ui/`: thin host and Debug clients, not independent semantic owners. Win32 demo and Android Java demo do not prove RNW/RN Fabric shipping integration; do not invent production physical PASS.
- Axis is externally owned; use existing data/publication seam or a source-faithful deterministic test adapter for E09. Do not fabricate a second Axis implementation.

## Implementation slices: one complete P32 lifecycle

`FC01-I0` input/RED/version gate → `I1` Shape2 and compatibility → `I2` Scene/Connector hit/render → `I3` five-tool Facade/Create/Snap/Preview → `I4` selected edits/conditional History → `I5` host/Axis integration → `I6` full DEV verification and reviewer-accessible E01–E10 evidence.

Order is dependency-aware; code execution may choose bounded internal ordering while preserving all frozen obligations. Once P32 begins, continue through full executable scope until `READY_FOR_CONTROL_REVIEW` or a legitimate terminal blocker. Do not stop for another control approval after each slice.

## No-go and exact terminal result

No selected ShapeKind conversion, new 16th Op or Proto tags, generic payload2, FC-06 attachment/routing, FC-07 distribution, FC-10 clipboard, Text/Image feature suite, G5 performance work, extra renderer/data-owner, or changes to original Gates/PHYS. Old `SH-G06` must remain REJECT.

Required evidence is the ten distinct `FC01-B01..B10` obligations mapped to `E01..E10`, not ten duplicate CI workflows. The P20-authoring `SH2-G01..25`/SH2-G15 Expected is trusted, **not an executed suite**. P32 writes the planned runner and tests; do not auto-bless Expected using candidate output.

Return only exact `result_revision`, reviewer-resolvable `materialized_ref`, evidence/provider refs and true blocker classification. Codex does NOT declare P34, FC13 or G5 PASS.

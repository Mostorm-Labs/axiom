# FC-04 D07-A/B P32 — partial repair and terminal package-scope blocker

**Terminal status: TASK_PACKAGE_DEFECT. D07-A/B is not complete.** The supplied P31 requires real, unpublished Scene-publication preparation before the first canonical write, yet expressly excludes changes to the Scene owner and supplies no existing prepare-only seam. Do not start a grouped commit using immediate Scene `apply`, a detached substitute coordinator, or fabricated preflight failpoints.

## Exact task and state

Repository `Mostorm-Labs/axiom`; worktree `C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas`; branch `codex/fc04-d07-selection-audit`; starting revision and package revision `5db29ce84c30b14da2133d137f4e69b79ffe4a55`; P31 `package.json` SHA-256 `56962b59726eca31f3cb015779234df84926bbbac427b382e285aea8dd70aa77`. The anchor `832866a2d001a247c4eb374c1fb1899258a6f6cf` and feature base `c7f874395d55952e9d5288e2a336a21747f37f11` are ancestors. The worktree was clean at start and repository identity matched. Exact package preflights were rerun before source edits; all three returned PASS, exit 0. Historical v0.2 inputs and evidence remain unchanged.

The exact P32 source and test changes in the containing result commit are restricted to:

- `runtime/interaction/include/canvas/interaction/canvas_editing_runtime.hpp` and `runtime/interaction/src/canvas_editing_runtime.cpp`: preserve the admitted Operation's actual committed record by nothrow move instead of copying it after canonical write; reuse it for Scene publication after History installation. Preserve the existing single-command callback order. Block synchronous reentrant canonical writes while Scene consumes the retained record. The Semantic engine's committed result remains the source of the real Operation identity, generation, stamp and ChangeSet.
- `verification/fc04_d07/audit_test.cpp`: bind the two prior RED counterexamples to the approved legacy-port rejection and independent-command negative-control semantics; preserve their old raw results. Add a real-runtime grouped-intent positive test, which remains RED because the producer does not exist. Add real Scene coordinator publication/reentrancy characterization.

No explicit multi-Operation producer, group History, grouped Undo/Redo, forward-group reservation, or Scene prepare-only API was added. The 11 protected original D07 checks were not weakened. No schema, SDK lock, public C ABI, global registry, D06-B, main branch or unrelated product source was changed.

## Observations and genuine scope defect

The `FC04D07PostcommitReceiptGrowthFault` real process originally terminated at generation 2 / History 0 with exit 86. The repaired admitted path rerun records the actual post-commit allocation failure with History 1, returns `accepted=1`, and recovers Scene to generation 2; exit 0. This is narrow single-command evidence, not completion of all group OOM obligations.

`fc04.d07.D07.LiveSceneApplyIsNotUnpublishedPreparation` exercises the existing `IncrementalRuntimeCoordinator` with a staged post-state. It observes Scene generation advance and publication callbacks while canonical semantic generation is still unchanged. The characterization passes; it shows that using the actual live `apply` as a pre-write reservation would expose staged state. It is not a successful intent test.

The source boundary is concrete:

- `runtime/scene/include/canvas/scene/incremental_runtime_coordinator.hpp:46` exposes `plan`, `apply`, and `recover`; neither latter operation returns an unpublished prepared publication.
- `runtime/scene/include/canvas/scene/scene_types.hpp:130` puts `PreparedPublication`, `prepare`, `prepareIncremental`, and `publish` in the private RuntimeScene/Coordinator boundary.
- `runtime/scene/src/incremental_runtime_coordinator.cpp:206` performs participant preparation and, at lines 259–266, invokes `SceneBinding::synchronize` with the actual `publishPending` callback in the same `apply` call. Scene and observer publication cannot be deferred by the authorized Runtime caller through this API.
- The actual owner retains private observation/checkpoint state; constructing a detached coordinator and swapping it in would not demonstrate the existing coordinator's failure/observer contract. A fake/test-only preparer or relocating actual post-prefix faults into preflight would not meet the frozen oracle.

P31 `design.md` requires actual result/publication resources to be reserved before first write, and `test-matrix.json` requires real Scene-publication reservation fault injection. P31 `closure.md` explicitly stops if other source files are required, and `design.md` excludes Scene source changes. The P31 structural DesignPreflight did not catch this gap; its previous PASS remains historical and is not treated as evidence that this owner seam exists. This result does not prove that every conceivable in-scope alternative is impossible; it records that no real, owner-preserving prepare/commit seam is available or bound, and a faithful implementation would require the Scene ownership/scope decision or a concrete compliant existing seam.

Return to P31/control for the smallest targeted correction: specify an unpublished prepare / no-fail publish / abort seam in the existing Scene coordinator and its real participant owners, authorize only the precise Scene headers/implementation and focused tests actually needed, and bind real fault injection plus ordered per-Operation publication. Alternatively provide a demonstrably equivalent existing seam that keeps the actual coordinator, observers, checkpoints, generation/revision, and all frozen failure semantics. Retain the stronger D07 pre-write failure rule; do not silently substitute 07-14 compensation, widen to a global canonical Transaction, or change public ABI. No unresolved semantic Authority conflict has been asserted merely from the existence of per-Operation notification.

## Verification summary

Exact commands, exit codes and raw output are recorded in `commands.json` and `raw/`. A new independent native Release build used the locked Semantic SDK consumer facts, verified Windows Skia SDK, VS2026/MSVC and `/WX`.

- Fresh initial original D07 baseline: 11 PASS / 3 FAIL, CTest exit 8.
- Final native D07 audit: 16 PASS / 1 FAIL, CTest exit 8. The only failure is `fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit`, with the production entry genuinely absent. Original 14 IDs are all present; 11 protected passes, both explicitly rebound negative controls, and the original two fault processes pass. Two new real Scene boundary/reentrancy checks pass. Zero skipped.
- Exact 160 Selection test IDs: 160 PASS, 0 FAIL, 0 skipped, CTest exit 0; XML IDs compared to the frozen list.
- Public Runtime C ABI manifest: PASS, exit 0 (54 functions, 49 structs, 163 enum constants).
- A separate locked-SDK Web build using Emscripten 6.0.6 passed. Actual Chrome/WASM Selection batch 1/2/3 regression scripts passed 19/28/38 synthetic-input assertions respectively; these are not physical input or grouped-intent tests. The first attempted build used an unrelated Emscripten 3.1.27 installation and failed on existing Scene C++ compilation; that incorrect-toolchain attempt is retained in the logs and was superseded by the correctly matched fresh build, without lowering warning levels.

The added positive group test was first observed failing against the actual missing `CanvasEditingRuntime` producer. The Scene reentrancy test was observed RED before the guard and GREEN after. The initial OOM process RED (86) and final GREEN (0) are preserved in the logs. Browser test reports captured starting Git HEAD `5db29ce...` while the exact P32 source patch was uncommitted; `raw/source.patch`, resulting commit and built artifact hashes bind the tested content. No zero-test run, skip, expected-failure inversion, or test-only group implementation was used.

## Handoff

This P32 return is partial and blocked. Preserve the existing OOM fix, P31-approved negative-control rebindings, and all evidence, then resume from the precise result commit only after the Scene owner boundary is resolved under a corrected package. D06-B remains OPEN. There is no main merge or P34/PHYS PASS.

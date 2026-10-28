# FC-04 Phase 2 P31 control review — blocked package bindings

Review subject: `eecee8f24a952a93f96ee9a4f946f46bbdcf51a4`, branch `codex/fc04-mse-phase2-p31-v0-1-d07cursor`, repository `Mostorm-Labs/axiom`. Package `.aegis/packages/FC04-MSE-PHASE2-P31-v0.1/package.json`: SHA-256 `029663cf325ca36e3131e6ba7b499bb784925f4d478a5882f35f9461281d8c87`. The subject descends from the D07 reviewed cursor `6bcf2d4bbf3314e5252951190c3c475403f52665`. The worktree was clean at review start. The delta from that cursor consists only of the 13 versioned P31 files.

## Decision and scope

`BLOCKED_PACKAGE_BINDING`: the mechanical preflights pass, but the package has unresolved execution/verification bindings described below. The package itself retains `p32_authorized: false` and requires control review. This review does not authorize product edits, alter the existing package or D06-B authority, or declare a Gate verdict.

D06-B has already been accepted with limited scope by [the Current P23 authority](https://app.notion.com/p/4074c57a590c81d9b124cd243e44f205); no second P23 decision is needed to repeat that policy. D07's functional P34 and its separate known OOM/release blocker remain unchanged. Group/Ungroup is not redesigned here.

## Finding 1 — required real Facade owner omitted

`runtime/composition/src/canvas_runtime_facade.cpp:24`–`:28` rejects `kSelectedAppearance`, `kSelectedTransform`, `kSelectedLock`, `kSelectedVisible` and other selected-object commands whenever more than one object is selected; lines 87–95 route those controls into the real Interaction Runtime. `runtime/interaction/src/canvas_editing_runtime.cpp:1941` also limits `patchSelectedProperty` to one selected object. Editing only the allowed Interaction owner therefore cannot make the existing Windows/Web Facade command path accept the Phase 2 requests.

The actual Facade source file is neither in `allowed_modification_paths` nor in `source-lock.json`; its exact subject Git blob is `e4f80f3bcd03f16e1d03f2e1023a56069ecb3b85`. The package requires Debug UI/platform wiring and an actual Facade path while requiring an exact allowed source surface. This is `TASK_PACKAGE_DEFECT`, not permission to bypass the guard or to mutate a UI-local canonical copy.

## Finding 2 — protected Selection regression conflicts with requested change

`runtime/composition/tests/selection_input_integration_test.cpp:198` contains `SelectionInput.MultipleRootsRejectSingleObjectEditingAtRuntimeAndFacade`, which requires `kSelectedTransform`, `kSelectedAppearance`, `kSelectedLock` and several other controls to remain unsupported for multiple roots. It is run through inherited CTest ID `FC01ASelectionInputIntegration`, line 156 of `.aegis/results/FC04-D07-P31-CORRECTION-v0.2/raw/selection-names.txt`. Some of those exact commands are required to become multi-root capable in Phase 2.

The source test file, Git blob `dc682c33e32111957b923161d5844c9465e68fe1`, is neither allowed for modification nor source-locked. The repair must explicitly preserve the original 160 test IDs while updating only assertions whose behavior is intentionally superseded, and retain negative coverage for still-single-object/unsupported commands and zero-mutation rejection. Do not remove the test, silently change the expected ID set, or mark the old expectation as passed.

## Finding 3 — MSE-25 binding changed the accepted scenario

The accepted [FC-04 decision addendum](https://app.notion.com/p/4064c57a590c81a997ddfff3da30ccea) identifies `MSE-25` as Escape during TextEdit/IME composition, which must cancel the active edit/composition without also clearing selection or exiting scope. Candidate `test-matrix.json:126` instead assigns MSE-25 only to preserving the owning object after commit/cancel. Both behaviors matter, but the accepted Escape scenario must not disappear under the same ID. Restore it and bind the owner-selection case as an additional distinct assertion or separately named subcase, with exact test names, inputs, expected observations, commands and evidence paths.

## Finding 4 — projection and receipt binding is not frozen

`runtime/foundation/include/canvas/runtime/selection_summary.hpp:65`–`:81` carries one aggregate reason and `:106` carries only three property aggregate slots (field IDs 1, 2 and 3). The package requires eight property rows, pre-commit per-root exclusion reasons and post-command exact changed/skipped outcomes; it simultaneously forbids edits to `SelectionSummary`, `CanvasControlSnapshot`, `CanvasControlRequest`, the RuntimeFacade ABI, and the semantic validator. `CanvasControlReceipt` contains a general detail string but no frozen structured per-root outcome contract. `debug_ui/src/snapshot_assembler.cpp:81` obtains its selection data from `RuntimeFacade::readSelectionSummary()`.

An internal, ABI-preserving solution may be possible, but this P31 does not bind an actual data path, owner, source files, versioned representation, or consumer validation for these requirements. Reinterpreting generic error strings or deriving applicability via an unrelated UI kind switch would create an unreviewed protocol and would not satisfy the accepted D06-B rule. Resolve this as a narrow P31/verification binding, or return `AUTHORITY_CONFLICT` if the required safe path truly needs a prohibited public ABI or semantic change; this review does not assert that every ABI-preserving solution is impossible.

## Preflight observation

In the real isolated worktree and on the required package branch, PackageBindingPreflight, EvidenceContractPreflight, DesignPreflight, FunctionalScopePreflight and `--check all` all exited 0, in that order. Exact invocations and outputs are under `raw/`. These checks validate the package's stated identities and cardinalities; their success does not resolve the source/semantic discrepancies above. No product test, SDK rebuild, implementation, or physical-device validation was performed in this review.

## Smallest correction and next step

Materialize a new versioned P31 correction without rewriting v0.1: bind the real Facade and regression-test owners and their blobs; freeze the subset projection/receipt path and its ABI-preserving boundary; restore the accepted MSE-25 Escape case alongside the owner-selection case; bind the protected 160-ID change precisely and retain all unaffected negative tests. Then run the corrected package preflights and control review, and only after exact P32 authorization continue Phase 2 on a descendant of the D07 cursor. Keep the D07 OOM blocker separate and do not reopen Group/Section product design.

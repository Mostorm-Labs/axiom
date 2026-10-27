# FC-04 D07-A/B Targeted P31 — package preflight result

**P31_PREFLIGHT_PASS.** This is a control-package binding/design result, not a product implementation, D07 test pass, Gate, PHYS or release result. The user's new bounded instruction permits a P32 producer to be implemented after these preflights pass; historical v0.2 remains BLOCKED for its own previously frozen prerequisite and its evidence is immutable.

## Repository and authorization

Actual worktree `C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas`, repository `Mostorm-Labs/axiom`, branch `codex/fc04-d07-selection-audit`, clean starting HEAD `832866a2d001a247c4eb374c1fb1899258a6f6cf`. Feature anchor `c7f874395d55952e9d5288e2a336a21747f37f11` is an ancestor. Current FC00X-C D07 targeted acceptance and 07-14 integration were read again; connector response snapshots and edit metadata are under `authority/`.

The new P31 package at `.aegis/packages/FC04-D07-AB-P31-v0.1/` has a package identity (`package.json`), bounded closure (`closure.md`), actual source ownership/lifecycle (`design.md`), blocking oracle matrix (`test-matrix.json`), executable P31 checks (`preflight.py`), and SHA-256 source/package binding (`source-lock.json`). The result is bound to its containing commit; the final exact revision is returned separately.

This versioned package supersedes only the earlier correction task's *pre-existing producer* prerequisite for the newly authorized P32. The earlier P31 correction result is not changed or reclassified. The P32 positive test is planned and remains NOT_RUN until it invokes the real production producer. No future-phase test result is counted as existing evidence.

## Source and technical review

The real owners at the subject remain as described in the previous audit. `EditorHistory` stores one forward Operation per entry, and `CanvasEditingRuntime`'s legacy span port rejects multi-Operation requests before writing. `OperationEngine::apply`, `atomic_apply`, ledger, object index and `prepareContainerPlan` currently reserve/allocate per Operation, so they do **not** constitute a ready-made whole-intent no-fail commit. The new package names the existing owners and authorizes only the required private Semantic reservation support; the producer belongs in the existing Runtime/History owners.

The lifecycle is explicit `begin → prepare → commit` or pre-write `abort`, with staged sequential validation, real inverse/expectation capture, upfront capacity for canonical/History/result/Scene consequences, one History unit, exact-target Undo and fresh-ID Redo. It forbids implicit time/adjacency grouping, public ABI/wire changes and a new canonical transaction. Each Operation must retain its own canonical identity/generation/stamp/ChangeSet and eligible ordered local DataBridge publication.

**Implementation evidence is still essential.** The P31 checks do not prove a no-fail write segment, allocation closure, per-Operation notification timing, real producer behavior or actual injected failure behavior. P32 must prove them on the actual owners. If a recoverable failure actually occurs after a canonical prefix, it cannot be described as a pre-write abort; a forced failpoint may not be moved or hidden to pass. A demonstrated incompatibility between per-Operation publication and coherent local visibility, or an uncontainable post-prefix failure, is an explicit stop-and-return condition. The proposed route must not be represented as an established mechanism before those tests exist.

## Actual P31 checks

The supplied instruction had no bundled executable for these three checks. The package's `preflight.py` performs explicit, reproducible **P31 structural** checks against exact repository bytes, source owners, historical test inventories, accepted authority snapshots and the proposed test/closure bindings. It is not an official Gate evaluator or a product test runner. Full check-by-check JSON, commands, outputs and exit codes are in this result directory.

| Command | Result |
| --- | --- |
| `python -X utf8 .aegis/packages/FC04-D07-AB-P31-v0.1/preflight.py PackageBindingPreflight` | PASS, exit 0 |
| `python -X utf8 .aegis/packages/FC04-D07-AB-P31-v0.1/preflight.py EvidenceContractPreflight` | PASS, exit 0 |
| `python -X utf8 .aegis/packages/FC04-D07-AB-P31-v0.1/preflight.py DesignPreflight` | PASS, exit 0 |
| `python -X utf8 tools/check_runtime_abi_manifest.py --root .` | PASS, exit 0; 54 functions, 49 structs, 163 enum constants |

The actual executable is `D:/Softwares/Anaconda/python.exe`. `commands.json` and `raw/` retain exact invocations and output. All 40 bound source/evidence paths were compared to the exact starting Git revision and locked by SHA-256. No product source, test source, SDK lock, global Authority or registry was edited.

The preceding exact-source audit remains **historical evidence**: original D07 11 PASS / 3 FAIL, Selection 160 PASS / 0 FAIL. Those tests were not rerun in this P31-only turn; no new P32 positive exists yet. `FC04D07PostcommitReceiptGrowthFault` remains unresolved with previous controlled exit 86. Physical input, Chrome/WASM and product build were not run here. The planned P32 test matrix includes real producer, two independent commands/two Undo, corrected negative controls, actual forward and reservation fault boundaries, group Undo/Redo and the original OOM test, without skipping the 11/160.

## P32 handoff

Use the exact containing commit and package SHA-256 returned to the user, with branch and task anchor described in `package.json`. P32 may proceed only under this bounded package and must continue through both A and B until its closure passes or a specific terminal blocker occurs. No automatic main merge, D06-B closure or P34/PHYS claim is authorized.

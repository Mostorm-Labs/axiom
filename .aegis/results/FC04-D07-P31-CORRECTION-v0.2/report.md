# FC04 D07 P31 correction v0.2 — BLOCKED_VERIFICATION

The user-authorized P31-only check reached the package's explicit terminal blocker: **no real production explicit multi-Operation intent/group entry exists at the inspected source revision.** Product and test source files were not edited. No D07 implementation/Gate PASS, D06-B closure, or main merge is claimed.

Repository `Mostorm-Labs/axiom`; existing Selection worktree `C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas`; branch `codex/fc04-d07-selection-audit`; starting HEAD `a85a8077a58fce97c617badc7116794f6bc1a15b`. Starting tree was clean. Feature anchor `c7f874395d55952e9d5288e2a336a21747f37f11` and audit `e02fb7b8601bd01be968dbfb6aa783cfb3027e25` are ancestors. Source identity was reconciled without switching/resetting branches. This result is bound to its containing Git commit; exact final commit is returned separately to avoid self-referential hashes.

## Materialized correction

New directory `.aegis/packages/FC04-D07-P31-CORRECTION-v0.2/` preserves the supplied three documents byte-for-byte and contains `package.json`, `input-identity.json`, and `test-bindings.json`. ZIP SHA256: `cf6b7928c25604400318de8c9e42506944adcdd5223d07894e0d0ad6b92125aa`.

This is a **blocked binding candidate**, with `activate_for_product_repair=false`. It does not replace the old active executable assertions or activate product repair. The exact versioned mapping is:

| Obligation | v0.2 binding | Execution status |
| --- | --- | --- |
| Independent commands | Preserve `fc04.d07.D07.TwoIndependentCommandsNeedTwoUndoUnits` and all assertions | PASS in fresh native rerun |
| Naive sequential fallback | Historical `fc04.d07.D07.NaiveSequentialFallbackCannotSatisfyOneUndo_RED` is a negative-control counterexample, not grouped success acceptance; reuse the unchanged two-independent-command test as its executable characterization oracle | Historical assertion still FAILS in fresh replay; no failed product result relabeled PASS |
| Explicit grouped intent | Planned `fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit` requires real production owner, multi-Operation submit/completion, whole-intent Undo, fresh-ID Redo | NOT_RUN_MISSING_PRODUCTION_ENTRY; source/entry binding explicitly null |
| Old legacy span entry probe | `fc04.d07.D07.RequiredMultiIntentEntryIsUnavailable_RED` retained as historical gap probe | Still FAILS; never used as completed grouped commit proof |
| OOM | `FC04D07PostcommitReceiptGrowthFault` remains independently blocking | Still FAILS, controlled terminate exit 86 |

No new executable positive oracle was fabricated, no UI grouping flag or temporal/OperationId heuristic was introduced, and no existing test was weakened, renamed, skipped, or removed. Full old package and audit receipts remain immutable; all files enumerated by both historical manifests were rehashed successfully. The previous review bundle was opened and inspected against its existing archived contents; its SHA256 remains `53211a9792fb778ac5f90fb20717a16b1663259daddafcbad58afea6d186e215`.

## Exact missing owner

See `source-bindings.json` and the two raw source-search command records. Read-only inspection covered the production History owner, Runtime submit owner, command callers, public RuntimeFacade and CanvasRuntimeFacade, and Section's current command path.

- `runtime/interaction/include/canvas/interaction/editor_history.hpp:60`: `PreparedEntry` has one forward Operation. At line 72, `executeUndoable(const Operation&)` is a standalone command entry, with no explicit group identity/plan boundary.
- `runtime/interaction/src/editor_history.cpp:194`: execute one admitted Operation, commit one prepared History entry, publish. At line 232, the admitted Undo/Redo route requires `fresh.size()==1`.
- `runtime/interaction/src/canvas_editing_runtime.cpp:3347`: `submitAdmitted` accepts one Operation. At line 3439, the span compensation port rejects `operations.size()!=1` and all non-UndoRedo sources before mutation.
- `runtime/interaction/src/canvas_section_commands.cpp:453`: Section calls `history_.executeUndoable` once with one topology/content Operation; publication deferral here is not a multi-Operation HistoryGroup.
- Public `TextIntent` and control queues do not expose an arbitrary typed multi-Operation grouped intent. They are not substitute APIs for the requested positive oracle.

The missing production capability belongs in the existing **EditorHistory / CanvasEditingRuntime command-admission and publication owners**. The accepted bounded D07 requirement exists, but its requested grouped producer is not implemented. This is not an unresolved global Authority conflict, and not permission to implement the entry in this correction-only task.

The current FC00X-C authority was fetched again and includes the targeted accepted D07 supersession and link to acceptance page `4064c57a590c819aabc5e9efbf9a2b53`. The correction page `4064c57a590c81769767d04ec247bb7c` was also fetched; it explicitly requires stopping when the real API is absent. Full connector responses and edit metadata are preserved under `authority-inputs/`. No Notion or global registry writes occurred.

## Actual preflights and tests

The supplied ZIP has no executable preflight program. `preflight.py` is a task-local structural evaluation of its explicit requirements, not a new product runner, replacement oracle, Authority decision, or official Gate evaluator. Its outputs deliberately block missing real bindings. Exact commands, hashes, checks and outcomes are recorded in each JSON and under `raw/`.

Run from the actual Selection worktree:

```powershell
python -X utf8 .aegis/results/FC04-D07-P31-CORRECTION-v0.2/preflight.py PackageBindingPreflight
python -X utf8 .aegis/results/FC04-D07-P31-CORRECTION-v0.2/preflight.py EvidenceContractPreflight
python -X utf8 .aegis/results/FC04-D07-P31-CORRECTION-v0.2/preflight.py D07Preflight
```

The actual Python executable is `D:/Softwares/Anaconda/python.exe`. Each invocation returns exit **2**, `BLOCKED_VERIFICATION`:

- PackageBindingPreflight: repository/source ancestry, supplied bytes, current acceptance link, independent semantics, separated negative-control mapping, and blocking OOM mapping pass structural checks; **real explicit owner/source binding is missing**.
- EvidenceContractPreflight: preserved historical hashes, exact native artifacts, nonempty exact 14/160 test inventories and invocations pass structural checks; **positive real SUT invocation is unavailable**.
- D07Preflight: original 11 D07 passes and all 160 regressions retained; **explicit positive is unavailable and postcommit OOM remains a product failure**.

Raw CTest replay, using absolute `--test-dir`, absolute `--tests-from-file`, `--no-tests=error`, `--output-on-failure`, `--timeout 180`, and separate `--output-junit` files:

| Fresh execution | Exact IDs | Result | Exit |
| --- | --- | --- | --- |
| Original D07 native set | `raw/d07-names.txt` | 11 PASS, 3 FAIL, 0 skipped | 8 |
| Existing Selection native regressions | `raw/selection-names.txt` | 160 PASS, 0 FAIL, 0 skipped | 0 |

See `raw/d07-command.json`, `raw/selection-command.json`, their logs and XML for the complete commands/observations. The actual 14 and 160 executed IDs were compared to the exact frozen lists; zero matches cannot pass. Existing native executables were reused after checking their hashes against the accepted original source/build receipt (`sut-identity.json`); no build was needed or run. No Chrome/WASM, physical input, public host GUI, or product repair testing was performed in this task.

The OOM raw observation is retained unchanged in meaning: `before_generation=1 generation=2 ordinal=2 history=0 scene=1 tx=0 objects=3`, followed by controlled `TERMINATE_REACHED`. This is a genuine unresolved product defect, not a consequence of rebinding the naive negative control.

## Terminal handoff

Per supplied `02-codex-p31-correction-prompt.md`: “If no real explicit-intent API exists ... return BLOCKED_VERIFICATION ... Do not implement the API here.” Therefore **P31 correction is not passed; product repair must not start under this result**.

Return to P31/control with exact missing entry/source/completion binding. To proceed, provide an exact revision containing the real producer, or separately authorize its bounded implementation through the existing History/Runtime owners with a real positive conformance test. The current requirement cannot be met by changing standalone Undo semantics, treating span rejection as success, or relabeling a historical counterexample. This result neither demands a fresh product redesign nor claims that upstream semantic Authority must change.

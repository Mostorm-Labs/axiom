# FC04 D07-A/B P32 — partial implementation and blocker

**Terminal classification: NEW_HIGH_IMPACT_FAILURE_MODE.** Stop P32 and return to control review. This result is not D07-A/B PASS and is not suitable for merge. The containing commit is the materialized result revision; its parent is `c86fe624755794cf2fedd39fc46b2dc955ded1bd`.

The execution started with a clean `codex/fc04-d07-selection-audit` worktree, the package SHA-256 `b653d8847493f724c60c6921d2e807321174c89b8e310f272ccda5d789143da8`, result anchor `15e76458f3872be4d1b51d4aac05eb8402719014`, and feature anchor `c7f874395d55952e9d5288e2a336a21747f37f11`. All four v0.2 package preflights passed before modification.

## Partial work

- A real Scene prepare/abort path stages a full Scene, RenderScene, spatial index, damage and RuntimeScene projection before publication. A focused real Runtime test verified that a successful prepare and abort do not visibly alter its captured state. The full SCENE-PREP-01–06 matrix remains incomplete.
- The Runtime/History/Semantic path now has a preliminary explicit Intent producer. The tested SetTransforms plus PatchProperties example creates one Undo unit, undoes/redoes with fresh IDs, retains separate ordered commit records, and publishes a coherent Scene. The implementation currently fails closed for other Operation kinds; it does **not** yet cover the full authorized supported-kind and failure matrix.
- The previously fixed single-command OOM cases remain exercised. A new real post-canonical allocation-injection test initially exposed an MSVC allocation when moving the prepared SceneRecordStore value. The partial implementation now holds its prepared payload by unique ownership and commits by reference; the same test passes for the tested group.
- The qualification-hook configuration, complete fault-phase coverage, all six frozen Scene obligations, Selection 160, and Chrome/WASM have not been verified at this revision. D06-B remains open.

## Blocking observation

`FC04D07ExplicitIntentPrepareAllocationSweep` injects one real allocation failure at each successive allocation while invoking the production Runtime preparation. In two reproductions it completed attempts 0–100 with the checked canonical generation, ordinal, Scene generation, History size and target transform unchanged; attempt 101 then hung. The frozen test timed out after 15 seconds. The log is `raw/prepare-sweep-timeout.log` and the second reproduction's indices are `raw/sweep-stderr.log`.

The debugger stopped at the actual injected allocation at index 100, inside `google::protobuf::internal::ArenaStringPtr::Mutable`, called from nested `AssignDescriptorsOnceInnerCall` / Abseil `CallOnceImpl`, reached through `SemanticCodec::measureProtobufOperation` and the real `prepareUndoableCommand` (`raw/fault-index-100-stack.log`). A separate debugger attachment to the next hung preparation shows `absl::...::SpinLockWait` / `CallOnceImpl` / `google::protobuf::Reflection::GetTcParseTable` on the same F/U measurement path (`raw/prepare-sweep-hang-stack.log`). The locked SDK's `call_once.h` sets the control to running, invokes the callable and sets done only after it returns; the relevant source and SDK header hash are recorded in `raw/sdk-absl-call-once-excerpt.txt` and `source-lock.json`. The evidence supports an allocation exception escaping lazy descriptor initialization and leaving a later attempt waiting; it does not establish a safe in-scope remedy.

This is not merely an absent test or a recoverable precommit rejection: subsequent work can hang despite no canonical write. The path involves the locked published Semantic SDK and `runtime/semantic/src/codec.cpp`, which v0.2 did not authorize for modification. Do not replace the SDK, prewarm around the injection to claim success, weaken the oracle, or extend the source scope silently. Control/verification and the relevant Semantic SDK owner must decide a safe, bounded correction and verification. Preserve this partial work and the old OOM fix when resuming.

## Actual checks

Exact argv, exit codes and raw logs are in `commands.json`:

- Current D07 executable: 18 tests passed; the separately run OOM group adds the original two protected IDs, so all 14 original D07 IDs are present in the combined JUnit results.
- Two historical OOM tests and the new post-canonical no-allocation test: 3 passed.
- Existing Scene atomic publication and recovery: 2 passed.
- Runtime C ABI manifest: passed; the product/test source diff whitespace check passed. The complete staged diff reports trailing whitespace in three raw debugger/SDK excerpt lines; their bytes are retained as captured evidence.
- Precommit allocation sweep: **TIMEOUT / failed**, exit 8 via CTest; see the blocker evidence above.
- Frozen Selection 160 and actual Chrome/WASM: **NOT_RUN at this revision**. Historical results from `15e76458` are not current-revision passes.

The SDK was resolved through the published locked Semantic SDK and its offline check; no SDK, historical evidence, main branch or global authority was modified. Per-file current SHA-256 values, SDK header identity and all collected evidence are locked in `source-lock.json` and `manifest.json`.

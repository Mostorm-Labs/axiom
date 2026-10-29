# Codex — Correct FC-04 D07 P31 Test Bindings (No Product Code)

Repository: Mostorm-Labs/axiom
Feature baseline: `codex/selection-summary-batch3` @ `c7f874395d55952e9d5288e2a336a21747f37f11`
Audit branch: `codex/fc04-d07-selection-audit`
Preflight commit reported by executor: `a85a8077a58fce97c617badc7116794f6bc1a15b`
Prior D07 audit: `e02fb7b8601bd01be968dbfb6aa783cfb3027e25`

## Preflight
1. Verify repo identity, actual branch, HEAD, worktree state and ancestry. Do not switch/reset branches or overwrite work.
2. Read `.aegis/results/FC04-D07-REPAIR-PREFLIGHT/report.md` and `out/fc04-d07-repair-preflight/review-bundle.zip`.
3. Confirm exact assertions and invocation styles of both tests.
4. Confirm D07 acceptance is linked from the current authoritative record; otherwise stop as `AUTHORITY_CONFLICT`.
5. No production-source edits in this task.

## Required P31 correction
- Preserve `TwoIndependentCommandsNeedTwoUndoUnits` unchanged in meaning.
- Rebind `NaiveSequentialFallbackCannotSatisfyOneUndo_RED` as a negative control demonstrating that standalone sequential calls cannot meet grouped-intent semantics. Keep historical results immutable.
- Add a distinct positive oracle for the actual explicit intent/group entry point, e.g. `ExplicitIntentGroupProducesOneUndoUnit`.
- The positive oracle must call the real IntentPlan/UndoIntent/HistoryGroup owner.
- Preserve OOM and failure-containment requirements as blocking.
- Preserve 11 passing D07 checks and 160 Selection regressions; do not alter historical records.
- Do not add global canonical Transaction, new wire semantics, schema, OperationKind, FieldId, public ABI or global registry.
- D06-B remains separate and OPEN.

## Revalidate
Update only the versioned P31 test binding and closure mapping. Bind the real explicit-intent owner/source path and positive oracle. Record exact commands, IDs, outcomes, hashes and evidence paths. Run PackageBindingPreflight, EvidenceContractPreflight, package consistency checks and D07 preflight against the revised package. Do not mutate production source.

## Terminal success
Return a new versioned package/result revision, exact test-binding diff, proof independent-command semantics are unchanged, explicit grouped-intent positive mapping, preserved OOM obligations, successful preflights, commands/exit codes/raw outputs/hashes.

If no real explicit-intent API exists, or the frozen oracle cannot be corrected without upstream Verification Authority, return `BLOCKED_VERIFICATION` or the appropriate earliest-owner blocker. Do not implement the API here. Do not claim D07 implementation PASS; this task closes only the P31 contradiction.

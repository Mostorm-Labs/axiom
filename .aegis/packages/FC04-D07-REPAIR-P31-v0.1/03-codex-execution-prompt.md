# Codex — Execute FC-04 D07 Narrow Repair

Worktree: `C:/Users/Chado/.codex/worktrees/selection-unified-batch1/canvas`

Expected feature anchor: `c7f874395d55952e9d5288e2a336a21747f37f11`
Expected branch: `codex/fc04-d07-selection-audit`
Reported audit result: `e02fb7b8601bd01be968dbfb6aa783cfb3027e25`

## Step 0 — Preflight
1. Verify repository identity `Mostorm-Labs/axiom`.
2. Record actual branch, HEAD, worktree status, and ancestry.
3. Read `.aegis/results/FC04-D07-AUDIT/report.md` and `commands.json`.
4. Extract exact IDs, commands, targets, raw outputs, and source paths for the three failed D07 checks.
5. Verify the current authoritative D07 record links the accepted bounded scope. If not, stop before product-source mutation with `AUTHORITY_CONFLICT`.
6. Inspect the exact three failing tests and implementations. Preserve 11 passing D07 checks and all 160 Selection regressions.

## Implement only the three D07 failures
Fix cross-Operation coordination, one user-visible Undo unit, and post-commit memory-failure containment. Reuse existing IntentPlan / IntentCommitSegment / UndoIntent / HistoryGroup and typed Operation owners where they actually meet the requirement.

Before first canonical mutation, validate all forward operations and targets, construct inverses/preimages/expectations, and reserve HistoryGroup plus required result/publication memory. A failed intent must not expose partial local Scene/observer publication or leave Document changed without the matching History unit.

If the required guarantee conflicts with accepted 07-14 per-operation publication semantics, stop and report the precise Authority conflict. Do not invent a global canonical Transaction.

## Execute and verify
- Re-run the exact original three failed tests without weakening assertions.
- Run the 11 original passing D07 checks.
- Run all 160 Selection regressions.
- Add/run failure-injection tests for each forward-operation and History/result reservation boundary.
- Verify success creates one visible Undo unit.
- Verify Undo after selection changes restores original targets; Redo restores forward meaning with fresh IDs.
- Verify failures preserve Document, Scene/index publication, History cursor and redo branch.
- Verify allocation failure cannot strand canonical mutation without History.
- Run relevant builds and `git diff --check`.

Continue until all closure criteria pass or a precise blocker occurs. Do not return after one fix or one test suite.

## Forbidden
No D06-B implementation, multi-selection property-edit commands, Group/Ungroup, Snap/Alignment, schema/FieldId/OperationKind/public ABI/global registry changes, test weakening, or automatic merge to main.

## Return
Record exact base/head, changed paths, test IDs, commands/exit codes/raw logs, fault-injection evidence, hashes, and terminal status. Do not claim D07 PASS unless every required criterion passes. D06-B remains OPEN.

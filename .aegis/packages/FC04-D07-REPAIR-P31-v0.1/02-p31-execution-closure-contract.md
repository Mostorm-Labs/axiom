# P31 Execution Closure Contract — FC-04 D07 Repair

## Required changes
Fix only the three original D07 failure areas:
1. cross-Operation intent coordination;
2. one user-visible Undo unit per successful multi-Operation intent;
3. post-commit memory-failure containment.

Preserve all Selection Batch 1–3 work.

## Preflight before edits
- Verify repository identity `Mostorm-Labs/axiom`.
- Verify actual branch, HEAD, working tree, and ancestry from feature anchor `c7f874395d55952e9d5288e2a336a21747f37f11`.
- Read the exact local report.md and commands.json. Bind the three failed test IDs, exact commands, test targets, raw failures, and source paths into the execution record.
- Confirm the accepted D07 scope is linked from the current authoritative D07 record. If D07 remains OPEN without this bounded acceptance, stop before product-source mutation as `AUTHORITY_CONFLICT`.
- Do not infer failed test IDs from the summary.

## Allowed files
Only the existing local multi-Operation intent coordinator/IntentPlan/IntentCommitSegment/UndoIntent/HistoryGroup owner; EditorHistory/undoable command/submit owner if directly implicated; minimum result/publication reservation owner if directly implicated; and focused runtime/interaction tests. Record exact paths before edits. If the fix needs wider scope, stop and classify.

## Forbidden
No D06-B implementation; no Group/Ungroup; no multi-selection property-edit commands; no Snap/Alignment; no schema, FieldId, OperationKind, public ABI, or global registry changes; no global canonical Transaction; no weakened/deleted/skipped tests; no automatic merge to main.

## Required tests
- The exact original three failed D07 checks now PASS with unchanged assertions.
- All 11 original passing D07 checks remain PASS.
- All 160 Selection regressions remain PASS.
- Failure injection at every forward Operation and History/result reservation boundary.
- Successful multi-Operation intent produces exactly one visible Undo unit.
- Undo after changing selection restores original committed targets.
- Redo restores forward meaning with fresh Operation identities.
- Failure leaves Document, Scene/index publication, History cursor, and redo branch unchanged.
- Post-commit allocation failure cannot strand canonical mutation without History.
- Existing single-Operation multi-target SetTransforms/PatchProperties remains intact.

## Blocking evidence
Exact repository/branch/HEAD; original failed IDs and source/test mapping; commands, exit codes, raw logs; failure-injection evidence; before/after Document, Scene, History cursor and redo assertions; final revision/diff/hashes; reviewer-accessible terminal result.

## Terminal success
All required tests pass; 11 D07 and 160 Selection regressions remain green; no partial local publication or missing History unit is observable on failure; exactly one Undo unit exists on success; scope is respected; evidence is reviewable.

## Terminal blockers
`AUTHORITY_CONFLICT`, `MISSING_REQUIRED_INPUT`, `ENVIRONMENT_BLOCKER`, `FROZEN_VERIFICATION_FAILURE`, `NEW_HIGH_IMPACT_FAILURE_MODE`, `TASK_PACKAGE_DEFECT`, `EXECUTION_DIVERGENCE`.

Continue through all executable closure items until terminal success or a precise blocker. Do not stop after an intermediate fix/test pass.

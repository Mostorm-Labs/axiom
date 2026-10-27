# FC04 D07 repair P31 preflight — terminal blocker

**Status: TASK_PACKAGE_DEFECT. Product implementation did not start.**

Repository Mostorm-Labs/axiom, branch codex/fc04-d07-selection-audit, starting HEAD `e02fb7b8601bd01be968dbfb6aa783cfb3027e25`, feature anchor `c7f874395d55952e9d5288e2a336a21747f37f11`. Exact expected branch/HEAD, clean starting tree, ancestry, prior evidence manifest and SUT executable hashes were verified. The user ZIP SHA256 is `b90240ab9c9c41dcf451241ae90ac702572dcd4ba0820865d3ac1e7e1209bb9a`. All four original package documents are preserved byte-for-byte under .aegis/packages/FC04-D07-REPAIR-P31-v0.1/.

## Authority preflight succeeded

The current FC00X-C record now contains the targeted D07 acceptance and links the supplied user acceptance record. The former OPEN statement is superseded for the bounded local scope. This is **not** a failure to find the latest acceptance and is not classified AUTHORITY_CONFLICT. Live acceptance, P31, current D07 and07-14 fetches are preserved under authority-inputs/. No Authority or registry was edited.

## Exact frozen verification contradiction

The package requires all three original failures to become PASS with unchanged assertions AND all11 original passes to remain PASS. Two of those obligations demand different results for the same ungrouped production call sequence:

| Protected original PASS | Required-to-become-PASS original RED |
| --- | --- |
| D07.TwoIndependentCommandsNeedTwoUndoUnits | D07.NaiveSequentialFallbackCannotSatisfyOneUndo_RED |
| audit_test.cpp:402 | audit_test.cpp:440 |
| executeUndoable(move(580)); executeUndoable(color(581)); undo() | executeUndoable(move(610)); executeUndoable(color(611)); undo() |
| History size2; records != initial after one Undo | records == initial after one Undo |

Both use the same D07 fixture, same initial records/selection/history and the same payloads. The move/color helpers use the argument only for OperationId; transforms, property fields/values and targets are equivalent. Neither path establishes a coherent-intent boundary. Operation identities or test names cannot legitimately select opposite grouping semantics. Implicitly coalescing the independent commands to make the RED green breaks the protected PASS. Special-casing580/610 or test names would be a test bypass, not a repair.

The original report explicitly classified these two REDs as gap/negative-control demonstrations rather than future coordinator Gate tests. The new P31 freezes them as unchanged green obligations without rebinding an explicit intent entry. This is a verification/package binding defect, not missing input or a product implementation choice to guess.

The other entry probe, D07.RequiredMultiIntentEntryIsUnavailable_RED at audit_test.cpp:429, also checks accepted=true together with a full unchanged state snapshot through the legacy span port. It can describe admission only under an explicit queued lifecycle; it is not a proof of successful multi-operation commit/one Undo. The new producer and completion boundary must be identified before using it as that acceptance test. This is a binding limitation; the mutually exclusive Undo pair above is sufficient to block the frozen package.

## Original three failures and actual execution

1. fc04.d07.D07.RequiredMultiIntentEntryIsUnavailable_RED — axiom_fc04_d07_audit_tests, accepted=false versus required true.
2. fc04.d07.D07.NaiveSequentialFallbackCannotSatisfyOneUndo_RED — same target, one Undo leaves the earlier independent transform.
3. FC04D07PostcommitReceiptGrowthFault — axiom_fc04_d07_postcommit_fault receipt-growth, controlled postcommit allocation reaches terminate86 with generation2/ordinal2/objects3/History0/Scene1. This remains a genuine repair regression; its assertion need not be weakened.

The exact archived commands are in original-audit-commands.json. Exact original14 case IDs were extracted from the verified XML and rerun using the absolute tests-from-file list with --no-tests=error; this bounds the replay to the same cases. Current result:11 PASS,3 FAIL,0 skipped, CTest exit8. See raw/baseline-audit-command.json, raw/baseline-audit.log and raw/baseline-audit.xml. Full original raw failure observations and test bodies are in raw/original-failures.json and assertion-bindings.json. No failed result is relabeled as passing.

The160 Selection regression record is retained unchanged and hash-verified as historical evidence. It was not rerun during this blocked preflight; no product/test source bytes changed. No new repair/forward-boundary injection suite was begun because the package is already unsatisfiable as frozen.

## Minimal package correction for control review

See p31-amendment-proposal.md. Bind explicit coherent-intent producer/completion tests instead of forcing the independent-command negative controls green. Preserve all11 passing checks, the independent-command two-Undo meaning, legacy span fail-closed behavior, all160 regressions, accepted product scope and the real receipt-growth OOM regression. Add real forward/reservation fault cases after the new accepted entry is defined. Historical counterexamples remain unchanged evidence. This is not authorization to weaken expected behavior or change grouping by operation ID.

Per02-p31-execution-closure-contract.md and03-codex-execution-prompt.md, TASK_PACKAGE_DEFECT is a terminal blocker; the implementation skill likewise routes an inconsistent frozen verification target back to P31. No product-source mutation, partial OOM fix, public ABI/schema/registry change, D06-B closure, main merge or D07/Gate PASS is claimed. The bounded product requirement itself is accepted; only the test-entry binding needs correction.

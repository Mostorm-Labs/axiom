# Minimal P31 verification-binding amendment proposal

Status: PROPOSED_FOR_CONTROL_REVIEW, not applied to the supplied package or Authority. Accepted product scope, allowed production owners, D06-B OPEN, no public ABI/schema/global Transaction, all160 regressions and postcommit memory requirement stay unchanged.

Replace only the clause requiring all historical negative controls to turn green with unchanged calls:

1. Preserve D07.TwoIndependentCommandsNeedTwoUndoUnits and all other11 existing PASS checks. Two independent executeUndoable calls still require two Undo units. Keep the legacy unqualified multi-operation span refusal characterization.
2. Retain the two original gap/negative controls and their RED logs as immutable historical audit evidence. They are not the conformance runner for a new coordinator. Do not special-case IDs, silently coalesce independent commands or invert their exit codes into PASS.
3. Bind a real explicit coherent-intent production entry and a defined completion boundary for the replacement admission/success/one-Undo tests. Authorize only the narrow focused test-call/fixture rebinding needed to distinguish one grouped intent from two independent commands; preserve the strong expected Document/Scene/History/redo and one-Undo assertions.
4. The entry/success test must verify actual forward target results and exactly one History unit at completion, then Undo after changing selection, Redo with fresh IDs and monotonic metadata. If admission is queued, separately assert no mutation before processing and the actual completed result after the owner pump; accepted/unchanged alone is not completion proof.
5. Retain FC04D07PostcommitReceiptGrowthFault unchanged as a real product regression that must become PASS. Preserve all historical failing evidence and add failure injection for each permitted forward/reservation/publication boundary on the explicit entry.
6. State which local observer publication may be deferred while preserving each Operation's generation/stamp/ChangeSet/typed records under07-14. If a real incompatibility remains, obtain a targeted control reconciliation; do not invent a global canonical transaction.

Publish the corrected ID→entry→oracle mapping in a versioned P31 update. The executor can then resume the bounded repair from the same worktree/branch without repeating the historical audit or expanding into D06-B/Group/Snap.

# P32 Handoff — Functional D07

Use the exact package at the final materialization commit on branch `codex/fc04-d07-functional-p31-v0-3`. The current functional baseline cursor is `b5072f2c7dd796b2ad7aed3a6bfa5d86b6058cad`; task anchor is `15e76458f3872be4d1b51d4aac05eb8402719014`; feature anchor is `c7f874395d55952e9d5288e2a336a21747f37f11`.

Before any source edit:
1. Create/use a separate worktree at the package's declared worktree path, based on the new package branch; do not reset or overwrite the prior worktree.
2. Confirm origin is Mostorm-Labs/axiom, branch and ancestry, exact cursor, and diff. Preserve all valid partial Scene/Intent work; reconcile rather than replay it.
3. Run PackageBindingPreflight, EvidenceContractPreflight, DesignPreflight and FunctionalScopePreflight from `preflight.py --all`. Any failure stops P32.
4. Continue through the full functional closure contract until terminal success or an explicit terminal blocker.
5. Do not modify SDK/upstream files or locks. Keep the descriptor-init OOM hang as a separate overall D07 blocker; do not claim overall D07 PASS.

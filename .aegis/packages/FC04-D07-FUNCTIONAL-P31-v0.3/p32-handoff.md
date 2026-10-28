# P32 Handoff — Functional D07

Package materialization commit: `4a2bcd54b0a51761f4d6a4de2f0bbc2932eb88b4`.
Package: `.aegis/packages/FC04-D07-FUNCTIONAL-P31-v0.3/package.json` on branch `codex/fc04-d07-functional-p31-v0-3`.

The functional baseline cursor is `b5072f2c7dd796b2ad7aed3a6bfa5d86b6058cad`; task anchor is `15e76458f3872be4d1b51d4aac05eb8402719014`; feature anchor is `c7f874395d55952e9d5288e2a336a21747f37f11`.

Before any source edit:
1. Create/use the declared separate worktree at `C:/Users/Chado/.codex/worktrees/fc04-d07-functional-p31-v0-3/canvas`, based on the new package branch. Do not reset or overwrite the prior worktree.
2. Confirm origin is Mostorm-Labs/axiom, branch and ancestry, exact cursor, and source diff. Preserve all valid partial Scene/Intent work; reconcile rather than replay it.
3. Run these exact read-only preflights from the repository root:
   - `python .aegis/packages/FC04-D07-FUNCTIONAL-P31-v0.3/preflight.py --repo . --check PackageBindingPreflight`
   - `python .aegis/packages/FC04-D07-FUNCTIONAL-P31-v0.3/preflight.py --repo . --check EvidenceContractPreflight`
   - `python .aegis/packages/FC04-D07-FUNCTIONAL-P31-v0.3/preflight.py --repo . --check DesignPreflight`
   - `python .aegis/packages/FC04-D07-FUNCTIONAL-P31-v0.3/preflight.py --repo . --check FunctionalScopePreflight`
   - Then run the aggregate: `python .aegis/packages/FC04-D07-FUNCTIONAL-P31-v0.3/preflight.py --repo . --check all`.
4. Any preflight failure stops P32; preserve exact output. If all pass, continue through the complete frozen functional closure contract.
5. Do not modify SDK/upstream files or locks. Keep the descriptor-init OOM hang as a separate overall D07 blocker; do not claim overall D07 PASS.

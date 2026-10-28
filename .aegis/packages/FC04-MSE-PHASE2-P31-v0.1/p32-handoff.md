# P32 Handoff — FC-04 Phase 2 Multi-selection Editing

Package path: .aegis/packages/FC04-MSE-PHASE2-P31-v0.1/package.json
Branch: codex/fc04-mse-phase2-p31-v0-1-d07cursor
Task anchor: 2fee75f66a7f96f6aac834228ede372a9abda59a (ancestor)
Accepted resume cursor: 6bcf2d4bbf3314e5252951190c3c475403f52665 on codex/fc04-d07-functional-p31-v0-3.

Before edits:
1. Create a separate worktree on the package branch; do not modify or reset the D07 worktree.
2. Verify origin, branch, task-anchor ancestry, cursor ancestry, exact source locks and expected diff.
3. Run these commands and preserve raw output:
   python .aegis/packages/FC04-MSE-PHASE2-P31-v0.1/preflight.py --repo . --check PackageBindingPreflight
   python .aegis/packages/FC04-MSE-PHASE2-P31-v0.1/preflight.py --repo . --check EvidenceContractPreflight
   python .aegis/packages/FC04-MSE-PHASE2-P31-v0.1/preflight.py --repo . --check DesignPreflight
   python .aegis/packages/FC04-MSE-PHASE2-P31-v0.1/preflight.py --repo . --check FunctionalScopePreflight
   python .aegis/packages/FC04-MSE-PHASE2-P31-v0.1/preflight.py --repo . --check all
4. Do not edit product source until package control review accepts this exact P31 and all preflights pass.
5. Continue MSE-01..MSE-27 and inherited regression closure until PHASE2_FUNCTIONAL_SCOPE_COMPLETE_READY_FOR_CONTROL_REVIEW or an explicit blocker. Do not stop after the property matrix or one test slice.
6. Do not implement Group/Ungroup in this phase. Do not change public ABI, schema, registry, SDK or dependency locks. Preserve the known D07 OOM blocker.

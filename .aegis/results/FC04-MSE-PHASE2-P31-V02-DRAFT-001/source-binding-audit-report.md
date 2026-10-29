# Phase 2 projection source binding audit

Subject source revision: `6bcf2d4bbf3314e5252951190c3c475403f52665`. Execution branch before this evidence: `codex/fc04-mse-p31-v02-binding-fix` at `2bb486f22d5f124fa80f874deddbc490c1ce5b0e`.

The companion `projection-source-binding.md` records the complete proposed projection, displayed-token, queue, Interaction/History commit, receipt and Windows/Web host flow, with the exact owners and intended real-test bindings. Its proposal is not a reviewed or approved implementation contract. `projection-source-lock.json` records Git blob and SHA-256 identities for 13 audited subject files.

Commands actually run in the worktree:

```text
python .aegis/results/FC04-MSE-PHASE2-P31-V02-DRAFT-001/source-audit.py
exit 0
PASS source audit: 13 exact-cursor paths; semantic-generation queue gap confirmed; product tests NOT_RUN

python .aegis/packages/FC04-MSE-PHASE2-P31-v0.2/preflight.py --repo . --check all
exit 2
PackageBindingPreflight: PASS, HEAD=2bb486f22d5f124fa80f874deddbc490c1ce5b0e
EvidenceContractPreflight: PASS; implementation tests NOT_RUN
DesignPreflight: BLOCKED_PROJECTION_BINDING
```

The all-check stops at DesignPreflight; it does not run FunctionalScopePreflight in that invocation. The previous individual preflight record in `commands.json` separately recorded FunctionalScopePreflight exit 0 at the earlier package revision. None of these are product behavior tests.

The remaining design decision is concrete: whether the Debug UI C++ context/header and its consumers can change within the accepted ABI restriction, or which ABI-preserving injection mechanism supplies the feature port; then bind the selected bridge, typed internal queue and receipt, all changed source files, tests and independent review in a new package revision. The source audit proves the listed markers and exact bytes only; it does not establish the safety or implementation of the proposed design. Until this is resolved, P32 authorization remains false. The previously recorded D07 functional finding and separate OOM blocker are unchanged.

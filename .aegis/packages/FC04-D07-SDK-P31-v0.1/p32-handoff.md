# P32 Handoff — FC04 D07 Route A SDK Exception-Safety v0.1

Package path: .aegis/packages/FC04-D07-SDK-P31-v0.1/
Package commit is the exact package_ref supplied in the control response after materialization.
Execution branch: codex/fc04-d07-selection-audit
Task anchor: 3d15645d54d44c47474b4b0dd6ca20ade1e3c633
Feature anchor: c7f874395d55952e9d5288e2a336a21747f37f11
Incident: 3615a38422a103024bc23924c570e3c585656670

Before P32 begins, run:
1. python .aegis/packages/FC04-D07-SDK-P31-v0.1/preflight.py --package-binding
2. python .aegis/packages/FC04-D07-SDK-P31-v0.1/preflight.py --evidence-contract
3. python .aegis/packages/FC04-D07-SDK-P31-v0.1/preflight.py --design
4. python .aegis/packages/FC04-D07-SDK-P31-v0.1/preflight.py --all

P32 may begin only if all checks pass on the exact package ref and the worktree is clean.

P32 may create the bounded versioned Abseil/Protobuf source patches, apply them through the existing Semantic SDK producer, include patch-set digest in runtime identity, and add/run exception-safety and producer tests. Do not publish an immutable SDK Release Set, update semantic-sdk.lock.json, replace an installed SDK, or modify Axiom Runtime codec/schema. Return to control with a qualified candidate. Release promotion and lock update require a separate accepted release step.

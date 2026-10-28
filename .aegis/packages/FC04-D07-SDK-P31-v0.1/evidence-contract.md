# Evidence Contract — Route A SDK Exception Safety

## Before implementation
Verify repository identity, branch, HEAD, clean/dirty state and ancestry. Read the immutable Route A report/decision, P32-002 report, source lock and debugger evidence. Run package-binding, evidence-contract, design and aggregate preflights.

## Required evidence
- Reproduce the real allocation failure using locked source inputs.
- First-call controlled failure and second-call liveness for the same descriptor.
- At least two concurrent waiters terminate without deadlock.
- Descriptor/registration state before and after each injected failure proves safe retry or a supported stable failure state.
- Byte-identical measure/encode output versus no-fault control.
- Full allocation sweep without timeout/deadlock or canonical/Scene/History mutation.
- SDK unit tests, relevant upstream Abseil/Protobuf tests, patch application, producer reproducibility, SDK smoke/consumer qualification and required producer CI matrix.
- Current-revision D07 and all 160 Selection regressions; do not substitute historical passes.
- Patch-set identity proves unpatched cached assets cannot be reused.
- Exact source revision, commands, exit codes, raw logs, test IDs, environment, hashes, package identity and terminal receipt.

No immutable SDK release publication, semantic-sdk.lock.json update or installed SDK replacement during P32. After qualification, return to control. Release promotion must use the existing formal workflow and lock-only candidate PR.

Failure classes: AUTHORITY_CONFLICT, MISSING_REQUIRED_INPUT, ENVIRONMENT_BLOCKER, FROZEN_VERIFICATION_FAILURE, NEW_HIGH_IMPACT_FAILURE_MODE, TASK_PACKAGE_DEFECT, EXECUTION_DIVERGENCE, NO_SAFE_BOUNDED_REMEDY.

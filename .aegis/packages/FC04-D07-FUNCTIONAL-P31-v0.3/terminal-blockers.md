# Terminal Blockers — Functional D07 P31 v0.3

- AUTHORITY_CONFLICT: actual 07-14 consumer requires intermediate Scene visibility incompatible with accepted coherent local-intent contract.
- TASK_PACKAGE_DEFECT: required source owner/path is outside the explicit allowlist or a required verification oracle is missing from the frozen package.
- MISSING_REQUIRED_INPUT: exact baseline, source locks, test ID lists, SDK facts or authority input is absent/mismatched.
- ENVIRONMENT_BLOCKER: required Windows native build or applicable actual Chrome/WASM environment cannot run.
- FROZEN_VERIFICATION_FAILURE: any required functional test fails, skips, does not discover exact IDs, or reports partial mutation.
- NEW_HIGH_IMPACT_FAILURE_MODE: canonical/Scene/History divergence, unsafe failure after mutation, deadlock, or other material defect not safely contained in this scope.
- EXECUTION_DIVERGENCE: branch, cursor, ancestry, source lock, package identity or actual worktree state disagrees with the package.
- BLOCKED_KNOWN_HIGH_IMPACT_OOM_HANG: overall D07/release remains blocked by the preserved Protobuf/Abseil descriptor-initialization fault/hang; this package does not repair it.

Do not retry the old descriptor OOM sweep in the same poisoned process. Do not declare overall D07 PASS, merge or release readiness under this package.

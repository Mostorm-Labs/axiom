# Terminal blockers

- AUTHORITY_CONFLICT: a required upstream owner/path is outside scope, or exception-safe descriptor recovery conflicts with upstream invariants.
- MISSING_REQUIRED_INPUT: locked source archives, provenance, incident evidence or producer authority cannot be verified.
- ENVIRONMENT_BLOCKER: required source builds, SDK tests or producer platform cells cannot execute.
- FROZEN_VERIFICATION_FAILURE: any OOM, waiter liveness, descriptor integrity, byte equivalence, producer, D07 or Selection test fails.
- NEW_HIGH_IMPACT_FAILURE_MODE: deadlock, partial descriptor registration, duplicate registration, inconsistent bytes or unrecoverable Runtime state.
- TASK_PACKAGE_DEFECT: patch identity, source lock, package binding, producer recipe or test contract is inconsistent.
- EXECUTION_DIVERGENCE: scope expands, locked SDK is changed ad hoc, or historical evidence is rewritten.
- NO_SAFE_BOUNDED_REMEDY: no patch provides safe retry or supported stable failure without broader redesign.

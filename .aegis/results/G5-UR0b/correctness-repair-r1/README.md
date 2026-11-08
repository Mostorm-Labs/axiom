# G5-UR0b correctness repair r1 — TASK_PACKAGE_DEFECT

P31 has been frozen from the exact P20 work order at the reconciled df214581
cursor. Repository/binding preflight succeeds. Full proof-path design preflight
finds a scope omission before source edits: the frozen work order requires an
additional exact repaired worker profile, while Python require_canonical_chain
compares source_profile to the old two-hash dictionary and is outside scope.

profile-preflight.py accepts the real archived RT120 proof, then shows that
profile-only changes are rejected at that exact guard. The two negative inputs
are explicitly counterfactual unit inputs, not new sampled profiles or recordings.
No worker was executed and no archive was edited. See design-preflight.json for
the call path, reader line/hash and minimum scope correction.

The existing 13 initialization Node tests and 11 Python reader tests pass as
preflight controls. The 12 repair oracle groups, 17 recompilations and full suites
were not run in this blocked implementation attempt. The preceding checkpoint
validation had 2 allowed malformed-snapshot native checks; this continuation has
0 native calls, 0 browser workers, 0 builds and no commit/push. Historical sampling
stays 33 starts / 1251 seconds. All old windows and verdicts remain unchanged.

Return to control for exact source-scope reconciliation. This is neither completed
harness repair nor readiness for new sampling, and contains no Gate/P34 PASS.

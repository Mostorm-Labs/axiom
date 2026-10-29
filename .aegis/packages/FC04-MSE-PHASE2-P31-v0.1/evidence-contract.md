# Phase 2 Evidence Contract

Blocking evidence:
- Exact branch, task anchor, cursor and all package preflight outputs.
- Completed property-by-kind matrix with read/applicability/capability/write/History/test evidence for all eight property rows.
- MSE-01..MSE-27 individually discoverable with exact IDs, expected results, no skips, no zero-match-as-pass.
- Original D07 14, Selection 160, expanded D07 30, targeted fault 46 and Scene 2 regression suites at the exact source revision.
- Per-target eligibility, skipped reasons, changed outcomes, pre/post Document and Scene generations, History cursor/size and Redo branch.
- Failure injection at capability query, payload validation, Operation preparation, History reservation and Scene publication precommit boundaries; no failpoint moved past canonical mutation.
- Undo after selection change; Redo with fresh IDs; one user intent = one Undo unit; two independent commands = two Undo units.
- Actual-hit TextEdit and Escape/IME precedence tests.
- Windows native build/test and actual Chrome/WASM automated interaction where supported.
- Runtime C ABI manifest unchanged; no wire/schema/registry/lock changes; exact source diff and raw logs.
- Reviewer-accessible result report, manifest hashes, JUnit/test ID lists, commands and exit codes.

Corroborative evidence:
- Prior successful tests from different revisions, code inspection without execution, screenshots without a bound exact revision.

Separate overall blocker:
- The real Protobuf/Abseil descriptor-initialization OOM hang remains an overall D07/release blocker. Do not hide, rerun in the poisoned process, weaken or reclassify it as pass. This Phase 2 package does not repair it.

Browser qualification is automated mouse/CDP coverage only; physical touch/stylus/IME acceptance is not claimed in this phase.

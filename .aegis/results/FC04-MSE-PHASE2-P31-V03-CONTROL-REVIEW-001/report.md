# FC-04 MSE P31 v0.3 — preparer self-review

Result: `BLOCKED_VERIFICATION_BINDING`; independent control review and P32 authorization are not complete. Package subject: `7425e79e559b6d33b519995fe373afaa1d578883`, package SHA-256 `3a886463173245391f4e4d71e93a8f5472aec4515c9418f06e251d22d34fc3e5`. Repository: `Mostorm-Labs/axiom`; execution branch: `codex/fc04-mse-p31-v02-binding-fix`. The same assistant previously prepared v0.3 and performed this inspection: this is a self-review, not independent acceptance or a P34/Gate result.

## Results and evidence

- The published branch resolves to `78f57ff5725b253884feb5b10d00e4fc31745406`, a descendant containing the exact package; local working tree was clean at inspection. The product trees `runtime`, `debug_ui`, `apps`, and `schema` have no diff from the accepted D07 source cursor `6bcf2d4bbf3314e5252951190c3c475403f52665` at the package revision.
- Re-running `python .aegis/packages/FC04-MSE-PHASE2-P31-v0.3/preflight.py --repo . --check all` exited 0 at `78f57ff`; stdout is recorded in `observations.json`. These checks verify the specific mechanics implemented in the script, not the completeness or independence of verification or product behavior.
- D06-B Current Authority is [P23 acceptance](https://app.notion.com/p/4074c57a590c81d9b124cd243e44f205). It requires exact test IDs, source paths, commands, expected outputs and evidence artifact locations to be frozen by the successor P31 before implementation.
- ADR-0022 identifies the product stable boundary as C ABI; `canvas_debug_ui` is a static target whose include path is CMake PUBLIC. The chosen internally rebuilt C++ port is a plausible approach; the review does not establish that all potential independently distributed Debug UI C++ consumers have been enumerated or certify compatibility.

## Blocking finding F1 — 27 scenario labels lack frozen executable/oracle bindings

`test-matrix.json` has 27 entries, each with only `id`, `blocking`, and `case`. It does not bind, for each MSE case, a real test selector/source path, fixture or setup, explicit expected observation/oracle, execution command, or evidence location. Some exact output requirements are described generally in `design.md` and inherited P31 closure, but they do not provide the per-case identities and observations required by the Current D06-B decision. The tests themselves may be created during P32; the missing item is the **prior binding of what they will run and prove**, not a demand that the implementation tests already pass.

This matters especially for MSE-07/19/20/21/23/26/27: merely reporting a count of 27 scenario IDs can miss an incorrect eligible cohort, partial mutation or unknown-provider-as-unsupported result. `EvidenceContractPreflight` checks the ordered IDs, historical case counts and two keywords in MSE-25; it does not validate those missing per-case bindings. Its exit 0 must not be interpreted as their completion. The correct classification is `VERIFICATION_DESIGN_DEFECT` / `TASK_PACKAGE_DEFECT`, routed to the owning verification/P31 stage.

## Blocking finding F2 — executable test discovery route not frozen

The allowlist proposes a new `verification/fc04_mse/CMakeLists.txt`, but at the subject the root `CMakeLists.txt` does not add `verification/fc04_mse`, and root `CMakeLists.txt` is not an authorized modification. A route via an already allowed `runtime/composition/CMakeLists.txt` or another existing owner may be viable, but v0.3 does not select or test an exact route. `verification/fc04_mse/CMakeLists.txt` does not exist yet, which is not itself a failure; the risk is that a future test target can be written without being configured/discovered, while IDs and zero tests look superficially complete. The source path, CTest/Chrome selector, nonzero discovery, expected case mapping and actual build entry must be specified in the successor package. This is a `TASK_PACKAGE_DEFECT`, not proof that such a target cannot be implemented.

## Binding weakness F3 — preflight checks locked cursor, not current source bytes

`preflight.py` reads each locked source with `git show CURSOR:path` and compares the cursor blob/hash to the lock. It does not compare those source/input files at the running HEAD and worktree against the cursor bytes. The exact inspected package and current checkout do have the same product tree, so no current source mismatch was observed. The weakness is prospective: the same branch can acquire source edits and still produce a green source-lock check, or worktree evidence can be changed while the script checks only the historical input blob. An executable successor must distinguish initial clean baseline validation from legitimate later P32 changes and verify actual frozen inputs, not equate an ancestor blob check with the executing file.

## Disposition

Do not issue P32 authorization against `7425e79e`. Freeze per-case test/expected/command/evidence bindings and a discoverable runner entry in a new versioned P31 package, strengthen its baseline/input binding, run its real-worktree preflights, and obtain the required independent control review. No product sources or historical packages/evidence were modified by this self-review. D07's separate Protobuf/Abseil OOM hang remains an overall merge/release blocker; this result does not reopen or waive D07.

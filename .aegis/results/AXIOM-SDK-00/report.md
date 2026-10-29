# SDK-00 source and contract materialization — REVIEW CANDIDATE

`Mostorm-Labs/axiom` origin was fetched; `origin/main` and starting revision were both `50e53124ffe7ed2959d6bcf956c3d252fce6451a`. FC05 `e5e630c5662167763fc7ba66b94c35881b1e1470` is an ancestor. There was no delta after the plan baseline. The user-specified plan and independent review were read without modification; their byte hashes are in `facts.json`. This control worktree was newly created and was clean at its exact base.

SDK-00 delivers the per-symbol V1 inventory, source/Authority decision comparison, specific proposed public handles/target/control/multi-selection APIs, owner and lifetime/error analysis, verification obligations, platform sequencing and six-file SDK-01 package. No product source, canonical ABI manifest, SDK lock, old Gate or historical evidence changed. The package is a **DRAFT**: no one has accepted its ABI extension, thread/lifecycle, exact verification or DataBridge decisions, and `p32_authorized=false`. Content-addressed draft consistency is not Authority or EvidenceContractPreflight acceptance. Do not render an executable P32 handoff from it.

Actual static commands at the exact baseline:

- `python tools/check_runtime_abi_manifest.py --root .`: exit 0; 54 function declarations, 49 structs, 163 enum constants. No SDK linkage demonstrated.
- `python tools/check_runtime_boundaries.py --root .`: exit 0; 103 contract files. Not an SDK installed-header audit.
- `ctest --test-dir <existing fc04 build> -N -R <12-name regex>`: exit 0, all 12 names discovered, but four executables were reported missing. No test execution was performed; none of these discovery entries is presented as passing.

The original allocation sweep timeout, open MSE-20/23/26 and Group physical pending are retained in `docs/planning/sdk/README.md`. Their presence does not prevent this draft, but no cross-operation OOM containment, release readiness or physical success is claimed. Proposed API01–API12 obligations are not frozen Verification Authority. The DataBridge and related ICF mirrors say Current Proposed Freeze Candidate, which is not accepted implementation authority. These are the precise next control/verification decisions; SDK-01 P32 must not start on this draft.

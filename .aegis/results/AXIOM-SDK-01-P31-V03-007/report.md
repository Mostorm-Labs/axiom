# SDK-01 P31 v0.3 binding candidate — 007

Status: DRAFT, no P32 authorization. Baseline `ff34c2c4fe58be0c1e7bf75f954b0972a760db02`. Exact package and input SHA-256s are in `facts.json` and the package locks. Earlier package and results are retained unchanged.

## Materialized

The package locks the ABI candidate, per-root style-preserving scenario, current Runtime and host sources, prior authority and verification locks, and evidence to exact repository revision and hashes. P01–P10 distinguish partial eligibility, permission/staleness, exact commit and history, mixed stroke leaf preservation, external installed-library consumers, native host, actual Chrome/WASM host, layouts and existing regressions. These are targeted draft bindings; retained API01–API12 obligations still apply. The two public consumer scenario sources are compile-only and are not complete oracles. Windows and browser driver sources are existing hosts, not implemented test drivers. Required public fixture setup, independent document/Scene/History observations and actual platform driver commands remain missing and must not be treated as executable acceptance.

## Preflight observations

Run `python .aegis/results/AXIOM-SDK-01-P31-V03-007/run-preflights.py`; the exact invocations, output, interpreter and exit codes are recorded in `raw/preflights.json`. Integrity exit 0; PackageBinding, EvidenceContract, Design and all exit 2. The first reports missing accepted exact ABI and execution authorization; the second reports incomplete/unaccepted real SDK and Windows/Chrome drivers and oracles; the third reports unaccepted ABI, lifecycle and per-root owner path. A deliberate temporary change to this new package's context was detected (exit 2); bytes were restored and integrity rerun successfully (exit 0). This verifies one checker failure path, not that all preflight failure modes were tested.

No new product, Windows, Chrome or physical tests ran in this package task. Previously observed owner and compile-only evidence is available under `AXIOM-SDK-01-ABI-CANDIDATE-006`; its limitations continue to apply. D07 OOM, MSE-20/23/26 and physical status remain unchanged.

## Return

Control/Verification must resolve the candidate ABI and lifecycle, materialize and bind independent real-consumer/host drivers and hosted evidence boundary, and accept an exact corrected package. Do not treat this draft as a P32 handoff or official Gate pass.

# SDK-01 v0.2 implementation context — draft, not authorized

Source and task anchor: `f10a5adce2c54ab87378feba0c7d4709e4273747`. The user chose stale rejection, refresh and explicit retry; `docs/planning/sdk/decision-stale-target-v0.1.md` is the narrow decision record. Prior test failure is retained in `.aegis/results/AXIOM-SDK-01-REVIEW-002/`; real-owner regression now passes as recorded in `.aegis/results/AXIOM-SDK-01-STALE-DECISION-003/`. Existing Runtime source was not changed to resolve that test conflict.

The external C/C++ consumer fixture and runner are real-library-only, but cannot execute until an SDK is installed; this is not an SDK pass. `verification/sdk01/case-bindings-v0.2.json` binds partial oracles and identifies missing public/Windows/Web drivers. Both current consumers still use the internal multi-selection port. Preserve unknown-permission refusal, exact target/stamp, eligible/excluded and receipt semantics; implement a validated equivalent public path and migrate consumers before hiding the internal port.

Do not start SDK-01 P32 from this draft. ABI type and threading decisions, executable public multi-selection tests and P20/evidence acceptance remain unresolved. D07 OOM, MSE-20/23/26 and physical test status remain open; see source reports.

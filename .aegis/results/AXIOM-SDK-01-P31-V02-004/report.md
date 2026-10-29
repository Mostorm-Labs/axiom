# SDK-01 P31 candidate v0.2 — partial, not P32 authorized

`AXIOM-SDK-01-v0.2` is a new, immutable-by-history candidate anchored to `f10a5adce2c54ab87378feba0c7d4709e4273747`. It binds the user's explicit stale-request rejection/refresh/retry decision, the 13/13 existing-owner tests, candidate external C/C++ consumer and concrete but incomplete multi-selection case mappings. The prior package and all historical failures are unchanged. Package SHA-256 and exact command outputs are in `facts.json` and `preflight.json`.

The package's local preflight was run against the real worktree with `--repo .`: `integrity` returned 0 for identity, ancestry and locked bytes only; `PackageBindingPreflight`, `EvidenceContractPreflight`, `DesignPreflight`, and `all` each returned 2 with explicit blockers. These are checks of this draft package, not a Control Review, accepted P20, P31 PASS or authorization.

Remaining blockers:

1. Concrete public ABI types, handle/target validity, per-API thread and callback lifetime, and multi-selection permission/target/receipt contract still need accepted binding.
2. The external consumer currently checks only V1 ABI and two-runtime lifetime. Exact public multi-selection/edit/full-ID oracles and the real Windows/Web migration drivers are not implemented or accepted. A library is not yet installed, so even the two smoke cases remain NOT_RUN.
3. Hosted evidence requirements and P20 acceptance have not been frozen. `p32_authorized=false` remains intentional; P32 cannot start from this candidate.

MSE-20/23/26 missing real provider/fault coverage and the historical D07 Protobuf/Abseil OOM hang remain separate recorded limitations. No physical, SDK product, Gate or release pass is asserted.

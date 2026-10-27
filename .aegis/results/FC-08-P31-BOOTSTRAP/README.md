# FC-08 P31 verification bootstrap — BLOCKED_VERIFICATION

This is the bounded bootstrap return for exact product package
`f67b9db29090449799bd3e05ef7f1d269dab6d31` and unmodified production subject
`427580ae13b60a39e88959b282cbffc695c9aabb`.
The active evidence is `attempts/bootstrap-002/output-index.json`.

Both product package preflights were actually invoked inside this worktree
with `--repo .` and the exact package revision. Both exited **2** and reported
`BLOCKED_VERIFICATION`; byte bindings/repository ancestry match, but executable
verification bindings do not. See `preflight-invocations.json` and both raw
stdout/stderr logs. The checker unit tests pass; they are **not Axiom product
tests** and their TDD RED is **not FC08-RED-01**.

## Required control decisions

- **BOOT-VER-01 / P20 Verification:** Frozen reference geometry declares B an
  ellipse while the same input's canonical Shape record uses `shape_kind=1`.
  Released registry V1 defines1=RECTANGLE and2=ELLIPSE. In particular, both
  FC08-S02 variants are affected. The preflight lists every matching affected
  input and its exact input/expected hash and JSON pointers. A runner must not
  silently overwrite canonical records from helper geometry or claim this is
  an independently valid ellipse fixture. Review the intended truth and any
  necessary exact input/lock delta in P20. Current inputs remain unchanged.
- **BOOT-PKG-02 / P31:** The product lock directs exact headless applicability
  to the frozen variant-index/corpus manifest. Those files contain no such
  per-variant map. A global list of three profiles and old baseline statuses
  do not establish the accepted product subset. Bind it explicitly in P31;
  route to P20 if applicability itself needs a verification decision.

The missing product runner is an authorized repair target, **not by itself a
reason to stop**. The stop is the unresolved frozen verification truth above.
No product runner stub was created. FC08-RED-01 is explicitly NOT_RUN, with
all seven required representatives and profile-specific reasons recorded.
No prior capability summary, descriptor inspection, qualification hook or
synthetic trace is relabeled as a new product RED observation.

## Preserved boundaries and findings

ImplementationDesignPreflight records exact existing owner/source hashes and
line anchors: public Facade, Composition ingress/owner pump, Semantic
prepare/atomic writes/ledger/clock, actual History prepare/commit, Scene
prepare/publication/recovery, preview handoff, Native/Web host and headless
bootstrap/replay. The accepted future F/U/reservation and handoff sequence is
cited from P13/P15; its absence in the baseline is not filled in by tests.
Existing owners are source-bound, but overall design preflight remains blocked
by verification conflicts. No product behavior has been proved or implemented.

No runtime, schema, registry, descriptor lock, CMake, SDK lock, old results,
frozen inputs/expected/oracles or qualification semantics were changed.
No product Windows/Chrome/WASM/headless build/run was performed after the
blocking preflights; this is not an environment PASS or an environment-failure
claim. No P32, P34/DEV/PHYS PASS, release readiness or main merge is authorized.

Reproduce a blocked preflight (exit2 is expected):

```powershell
python tools/fc08/bootstrap_preflight.py --repo . --package-revision f67b9db29090449799bd3e05ef7f1d269dab6d31 --phase package --report out/bootstrap-new-package.json
python tools/fc08/bootstrap_preflight.py --repo . --package-revision f67b9db29090449799bd3e05ef7f1d269dab6d31 --phase evidence --report out/bootstrap-new-evidence.json
python -m unittest discover -s tests/fc08 -p bootstrap_preflight_test.py -v
```

Return to P20/P31 control review. Resume with exact accepted successor bindings;
then establish the actual runner and FC08-RED-01 before separate P32 review.

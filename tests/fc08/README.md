# FC08 verification materialization v0.2

The current successor uses exact package
`a0883474790fb5eb70c49c96907caef80fa6b2ab`. Review the results in
`.aegis/results/FC-08-VERIFY-MATERIALIZE/attempts/v0.2-obs01/` and the current
`.aegis/materialized/FC-08/executable-verification.json`.

This work adds default-OFF private OBS-01 instrumentation and verification
hosts/corpora. It does **not** implement Section, operations17/18 or container
Facade APIs. Future product records explicitly remain CAPABILITY_ABSENT or
NOT_REACHED_BASELINE, including H06's admitted-plan boundary. Actual inherited
codec/import/replay/owner regressions are reported separately. No Gate, DEV or
PHYS PASS is issued; the product package candidate is unauthorized.

Run the fixed entrypoint in a fresh output directory:

```powershell
python tools/fc08/run_verification.py --design .aegis/materialized/FC-08/executable-verification.json --case all --profile windows-native --output out/fc08/new-native-run
python tools/fc08/run_verification.py --design .aegis/materialized/FC-08/executable-verification.json --case all --profile web-wasm --output out/fc08/new-browser-run
python tools/fc08/run_verification.py --design .aegis/materialized/FC-08/executable-verification.json --case all --profile headless --output out/fc08/new-headless-run
python tools/fc08/check_materialization.py --repo . --attempt v0.2-obs01
```

`discover` enumerates exact IDs. `--case FC08-T01` selects every variant of that
logical case. Exit0 collects materialization observations; inspect each row's
product_status. An old reader's unknown-field rejection never qualifies a
future product negative. The WASM runner launches real Chrome; Node is only a
loopback/CDP transport. Headless replay creates no EditorHistory owner.

Qualification builds require BUILD_TESTING, AXIOM_BUILD_FC08_VERIFICATION and
AXIOM_FC08_QUALIFICATION_HOOKS. The last option defaults OFF. Exact build
invocations, locked SDK facts, compiler definition audits, actual binary hashes,
raw probes, independent controls and original failures are retained in the
successor attempt. Compiler/SDK installation paths in recorded invocations
describe this machine and must be resolved locally on another machine.

Earlier corpus attempt directories are authoring history, not accepted goldens.
Only the manifest selected by executable-verification.json is the current
candidate. The historical v0.1 record below is preserved, not retroactively
changed to success.

## Historical v0.1 terminal blocked result

This is verification-only work from exact package
`db21441a1e978ea30fcfb4185691dadbb6f81f78`. It does not implement FC08.

The authoritative execution return is
`.aegis/results/FC-08-VERIFY-MATERIALIZE/terminal-result.json`.
Status is **BLOCKED_EVIDENCE / TASK_PACKAGE_DEFECT (OBS-01)**. No DEV/P34/PHYS
or complete-corpus claim is made. The product candidate remains unauthorized.

Verified outputs include accepted technical-source exports, source-complete
isolated BASE-01 and proposed descriptors, unchanged seed bytes, independent
non-dyadic arithmetic tables, fixed R02 controls, and actual old-baseline raw
ingress observations on Windows. Source and environment identities are saved.

`reference/variants.py` is an expansion inventory. `corpus/variants/` and
`wire-variant-index.partial.json` are **unreviewed M02 drafts**, not accepted
goldens or complete stateful tests. Their exact outstanding review issues are
in `.aegis/materialized/FC-08/corpus-status.json`. No later task may interpret
their presence as M02 completion. The original 60-case accepted catalog remains
unchanged under `sources/accepted-seed/verification/`.

The observed missing Container typed API is a permitted nonpassing capability
absence. OBS-01 concerns a different requirement: deterministic real-owner
fault/observation hooks at canonical staging/reservation/publication checkpoints.
Existing Scene hooks are downstream of canonical commit. The package forbids
changing the necessary owner sources and explicitly requires a terminal return
for this observation-hook gap. No simulated coordinator or direct store writes
were used to claim a binding.

The Web build attempt selected the machine's older ambient `EMSDK` and failed
on an unsupported warning under `-Werror`. Its actual compiler path and raw log
are retained. No browser invocation or qualified Web result is claimed. Future
resumption must select and verify the locked-compatible local Emscripten rather
than reuse that build cache. This failed attempt is not a semantic RED and is
not presented as an irreducible environment blocker.

Recheck preserved artifacts:

```powershell
python -m unittest discover -s tests/fc08 -p 'test_*.py' -v
python tools/fc08/check_materialization.py --repo .
```

The checker intentionally returns **2** for the recorded terminal block.
The fixed runner is not fabricated as a placeholder. M02 completion, complete
runner/observation bindings, browser qualification and inherited regressions
remain pending the bounded P31 resolution in `terminal-result.json`.

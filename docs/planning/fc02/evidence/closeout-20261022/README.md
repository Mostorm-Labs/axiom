# FC-02 closeout evidence, 2026-10-22

Tested product: `3d7afa351dfbd42f18c449bda35f861089b1fb4f`,
`Mostorm-Labs/axiom`, branch `codex/fc02-windows-ime-resume`.
Remote delta was `0 0` after fetch. The original resume baseline
`802a064dcb6f543112137c813d7078fef6efe7e5` is an ancestor.
The only preserved untracked file at entry was `imgui.ini`.

This commit contains documentation and execution evidence only. It does not
modify product code, SDK locks, Authority, Golden, historical Gate or PHYS records.
`result.json` returns **READY_FOR_CONTROL_REVIEW**; it is not an independent
review, Gate verdict, or claim that the full physical matrix passed.

Copied text evidence uses UTF-8/LF with trailing whitespace removed. Original
machine-local outputs are unchanged; JSON values and XML test observations are
preserved.

## Read order

1. [Current closeout report](../../../FC02_CLOSEOUT.md).
2. [Public consumer guide](../../../FC02_CONSUMER_GUIDE.md).
3. [Result](result.json), [coverage map](coverage-map.json),
   [machine-derived validation](validation-summary.json).
4. [Human feedback](human-feedback.json), [unobserved manual matrix](manual-matrix.json).
5. [Exact artifacts/processes](artifacts.json), [device facts](device.json).

## Fresh verification

- `ctest.xml` / `ctest.log`: the frozen 233-name regression list plus
  `FC02MinimalTextConsumer`, 234 executed, no failures or skips.
- `python.log`: 17 schema/contract/font checks with the resolved Semantic environment.
- `web-consumer.json`: 5 ordinary JS/WASM consumer cases, all PASS.
- `web_fc02_text_test.json` / `web_minimal_runtime_test.json`: 7 + 13 groups, all PASS.
- `web-shape-reentry-full.json`: committed Chinese/numeric three-shape × DPR1/2
  oracle, all six PASS, including bounded blink frames, no idle frames after
  finish, nearest caret, inside positioning and outside finish.
- `web-shape-reentry.json`: additional numeric-only IME-fact probe, six PASS.
  This earlier temporary probe is retained as supplemental evidence; it does not
  replace the stronger committed oracle above.
- `consumer-compile-surface.json`: actual native and Web compile commands for the
  two current Minimal applications have no `runtime/semantic/src` or
  `AXIOM_RUNTIME_QUALIFICATION`. Windows assert-based entry tests have `/UNDEBUG`;
  the ordinary consumer test uses an unconditional REQUIRE helper.

The initial collection filter also included the legacy Ink Playground, whose
qualification definition is outside this repair's scope. That overbroad collector
assertion was corrected to inspect the two current Minimal applications. No
production flags or legacy code were changed to make collection pass.

Chrome 155 ran headless SwiftShader/CDP. Synthetic composition, pointer and
preedit events are not real IME/touch qualification. The live user page was not
automated. The user reported that both latest products basically passed; no
additional per-case physical results were inferred.

## Reproduction on this machine

Run from the worktree root with the locked SDK environment:

```powershell
$facts = Get-Content docs/planning/fc02/evidence/closeout-20261022/build-and-launch/environment-native.json -Raw | ConvertFrom-Json
$facts.environment.PSObject.Properties | ForEach-Object {
  [Environment]::SetEnvironmentVariable($_.Name, $_.Value, 'Process')
}
$list = (Resolve-Path docs/planning/fc02/evidence/closeout-20261022/ctest-names.txt).Path
ctest --test-dir out/fc02-resume/build --tests-from-file $list --output-on-failure -j 4
python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract verification.tests.test_fc02_host_contract verification.tests.test_host_text_font_resource -v
node --experimental-websocket apps/axiom_canvas_demo/tests/fc02_text_consumer.mjs 'http://127.0.0.1:4175/index.html?rev=3d7afa35' out/fc02-consumer-result.json
node --experimental-websocket docs/planning/fc02/evidence/web-label-reentry-20261022/chrome-reentry.mjs 'http://127.0.0.1:4175/index.html?rev=3d7afa35' out/fc02-reentry-result.json
```

`native-commands.json` / `web-commands.json` are the exact test invocations.
`verify.py`, `web-existing.mjs` and `chrome-numeric-ime.mjs` preserve the local
runner sources as executed; their `out/` paths are machine-local staging paths.
The committed full reentry oracle is under the previous evidence directory.
The `web-shape-reentry-full.json` command is shown above and was run separately
after checking that the temporary numeric runner omitted the stronger bounds.

For a new machine resolve SDKs using `tools/setup_build_environment.py`, including
the offline second resolution, rather than importing this machine's absolute
paths. Use `tools.semantic.cmake_consumer.consumer_cmake_arguments` for CMake.

## Builds and artifact identity

`build-and-launch/` preserves successful configure/build commands and output for
the same clean product revision used by the user:

```powershell
cmake --build out/fc02-resume/build --parallel 6 --target axiom_minimal_runtime_host_windows
cmake --build out/fc02-resume/web-build-repair --parallel 6 --target axiom_canvas_demo_web
```

The native command requires the VS 2026 x64 environment. Web requires the
installed Emscripten environment. Full configure arguments, actual SDK roots,
resolver facts and tool setup are in the retained JSON and `build-latest.py`.
No dependency source rebuild occurred. Prior strict Clang source checks remain
in `../interface-repair-20261022/`; they are prior evidence, not a new syntax run.
The current closeout changes no C++/JS/CMake, so no further product rebuild was
needed after adding documentation.

`artifacts.json` records the exact versioned exe path/hash and all six Web file
hashes. Each served Web file was fetched and compared again with its local file.
The launch-time PID 11876/path/startup `build_revision` and `dirty=0` were checked.
It had exited by the closeout process snapshot; no replacement window was opened.
The Web service still listened on 127.0.0.1:4175, PID 233664 at capture time.

`windows-startup.log` materializes product identity; `windows-log-observation.json`
contains captured log hash and native event counts. Full user text logs remain
local. Counts do not prove per-case visual/IME acceptance. Earlier FAIL and
successful specific human retests are linked in `human-feedback.json`.

The evidence-bearing result revision is resolved without self-referential SHAs:

```powershell
git log -1 --format=%H -- docs/planning/fc02/evidence/closeout-20261022/result.json
```

The executor returns that exact pushed commit as `result_revision` and
`materialized_ref`. Runtime artifacts remain bound to the tested product revision
above; the documentation commit does not relabel the existing binaries.

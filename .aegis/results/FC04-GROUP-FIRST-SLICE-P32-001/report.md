# Group/Ungroup first slice: implementation evidence

The initial source-bound preflight for FC04-GROUP-FIRST-SLICE-P31-v0.1 ran against the clean source at aba4d0f21c553e367fe726bba4b64f78bceb0067 and exited 0 before edits. The bounded package was committed as 6eec357611ca1e8367307d2266d52e8117deb3d8. Separate versioned amendments bind the Selection summary expectation and existing Debug UI. This is a same-preparer execution record, not an independent Control Review or Gate decision.

Tested sources are the dirty worktree above 6eec3576, identified individually by SHA-256 in facts.json. The ordinary RuntimeFacade and Selection -> existing topology -> History -> Scene path implements Group and Ungroup; the Debug UI sends typed intents. The tests cover stale selection rejection and transformed Group -> Section release, as well as single Undo/Redo. The C ABI manifest and wire schema are unchanged.

## Commands and actual results

- Windows (MSVC environment): `cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out/fc04-mse-p32/build --target axiom_fc08_section_consumer_tests axiom_selection_summary_tests axiom_selection_summary_ui_tests axiom_minimal_runtime_host_windows --parallel 8'` — exit 0.
- `ctest --test-dir out/fc04-mse-p32/build -R '^(FC08SectionConsumer|SelectionBatch3Summary|SelectionBatch3Ui)$' --output-on-failure --no-tests=error --output-junit out/fc04-mse-p32/group-focused.xml` — exit 0, 3/3 CTest cases. Because CTest resolves the JUnit path under the build directory, the raw XML was copied from `build/out/fc04-mse-p32/group-focused.xml`.
- `ctest --test-dir <absolute out/fc04-mse-p32/build> --tests-from-file <absolute out/fc04-mse-p32/selection160-names.txt> --output-on-failure --no-tests=error --output-junit <absolute out/fc04-mse-p32/selection160-group.xml>` — exit 0, 160/160; raw names, log and XML preserved.
- `ctest --test-dir <absolute out/fc04-mse-p32/build> -R '^FC04[.]MSE-' --output-on-failure --no-tests=error --output-junit <absolute out/fc04-mse-p32/mse-with-group.xml>` — CTest exit 8: 24 passed; 20, 23 and 26 failed because no matching real tests were selected. They are not passes. The wrapper script itself returned exit 1 following that CTest failure.
- `python tools/check_runtime_abi_manifest.py --root .` — exit 0, 54 functions, 49 structs and 163 enum constants.
- Locked Emscripten 6.0.6 and Semantic web-wasm32 SDK: `cmake --build out/fc04-mse-p32/web-build --target axiom_canvas_demo_web --parallel 4` — exit 0. Latest incremental check reported no work.
- Actual Chrome/WASM, synthetic CDP input: `node out/fc04-mse-p32/chrome-group-undo2.mjs --repo . --build out/fc04-mse-p32/web-build/apps/axiom_canvas_demo --output out/fc04-mse-p32/browser-group-final --chrome 'C:/Program Files/Google/Chrome/Application/chrome.exe' --python 'C:/Users/Chado/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'` — exit 0, 15 assertions, 0 failures; full invocation and artifact hashes in chrome-final.json.
- `git diff --check` on product changes — exit 0. The full staged diff check flags whitespace reproduced verbatim from Google Test output in mse.log and mse.xml; those raw artifacts were deliberately preserved unchanged.

Earlier synthetic Chrome scripts failed because their fixed button coordinates did not account for Debug UI scrolling; their unmodified results are preserved as chrome-earlier-ungroup-failed.json and chrome-earlier-undo-failed.json. The final script uses controlled scrolling and reports its full observations; this is not physical input evidence.

## Remaining work

The first slice does not implement the full Group interaction: nesting, drag into/out of Group, wider mixed-object/selection cases, and comprehensive cross-platform failure and integration coverage remain. The full frozen FC08 corpus and the three missing MSE tests are not claimed as passed. D07 Protobuf/Abseil fault-injection hang remains a separate blocker to final closure and release. Windows and Web physical validation is explicitly deferred at the user's request. No DEV, P34, PHYS or release PASS is claimed.

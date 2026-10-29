# Group scope and hierarchy — bounded implementation and evidence

Starting point: `4e2b629f92cd73f69dc83b4e36d7aeb4bf7ed2dc`, branch `codex/fc04-mse-p31-v02-binding-fix`, clean before this slice. User requested continuing Group interactions and phase 2; prior same-preparer review permission and physical-test deferral remain in effect. Existing first-slice Group and Debug UI file scope applies. This is not independent review, DEV, Gate, PHYS or release approval.

Debug UI now exposes Enter group and Leave group using existing SectionIntent routing and the existing SelectionSession scope owner, fencing the input with selection revision and semantic generation. A real ImGui/Runtime test enters the group, uses actual platform pointer input to select a child, and leaves scope. Before implementation the UI test failed on entering scope (SelectionBatch3Ui, assertion at line 169, CTest exit 8); after implementation it passes.

Real consumer tests additionally exercise nested Group plus Section through group/ungroup and Undo/Redo with unchanged world transforms; pointer-driven movement of grouped Sections and Undo; pointer-driven Group drop into and out of Section with one-operation commits and Undo. These tests verify existing Runtime behavior; they are not evidence of a new drag-into-Group implementation.

## Commands and results

- MSVC 19.51 environment: `cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && cmake --build out/fc04-mse-p32/build --target axiom_selection_summary_ui_tests axiom_fc08_section_consumer_tests axiom_minimal_runtime_host_windows --parallel 8'` — exit 0. The consumer target was rebuilt after the final test addition, also exit 0. Locked published Semantic SDK consumed.
- `ctest --test-dir out/fc04-mse-p32/build -R '^(SelectionBatch3Ui|FC08SectionConsumer|SelectionBatch3Summary)$' --output-on-failure --no-tests=error --output-junit <absolute out/fc04-mse-p32/group-scope-final.xml>` — exit 0, 3/3 test entries. Exact raw log and XML are in raw.zip.
- `ctest --test-dir <absolute out/fc04-mse-p32/build> --tests-from-file <absolute out/fc04-mse-p32/selection160-names.txt> --output-on-failure --no-tests=error --output-junit <absolute out/fc04-mse-p32/selection160-scope.xml>` — exit 0, 160/160. The 160-run preceded the last added Group-out-of-Section assertions; product sources did not change between the runs. The new assertions passed in the focused test afterward.
- Locked Emscripten 6.0.6 and locked Semantic web-wasm32 SDK: `cmake --build out/fc04-mse-p32/web-build --target axiom_canvas_demo_web --parallel 4` — exit 0.
- `node out/fc04-mse-p32/chrome-group-scope-final.mjs --repo . --build out/fc04-mse-p32/web-build/apps/axiom_canvas_demo --output out/fc04-mse-p32/browser-group-scope-final --chrome 'C:/Program Files/Google/Chrome/Application/chrome.exe' --python 'C:/Users/Chado/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'` — exit 0; 18 assertions through real Chrome/WASM and synthetic CDP inputs, including Group, Enter, Leave, root-first reselection, Ungroup and Undo/Redo. The earlier observation script entered scope then attempted Ungroup without reselection and exited 1; the actual raw result is retained in raw.zip. No physical-input claim.
- `python tools/check_runtime_abi_manifest.py --root .` — exit 0. `git diff --check` — exit 0 on source changes.

The web build embeds a previously configured revision string (`6eec3576`, dirty); it is not presented as the current source revision. Exact tested dirty-source hashes and web artifact hashes are in facts.json and Chrome results, respectively. No historical evidence, frozen expected values, descriptors or SDK locks were rewritten.

## Unresolved

MSE-20 requires a real per-object denied permission source; MSE-26 requires a genuine absent/failing property provider; MSE-23 still requires actual capability-query and History-reservation fault coverage. Do not turn absent providers into passing tests. Existing Section drop accepts Group root into/out of Section; Group membership drag-in/out and its product semantics/UI remain unfinished. Wider nested transforms, cross-platform matrix and physical validation remain incomplete. The separately known Protobuf/Abseil OOM hang remains unchanged. No claim of stage 2/3 completion is made.

# Selection Batch 3 — implementation receipt

Tested source: `b588c93a2c4b19a5f536453cf3f8c4c9c4d69626` (clean). Base: `0eb11b25d60f2b6f932f12735de4779d2a3f2f21`. Branch/ref: `codex/selection-summary-batch3`. This receipt is an evidence-only descendant of that tested source. Main is unchanged; no PR or merge was created.

## Prior batches and scope

Batch1 `5daf623eba9a943706d813f8e197e00a9f6a8b2e` and Batch2 `0eb11b25d60f2b6f932f12735de4779d2a3f2f21` are implemented local branches, both ancestors of this subject, not merged into main `2fee75f66a7f96f6aac834228ede372a9abda59a`. Exact-head GitHub searches found no PR for either. Their branches and earlier evidence remain intact. The supplied ZIP and five accepted documents were hash-checked byte-for-byte; execution identity is in docs/planning/selection-batch3/execution.json. Existing cancel/toggle, root resolution, input and Scene owners were reused.

## Delivered ownership

- SelectionSession: normalized operation roots, validated primary, per-kind root counts, Group scope and existing revision. Read-only projection, no second selection store.
- CanvasEditingRuntime: consistent published Scene bounds, grounded action capabilities, typed visibility/lock/opacity uniform/mixed/partial/unavailable aggregates; lifecycle, stale, invalid hierarchy, missing metrics/routes and active-session reasons. No partial bound is advertised as complete.
- Semantic: read-only access to existing applicability and exact canonical-default table. Opacity=1 is the accepted elided default, not a new fallback. Existing normalization regression stays unchanged.
- Facade/composition: same summary DTO through a separate query and CanvasControlSnapshot. Legacy RuntimeStateSnapshot stays trivially copyable; its selectionRevision is the actual existing counter.
- DebugSnapshotAssembler/Debug UI: owning value copy, existing leading/trailing product reads plus real token fencing; stale IDs/actions are cleared. Control/Dashboard/Inspect and selected actions consume Runtime facts, rechecking at submission. No multi-root primary-only mutation.
- Minimal Windows/Web facade forwards only; Web bridge serializes full IDs and decimal-string64-bit tokens. Existing specialized Text/Image/Crop and FC08 Section panel paths remain intact.

All45 changed files, owner mappings and source hashes: `source-files.json`. Source/design notes: `docs/planning/selection-batch3/05-source-contract-map.md` and `06-implementation-notes.md`.

## Actual validation

| Execution | Result | Exit |
| --- | --- | --- |
| Windows Native Release /MT /WX scoped builds, including real D3D12 Minimal Host | PASS | 0 |
| 92 discovered Native CTests, no skipped/empty target matches | 92 PASS | 0 |
| Emscripten WASM/WebGL2 Minimal Host | build PASS | 0 |
| Actual headless Chrome/WASM production host, synthetic CDP mouse facts | 38 checks PASS | 0 |
| Semantic/host Python unittest | 18 PASS,0 skipped | 0 |
| Runtime module boundaries | 103 contract files PASS | 0 |
| Source diff whitespace | PASS | 0 |

Native scope includes summary/defaults, real ImGui/MinimalRuntimeFacade/snapshot contracts, Batch1/2 acquisition and geometry, transform/History/lifecycle/input, Section, Text, Image/Crop, Connector, Brush/frame retry and selected creation/action regressions. It is not a physical-device claim. Chrome ran the actual WASM host with SwiftShader and DOM pointer delivery, not a native substitute; actual revision/dirty0 and artifact hashes were checked.

Exact expanded configure/build/test argv and exit codes are in `commands.json`; discovered names and JUnit are in `raw/native-ctest-names.txt` and `raw/final-native-regression.xml`. Original local commands:

```powershell
cmd.exe /c 'call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat" >nul && D:\Softwares\Anaconda\python.exe -X utf8 out/selection-batch3/run_final_native.py'
& 'D:/Softwares/Anaconda/python.exe' -X utf8 out/selection-batch3/run_final_web.py
& 'C:/Users/Chado/AppData/Local/Axiom/toolchains/emsdk-6.0.6/node/24.19.0_64bit/node.exe' apps/axiom_canvas_demo/tests/web_selection_batch3.mjs --repo . --chrome 'C:/Program Files/Google/Chrome/Application/chrome.exe' --python 'D:/Softwares/Anaconda/python.exe' --git 'D:/Softwares/Git/cmd/git.exe' --output out/selection-batch3/browser-final
& 'D:/Softwares/Anaconda/python.exe' -X utf8 out/selection-batch3/run_final_python.py
```

Python driver imports the emitted SDK environment and runs the exact18-test unittest command and boundary checker listed in commands.json. Locked Semantic native/Web offline resolutions and Skia verification were reused; no dependency source rebuild or lock changes. Reproduction drivers/configuration are included; discover local toolchain/cache paths on a different machine.

## Failure preservation and classification

Raw RED/dirty-source intermediate logs remain historical failures, not final PASS: missing model/API compile checks, bound/stale/metric behavior, mixed-property/capability and real ImGui disabled-action behavior, lifecycle/empty-Group cases, absent Web transport and the canonical-elided-opacity omission. The expanded Native build caught the legacy trivially-copyable state contract; it was preserved, not weakened, and the dynamic DTO moved to the separate snapshot/query. A snapshot publication RED then led to owning-copy and generation-fence regressions. One font fixture was repaired to admit the locked real font identity without publishing bytes; an early nonfatal property assertion was corrected to avoid dereferencing an absent optional. Initial SDK flag misuse and one relative CTest list-path driver error are setup failures, not Axiom passes. No frozen corpus/oracle/historical evidence was modified.

## Remaining range

No blocker remains for this bounded batch. The aggregate profile is visibility/lock/opacity only; fill/stroke/font/text effects and bulk writes are not added. New Group/Ungroup/Wrap mutations, Snap/Alignment are outside scope/disabled; existing FC08 dedicated commands are preserved. Summary-specific exhaustive combinations for every kind/resource failure are not claimed; existing Image/Text/Connector workflow passes do not constitute that exhaustive coverage. Physical touch/pen/IME/GPU, hosted CI, other platforms, full302 and G5 performance are NOT_RUN. On-demand Group scans and pure delete planning need later scale measurement. No DEV/P34/PHYS/Gate/release readiness claim.

Next smallest step: review/integrate the three existing selection batches, then separately authorize the next selection-editing slice. No automatic main merge.

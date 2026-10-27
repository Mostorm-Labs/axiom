# Selection Batch 3 reproduction

Run from this repository root with the locked resolver facts in out/selection-batch3. The recorded configure arguments and environment JSON describe this machine; resolve SDKs/toolchains for a different machine instead of assuming these absolute paths exist. These scripts are local verification drivers, not a frozen FC08 product runner.

- Native: enter the VS 2026 x64 environment, then `python -X utf8 out/selection-batch3/run_final_native.py`. The driver builds only its explicit target list, discovers actual CTest registrations by executable, rejects missing targets/empty matches and uses an absolute tests-from-file path with --no-tests=error.
- Web: `python -X utf8 out/selection-batch3/run_final_web.py`, then run the committed `apps/axiom_canvas_demo/tests/web_selection_batch3.mjs` with verified --chrome, --python and --git paths and a fresh --output directory. This starts actual headless Chrome/WASM and uses CDP mouse facts; it does not establish physical-device acceptance.
- Python/boundaries: `python -X utf8 out/selection-batch3/run_final_python.py`; resolver environment is imported before all tests, so missing protoc is not silently skipped.

Use the raw command JSON arrays for exact invocation/exit codes. The final source subject is recorded in result.json; later receipt-only commits do not change that tested subject. Original RED and intermediate dirty-source runs are preserved separately and never relabeled final PASS.

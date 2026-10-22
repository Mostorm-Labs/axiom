# FC-02 Windows device startup — 2026-10-22

Source baseline: `164dfa2fc0af84df397b3ca2de0f6543fd50da02`.
Tested original product: `3d7afa351dfbd42f18c449bda35f861089b1fb4f`.
Branch: `codex/fc02-device-startup-20261022`; no merge is authorized by this task.

Read [the investigation](../../../FC02_WINDOWS_DEVICE_STARTUP_20261022.md),
[result.json](result.json), then [manual-matrix.json](manual-matrix.json).
Original FAIL remains FAIL. The complete-resource CLI startup succeeds with
the exact same executable hash, without rebuilding or changing product code.
That does not mean the ten physical scenarios or FC-02 independently passed.

`original-*` preserves the exe-only directory and launch, diagnostics, PE and
correlated Application/WER query. Original exe bytes are preserved locally in
out/fc02-device/intake, not committed. `icu-only-*` and `icu-cjk-*` preserve the
two still-failing controlled retries. `complete-*` captures startup, process,
loaded DLLs and a native read-only HWND/DPI snapshot. Raw text is stored as
lossless gzip snapshots (mtime=0); JSON is UTF-8/LF.

`device-info.json` / `display-info.json` preserve OS/CPU/GPU/driver, monitor DPI,
touch/pen facts. `application-events-after-probes.json` is limited by the run
time window and exact app filename. An older crash of another surface-binding
test was excluded from attribution to this application. No crash dump or system
security changes were needed.

`distribution-manifest.json` binds every packaged file except the manifest
itself; `delivery.json` identifies the local complete directory and ZIP/hash.
No exe/font binary duplication or user imgui.ini is added to Git. The raw
startup scripts are retained for audit, using this machine's discovered paths;
they are not a new Runtime API or replacement package authority. Do not run the
one-shot staging scripts over existing delivery directories.

`commands.json` distinguishes new local checks from NOT_RUN_DEPLOYMENT_ONLY
product build/CTest/Python suites. Historical 234/17 results are not relabeled
as local reruns. The locked Skia consumer reverified the installed SDK without
network download or source compilation. `sha256-manifest.json` binds all files
in this evidence directory other than itself.

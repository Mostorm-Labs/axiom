# POC-01 Android Visual Smoke Evidence

This directory is reserved for the independent Android visual-smoke result.
It is intentionally separate from `verification/evidence/g0/gt-g0-12/`, which
records the Android lifecycle/JNI instrumentation adapter.

The visual smoke must be produced by
`pocs/shared_engine/tools/run_android_visual_smoke.py` and must include:

- `result.json` with `executionKind`, `physicalExecution`, backend, document
  digest, RGBA hashes, text-region metrics and the visual gate;
- `actual.rgba` containing exactly 800×600×4 RGBA8888/sRGB bytes;
- `visual/expected.png`, `visual/actual.png`, `visual/diff.png` and
  `visual/metrics.json` from the reviewed POC-01 comparator;
- `logcat.txt`, APK hash and a file-level manifest for the captured run.

The fixed requirements are:

- fixture digest `47826449b895ac4f4a57b4f386379775`;
- `Canvas v2` text region must contain non-background dark pixels;
- at least 99.9% of pixels must be within per-channel tolerance ±2;
- the hosted emulator and physical Android device reports are independent;
- this report cannot change or replace the G0-12 instrumentation Evidence.

No result is marked Pass until a hosted or physical run has produced the
artifacts above. The current implementation is the reproducible runner and
workflow; the first captured result is added under a dated child directory.

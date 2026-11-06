# G5 pause/resume navigation repair

P35/P36 implementation repair on `codex/geometry-navigation-integration`.
Code revision: `0d581f8921d2c706529a201ea26a5d05521178da`.
Disposition: **READY_FOR_CONTROL_REVIEW**, limited to this deterministic repair.
This does not supersede previous BLOCKED evidence or declare G5/P34, physical
latency, another platform, or 100K/1M acceptance.

## Reproduction and causal finding

The user reported that 4240 paused after trackpad zoom-in, about one second of
inactivity, then zoom-out or pan, while historical 4241 felt smoother. Their own
document contained an approximately 2.3 MB image, rectangle, triangle, ellipse,
sticky, table, and about five long strokes.

The root-SDK reproducer sends 18 RAF bursts of four synthetic ctrl-wheel events,
pauses with a 1000 ms timer without waiting for exact completion, then sends
another 18 bursts for zoom-out or pan. It keeps all 144 input events and records
dispatch, RAF, GL content calls, local accepted-submission diagnostics (ON only),
coarse receipts, final pixels, and document/operation/selection identity. A GL
content call is not proof of physical display or a camera-qualified frame.

Cached SDK bisection first observed the stress regression at `903761e4`:
`90ef6b69` and `676aad1e` responded promptly, while `903761e4`, `5672f6ae`, and
`3fcf642b` waited about 500 ms. Higher resident-density planning exposed a
pre-existing retention weakness: a completed narrow zoom-in frame replaced the
only retained wide frame. Resumed navigation waited for a complete cold tile
plane although input callbacks continued. Reverting the density formula removed
the stress symptom; reducing density only during motion did not. Both density
experiments were withdrawn. The committed fix retains the latest density policy.

The renderer now retains at most one additional, already admitted, overlapping
wide canonical image with matching provider/frame/Scene/surface/metrics identity.
Coverage partitions those two sources and world tiles by density, so a coarse
source does not replace sharper overlapping pixels. There is no new readback,
pixel copy, pool, or budget increase. The extra source is retired first under
mandatory pressure. GPU reads are marked when an image is actually sampled.
Unknown regions and changed Scene identity cannot be certified by these images.

## Final clean-source matrix

All 14 final workers used the same clean `0d581f89` observer source, immutable
matching installed packages, 180 s / 3 GiB process-family bounds, and complete
worker cleanup. The SDK builds are `90ef6b69` (old), `3fcf642b` (before repair),
and `0d581f89` (fixed), version 0.3.9. The requested `8c4ab04` changed evidence;
the historical installed production code was `90ef6b69`.

The user-like fixture has 12 Runtime objects, the three shape kinds above, five
2048-point `round-handdrawn@4` strokes, sticky, table, and a deterministic
2048 x 1024 PNG of **2,334,857 bytes**. It is not the user's private photo. Delta 1
produces approximately 2.05x zoom; the delta 4 stress fixture has two shapes and
approximately 17.8x zoom. View: 636 x 923 logical pixels, DPR 2. Chrome
156.0.8078.81, ANGLE Metal / Apple M2. Cases run sequentially, without concurrent
build/test work. These are diagnostic reproductions, not independent statistical
performance blocks.

| Case | Before content response | Fixed content response | Fixed accepted submit (ON) | Fixed max content gap |
| --- | ---: | ---: | ---: | ---: |
| Stress zoom, ON, delta 4 | 499.6 / 500.9 ms | 1.9 / 1.4 ms | 2.9 / 2.2 ms | 19.8 / 19.4 ms |
| User-like zoom, ON, delta 1 | 202.6 ms | 1.8 ms | 31.0 ms | 31.9 ms |
| User-like pan, ON | 60.5 ms | 1.1 ms | 30.5 ms | 43.5 ms |
| User-like zoom, OFF | 191.6 ms | 1.6 ms | unavailable | 30.5 ms |
| User-like pan, OFF | not sampled | 1.1 ms | unavailable | 42.1 ms |

The historical SDK also waits 198–214 ms in the user-like zoom case; its prompt
14.4 ms initial stress response is not universal, and its stress maximum gap is
486.6 ms. The fix addresses the retention weakness instead of promising that all
old-version behavior was smooth. Final coarse receipt waits remain about
179–263 ms in fixed cases. Response, maximum gap, and receipt wait are different
metrics and are not substituted for one another.

All scenario-matched final pixel SHA256 values are identical across versions and
diagnostics ON/OFF. Document generation, latest operation, and selection remain
unchanged during navigation. Both before-fix stress workers deliberately fail
the local <=150 ms accepted-response guard; this is expected RED, with a normal
nonzero child exit and successful cleanup, not an environment failure. All fixed
ON workers pass that guard. OFF native diagnostics are **UNAVAILABLE**, not zero.

See [runs/matrix.json](runs/matrix.json) and the retained compressed raw records.

## Verification and limits

- Native regression: the new test fails before repair with `zoom-in canonical
  discarded wide coverage before resumed zoom-out`; passes after repair, including
  unknown-region/Scene rejection and no canonical receipt from feedback.
- Eleven Chrome/WebGL2 native renderer modes pass: reentry, retirement, detail,
  oversized, frontier, quality, affine, background, contour, replay, exact-replay.
- Full configured native CTest: **1008/1074 passed**, with exactly the same 66
  failures/not-run as the archived baseline and no new failed names. Four existing
  Semantic test compilation failures remain. The full suite is not green. See
  [comparison](checks/g5-reentry-full-native-comparison.json) and compressed logs.
- 39 Node tests, 16 Python tests, and 118 Runtime boundary contracts pass.
- Normal ON/OFF installs each contain 36 files, root-only exports, and no private
  headers/archives. Shared default budgets remain **32/96 MiB**.
- Native/private GPU checks were built before the code commit from identical
  source bytes. Final normal SDKs and all 14 final workers bind the clean code
  commit. [source-bindings.json](source-bindings.json) distinguishes these facts.
- `diagnostic/` includes withdrawn prototypes and intermediate injected HTML.
  Some intermediate embedded build IDs are stale; directory names are not SDK
  authority (notably `676-on-zoom` contains SDK `903761e4`). Those records support
  investigation only. `runs/` is the qualifying final matrix.
- The earlier retirement-arbitration BLOCKED result is unchanged. Other G5
  navigation quality, recovery-tail, physical-input and scale obligations remain.

Read-only evidence verification (no browser or dependency installation):

```sh
node verification/evidence/performance/g5-pause-resume-repair/verify.mjs
```

`manifest.json` hashes every file in this bundle except itself. The verifier
recomputes feedback/accepted-submit metrics from raw events, checks source and
package bindings, expected RED/GREEN results, pixel equality, test counts, budget
settings and preservation of the previous BLOCKED result. `method/` freezes the
measurement code; execute the repository runner to reproduce, rather than running
the snapshot from its relocated directory. `reproduce/matrix.py` preserves the
exact sequential matrix and local SDK paths; use a fresh output path for reruns.

## Manual Demo

Normal diagnostics-OFF fixed Demo: <http://127.0.0.1:4242/example.html>.
Before-fix 4240 and historical 4241 remain running and were not reloaded.
`checks/demo-smoke.json` records two canvases, matching fixed mixed-page identity,
and no page errors. Test the user's actual document and physical trackpad on 4242;
the synthetic corpus is not a replacement for that comparison.

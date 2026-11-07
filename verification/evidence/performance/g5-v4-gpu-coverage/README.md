# v4 GPU coverage and exact repeated-erase no-op recovery

This is bounded local Web/Ganesh engineering evidence. GPU coverage is ready for independent control review. The user's second-erase `INPUT_REJECTED` remains **BLOCKED_REPRODUCTION**: it was not reproduced, and no semantic rejection was bypassed. This does not issue G5/P34, release, competitor, physical-input or cross-platform acceptance.

## Change and ownership

Runtime still owns input, pinned contour compilation, exact erase/no-op filtering, OperationEngine, History, Scene and receipts. The existing Skia adapter now requests GPU N32 coverage tiles for v4 on an available Ganesh target; the same contour/checkpoint/indexed replay supplies geometry. There is no CPU edge correction. Unsupported/raster targets retain CPU A8. CPU Direct recordings keep pinned Lighten bytes. Preview/canonical consume the existing geometry and keep their existing surfaces/lifetimes.

GPU white coverage uses fixed-function SrcOver, including recordings that have no GPU context themselves. The cache passes the target realization explicitly. This changes the coverage pass; colored paint/material opacity is applied once as before. A naive GPU Lighten prototype passed geometry but failed latency (30.6/24.1 second whole erase/navigation phases); it is retained as a failed experiment, not qualification. Its actual GPU tiles were 782/753, CPU tiles zero; moving coverage targets alone was insufficient. GPU-only white SrcOver removed the destination-dependent submission cost. CPU behavior is independently pinned, rather than globally substituting SrcOver.

Memory/work budgets remain 32/96 MiB. SDK public API/ABI/version, Brush geometry/pressure, semantic schema, dependency locks and old P34 evidence are unchanged. No dependency SDK was rebuilt.

## Verification

The real Chrome/Ganesh probe uses ANGLE Metal on Apple M2. It covers all three v4 profiles (handdrawn round, regular round, marker): 72 affine/background/cache combinations, evolving masks, stale generations, CPU A8 fallback and a deliberately offset tile-grid seam. Only a one-device-pixel band around independently transformed pinned contour/mask boundaries permits AA differences. Outside it tolerance is 2; transparent premultiplied color and maximum opacity are checked separately. The fixture is re-projected after content/transform mutation so stale two-knot bounds cannot crop the oracle. GPU totals: 204 tiles, zero CPU tiles and zero CPU edge pixels.

The older strict CPU affine oracle explicitly remains on CPU coverage in the Web probe. GPU canonical AA is separately qualified above; GPU navigation is checked against an independent GPU contour stream. Structured timing diagnostics are separated from actual error messages in that supplementary driver.

The sequential root-SDK fixture uses the default round-handdrawn v4 brush, 4800 variable-speed/self-crossing samples, 15 duplicates, two 121-point overlapping erasures and 48 immediate wheel events after each Up. It never waits or exports between Up and navigation. Five checkpoints compare canonical snapshot and CPU-export SHA256 through both erasures, Undo and Redo. Final compositor AA may differ; canonical data and CPU export must remain identical. Metrics include the existing 200ms quiescence observation window, not a physical display deadline. Whole-browser task CPU includes the fixture's final capture. OFF diagnostic fields are unavailable/null, not measured zero.

| DPR2 corpus | CPU coverage ms | Recovery after input ms | Up ms |
| --- | ---: | ---: | ---: |
| cpu-on-dpr2 | 750.5 / 753.0 | 1092.6 / 1091.5 | 415.1 / 817.4 |
| gpu-on-dpr2 | 0.0 / 0.0 | 423.1 / 393.8 | 416.3 / 866.2 |
| cpu-off-dpr2 | n/a / n/a | 1072.7 / 1055.6 | 416.8 / 822.4 |
| gpu-off-dpr2 | n/a / n/a | 362.2 / 388.8 | 412.5 / 862.2 |

Full native: 1014/1080 passed, the same 66 failed/not-built names as the prior accepted local baseline. Four Semantic test translation units still fail exhaustive-switch compilation. The current failed names and complete logs are retained. The final native binaries remained unchanged during execution. 42 G5 Node tests, 119 Runtime boundary checks, collector tests, GPU AA/seam/affine and ordinary Demo smoke passed. The ordinary Demo smoke creates 16 duplicated strokes, performs 12 short erasures and then zoom/pan, without UI/page errors.

## Repeated-erase CPU tail

A separate public-SDK reproduction repeats the identical121-point eraser trajectory without moving the viewport. The second gesture is an accepted, non-mutating no-op, but the old query spends about9seconds intersecting/subtracting masks over thousands of contour components. This explains a demonstrated second-erase CPU spike independently of coverage: Axiom CPU coverage is already zero in that run.

The existing pure Render query first validates the source/new mask. It then tries an exact empty-query proof against at most8 historical masks in original order, using exact semantic geometry equality or query-minus-old-mask. Only a valid empty result returns Miss early. Nonempty/failed proof retains the original component intersection/fail-closed path and reuses query-local paths. No new cache/state owner, area tolerance, input rejection bypass, Operation or History policy is introduced. Native tests preserve tangency, tiny positive area, partial/combined masks, affine and invalid-source behavior. Root-SDK RED/GREEN compares mutation count, snapshot/export identity and Undo/Redo, not only elapsed time.

| Repeated/covered query | Second Up ms | Snapshot/export unchanged |
| --- | ---: | --- |
| repeat-red | 9015.8 | True |
| repeat-green-on | 372.9 | True |
| repeat-green-off | 365.0 | True |
| repeat-contained-off | 462.8 | True |

## Remaining boundary

The CPU exact-hit/commit time on Up remains material. GPU rendering does not bypass the CPU semantic eraser. Diagnostic builds now distinguish ingress phase, exact geometry/PathOps, write admission, publication reservation, semantic apply and History rejection at existing owners. The controlled traces and UI smoke did not reproduce the user's original rejection, so it is explicitly unresolved.

GPU-target/counter evidence proves Axiom no longer invokes its own CPU coverage/edge path in these GPU runs. It does not certify every internal Skia path renderer or driver kernel as GPU-only. [Upstream Skia's software path renderer](https://skia.googlesource.com/skia/+/refs/heads/main/src/gpu/ganesh/ops/SoftwarePathRenderer.cpp) illustrates why a Ganesh target alone is insufficient evidence for that broader claim. No production GL interception or new Skia SDK build is used.

DPR1 passed as corroborative evidence on the prior GPU-only code. The DPR4 worker completed both erase state/receipt checks but hit its180second limit before completing Undo/Redo; the partial log and timeout are preserved as BLOCKED_EVIDENCE, not PASS. This CPU-query change does not close that DPR4 render/recovery boundary. No real Android/Windows/iOS artifact or physical Mac trackpad acceptance is supplied. The G3 snapshot authority issue and existing failed tests are untouched.

## Reproduction

Use the matching installed ordinary Demo at `http://127.0.0.1:4267/example.html`; diagnostic variant is 4268. 4263/4264 are the older naive GPU prototype;4265/4266 are the earlier GPU-only version and must not be mistaken for this result. Existing user documents/servers were preserved.

Code: `10a9a0f0dafc362c1e589c03e73069740eaf9ae7` (GPU layer introduced at `86367caec3341c5e3aa1c1682abde4f0ddb632f0`). Verification-only descendants are recorded in result.json; the production tree is unchanged. SDK build metadata reports that exact clean code revision. Raw observations contain full runner source identities and before/after package SHA256 maps. Run `python3.12 verify_evidence.py` here for offline validation. This verifies local integrity/oracle outcomes, not an official Gate.

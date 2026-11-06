# G5 post-main follow-up — 2026-11-07

This bundle executes the four ordered steps authorized by the user and frozen in
`docs/planning/G5_POST_MAIN_EXECUTION_2026_11_07.json`. The test/tooling and bounded
engineering-observation slice is **READY_FOR_CONTROL_REVIEW_WITH_FINDINGS**.
The DPR4 finding below is **not repaired**. This is not G5/P34, statistical,
physical-trackpad, release, or other-platform acceptance.

## Source, ownership and artifacts

- Controlled worktree: `/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`.
- Branch: `codex/geometry-navigation-integration`.
- Task anchor: `55e21707ccaa42cb5c2fef49de1265134fd6a996`.
- Frozen code: `c05d3a2f54fcb44c771d797bbe3255f46dcac962`.
- SDK: 0.4.3. Both diagnostic ON and default OFF packages were configured, built
  and installed after this clean code commit. Public `readBuildInfo()` reports the
  same revision and `dirty=false` in all ten final edited-navigation workers.
- Each package has 36 files, only the root package export, and no native headers
  or archives. Complete package hashes are in `raw/packages.json`.
- Web build settings remain Release, 32MiB optional and 96MiB shared Render budget.
  Prebuilt Semantic/Skia SDKs were reused. No lock, SDK version, API/ABI, Brush
  algorithm, semantic schema/admission rule or production Runtime was changed.
- Runtime/OperationEngine/History retain document and editing ownership;
  Scene/Render/Skia/provider retain derived preparation and presentation ownership.
  The new runner uses the installed public root JS SDK and existing diagnostics,
  not a second editing or rendering implementation.

`source-inputs.json` records the source fingerprint, creation-test hash and native
fixture binary hash. The native fixture/test were rebuilt from the same content
before the code commit; their build logs and binary identity are recorded
separately from the clean Web build stamp. Production code matches the task anchor.
Source and artifact hashes remain stable across each final worker and scale run.

The user's 4242 Demo/server/tab/document, older SDKs, historical Gate records and
earlier G5 evidence were preserved. These workers use their own ephemeral servers
and Chrome process families.

## 1. Four creation fixtures

Updated `verification/runtime_qualification/tests/creation_control_binding_test.cpp`
to assert the current one-shot creation contract: successful creation returns to
point selection and selects the created object. A second creation explicitly
reselects its tool. Same-pointer reuse and other-source cancellation remain covered.

The four cases failed before the fixture correction and passed afterward:

- `FC01Creation-arrow-arrow`
- `FC01Creation-arrow-arrow-pointer`
- `FC01Creation-line-line-pointer`
- `FC01CreationSources`

Logs: `logs/g5-followup-creation-{red,green}.log.gz`.
This repairs obsolete test expectations, not production interaction behavior.

Full configured CTest: **1010/1076 passed**, versus the previous 1006/1076.
There are exactly four resolved names and **no new failed names**. The other
66 existing failed/not-built cases, including four unbuilt Semantic executables
and the independent G3 snapshot issue, remain named in
`raw/native-comparison.json`. This is not an all-green native suite.

Other executed checks: 39 G5 Node tests, 119 Runtime boundary checks, and 11 Python
worker/preflight tests passed. The corresponding complete logs are under `logs/`.

## 2. Edited-navigation baseline

New executable: `verification/runners/g5/post_edit_navigation.mjs`.

Each fresh worker reports seven canonical objects, with rectangle/ellipse/table/sticky, a real
2,334,857-byte synthetic PNG (2048×1024), and a 4096-input long stroke. Copy/paste
adds another object, giving eight at the end. Synthetic image size does not imply
the same decode characteristics as the user's original image.

In the same document, it performs copy/paste, transform, three partial erases,
sticky text, table text, and image-resource replacement. Each of these eight
stages checks generation/operation/presentation and exact public-SDK export pixel
round trips through Undo/Redo. Each stage is followed by zoom/pan navigation.
Selected/unselected × paused zoom/pan add four reentry phases, with a deliberate
1000ms pause and no exact-completion prewait. Document generation, operation and
selection must remain unchanged during navigation. Camera restoration uses the
public SDK outside timed phases.

Two verification-fixture errors were found and corrected before freezing:

- `transformSelection` takes absolute transforms; the image helper now preserves
  the existing matrix and verifies its target bounds.
- `panBy` follows wheel-scroll semantics; camera restoration now uses the correct
  sign. No Runtime behavior was changed to accommodate these fixture errors.

All ten clean-source workers completed eight edits and twelve navigation phases;
the three context-restore workers add one further navigation phase each. Five
ON/OFF pairings have identical per-edit export pixels, per-phase final canvas
pixels and final export pixels, including DPR4 and context restoration.
See `edited-navigation.json`, `paired-pixels.json` and `raw/*dpr*/observation.json.gz`.

## 3. Cold restoration and remaining DPR4 finding

The primary DPR2 pair executes real `WEBGL_lose_context` loss/restoration after
compound editing. Surface availability changes, document/operation and export
pixels remain unchanged, and ON diagnostics show surface generation **1 → 2**.
OFF has no native diagnostic generation; it is recorded as null, not fabricated.

| Worker | First restored receipt after restore request | Max content-call gap in navigation | Max post-last-input final receipt wait |
| --- | ---: | ---: | ---: |
| ON DPR1 | — | 18.5ms | 149.8ms |
| OFF DPR1 | — | 18.2ms | 150.4ms |
| ON DPR2 | — | 59.9ms | 167.2ms |
| OFF DPR2 | — | 35.8ms | 166.1ms |
| ON DPR2 restore | 69.4ms | 34.0ms | 166.4ms |
| OFF DPR2 restore | 66.6ms | 34.7ms | 184.2ms |
| ON DPR4 | — | 317.2ms | 234.4ms |
| ON DPR4 repeat | — | 317.3ms | 233.1ms |
| OFF DPR4 | — | 318.9ms | 216.8ms |
| ON DPR4 restore | 126.2ms | 333.6ms | 235.0ms |

These are observed maxima over named synthetic phases, not statistical regression
estimates. ON/OFF instrument cost and run order are not a prior-version comparison.
The 100ms deliberate context-loss pause and subsequent quiescent RAF wait are
excluded from the first-restored-receipt column; `completeMs` includes them.

The DPR4 selected-pan gap is reproducible. In the first ON worker, eighteen
navigation attempts between accepted submissions return `presented=false` and
`coverage=false`, processing about 1,888–2,048 knots per attempt. Individual calls
take approximately 0.4–1.3ms (11.4ms in total). The next accepted canonical attempt takes about
0.6ms. This supports **coverage preparation spread across RAF opportunities** as
the immediate explanation for this submission gap, rather than one long CPU task.

DPR4 paused zoom is a separate cost: the accepted canonical attempts spend about
62–64ms in CPU coverage over 46 calls. These measurements do not isolate image
decode/upload from every other resource preparation step, so no claim that image
decode is the root cause is made. Shared budget usage peaks at about 67.24MiB;
that does not exclude sub-budget or retained-resource choices as contributing
factors. No budget or per-frame work limit was increased to hide the wait.

The next targeted Render work should trace pending coverage jobs and retained
content eligibility across these short attempts, preserve useful completed work
and allow feedback as soon as complete new-view coverage is available. It must
retain work bounds, pixel coverage and receipt rules. The separately measured
zoom coverage cost should not be conflated with this wait. This bundle provides
the reproducer and diagnostic evidence; it does **not** claim that fix or a new
latency SLO has been accepted.

GL content calls, local Runtime accepted submissions, final canonical receipts,
and physical display are distinct. `cameraQualified=false` is preserved. OFF
Runtime diagnostic timing and budget counters are unavailable/null. No same-frame
readback or optical sampling was added inside the latency loops. Final canvas
readback and export checkpoints are outside them. A synthetic wheel is not a
physical Mac trackpad, and the 1000ms pause is not an hours-long idle qualification.

## 4. Admission limits and bounded scale attempts

Admission limits were inspected before the scale attempts and were not changed:

- `runtime/semantic/src/codec.cpp`: snapshot preflight permits at most **65,535
  occurrences of a repeated wire field per message**, and an ObjectRecord is
  limited to **16MiB**. The repeated-field boundary explains this 100K-object
  snapshot rejection; it is not a universal Runtime document object maximum.
- `runtime/semantic/include/canvas/semantic/compact_stroke.hpp`: a CompactStroke
  must contain **1–262,144 knots**. A single 1M-knot stroke is invalid; this does
  not forbid multiple valid strokes whose document total is 1M.

The frozen existing corpus produced **49 outcomes: 36 MEASURED and 13
BLOCKED_EVIDENCE**. The latter are neither omitted nor marked performance passes.

| Axis | Component/fixture outcome | Public root SDK outcome |
| --- | --- | --- |
| 1K/10K objects, sparse/dense | Completed | Restore/render/navigation completed; DPR1/2 at 1K, DPR2 at 10K |
| 100K objects, sparse/dense | Scene/index checks completed; snapshot inspect rejected with SemanticError 7 | Blocked by unavailable admitted fixture |
| 1M objects, sparse/dense | Component/fixture workers hit 2GiB process-family cap; partial stages retained | Blocked; no 1M-object SDK claim |
| Single-stroke 1K/10K/100K points | Completed | Restore/render/navigation completed |
| Single-stroke 1M points | CompactStroke validation rejected | Blocked |
| Document total 1M points: 10 × 100K | Valid snapshot round trip | ON diagnostic DPR2 observation completed: 10 objects, 1M stored/source points, 16K point centres in the current viewport |

The last row is a separate scope, not 1M objects or all 1M points simultaneously
visible. That SDK worker took about 7.51s for the entire bounded script, not one
frame or navigation gap. Render budget peaked at 47,706,624 bytes. Five additional
single-edit kinds (transform/erase/text/table/resource) completed ON/OFF, including
history, navigation, receipts and snapshot census. See `raw/scale/matrix.json` and
`raw/point-total/result.json` for counts, timings, checks, masks, cache and stages.

Every worker has a 180s limit; native and browser-family RSS bounds are 2GiB and
3GiB. All **76** final workers were cleaned up. RSS sums can double-count shared
pages and are containment measurements, not unique physical memory use.

## Reproduction and retained evidence

Build matching ON/OFF Web SDK packages from the frozen code using the accepted
prebuilt dependency path. Exact configure/build/install commands are preserved
in `logs/g5-followup-build.py` and its logs. The same root wrappers, declarations,
WASM and resources must be used together.

From the repository root, for example:

```sh
python3.12 verification/runners/g5/bounded_worker.py \
  --prefix out/recheck-worker --timeout 180 --rss-mib 3072 -- \
  node verification/runners/g5/post_edit_navigation.mjs \
  out/g5-followup-on/js out/recheck 2
```

Set `G5_CONTEXT_RESTORE=1` for the additional context-loss/restore phase. DPR4 is
explicitly corroborative. Full ordered worker orchestration is preserved in
`logs/g5-followup-execute.py` and `logs/g5-followup-high-dpr.py`; use a fresh output
directory when reproducing. Do not run builds/native tests during latency sampling.

`pilots/` contains failed fixture investigation and successful exploratory runs
against an older installed artifact. They are **diagnostic-only**, not clean-source
qualification and not mixed into the final table. RED/GREEN, full native, Node,
Python, boundary and build/install logs are retained under `logs/`.
Compressed raw observations, worker outcomes, stage logs, source and artifact
hashes are archived. Large local `.axsnap` payloads and SDK binaries are not
duplicated here; their hashes, sizes, generation metadata and deterministic
generator inputs are retained. `SHA256.json` covers every archived file except
itself. It does not make old evidence or a different revision authoritative.

Remaining: the DPR4 coverage wait/cost, admitted 100K/1M-object product scope,
66 prior native failures/not-built tests, formal SLO/statistical review, physical
input and other-platform evidence. No existing G5/P34 conclusion is overwritten.

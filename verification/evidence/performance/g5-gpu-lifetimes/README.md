# G5 navigation GPU lifetime evidence

Result:READY_FOR_CONTROL_REVIEW. This is the derived navigation GPU residency/staging slice,
not an official Gate,G5,P34 or physical-platform PASS.

Implementation:10f3ecd86f60f3dfffb407644af3921817dfcc58;paired SDK baseline:61ede04e819edf7f8a280465d9be842ea1ade860.
The approved coverage reuse result remains independently bound by evidence commit
3e2903475310a3a333d4a25c03f973c91473f61b and its prior evidence directory.

The same controlled worktree/branch was used throughout:
`/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`,
`codex/geometry-navigation-integration`. Normal SDK version remains0.3.9;this is
not a release/publication task. Root declarations/wrapper,Semantic/Skia locks and
old Gate records are unchanged. ON/OFF packages contain36 files without private
headers/archives. All normal input/state/receipt calls use the installed root SDK.

The renderer now pre-admits bounded completion metadata and preserves pixel
charges through actual GPU uses. Only a final publication callback shrinks the
same token from3RGBA buffers+metadata to1image+metadata. CPU raster can shrink
immediately. Eviction/cancellation keeps unfinished-use charges. A shared opaque
service retains the context independently of provider replacement;callbacks hold
only quota/state. Logical abandon/deleted handles do not masquerade as GPU finish.
Budget close seals future admission. Canonical scopes distinguish new snapshot
writes from prior navigation-image reads. Existing two GPU preparation batches,
128 contour quota,32MiB navigation and96MiB shared hard limits remain unchanged.
There is no new permanent RAF/worker,consumer API,geometry algorithm or renderer.

## Measured outcomes

Default dense corpus:636x923 CSS,DPR2,five2048-sample scribbles,two shapes,image,
table and sticky;11 canonical records. Selected/unselected pan/zoom,foreground
idle and5s CDP freeze were measured in isolated Chrome workers. Each selected
zoom main case waits60s in the foreground. A separate60s pair runs candidate
first,then baseline. There are additional same-corpus DPR3/4 ON comparisons.
No builds/tests ran during qualifying timing;workers retain180s/3GiB caps and
owned-process cleanup evidence. Renderer:ANGLE Metal on Apple M2.

| Default zoom metric | Before | After |
| --- | ---: | ---: |
| OFF restored submission gap median | 23.90ms | 19.55ms |
| OFF warm exact wait median | 239.10ms | 231.70ms |
| ON warm CPU coverage median | 57.90ms | 56.95ms |

All qualifying completed pairs have equal initial/final framebuffer hashes,unchanged document,
selection/operation state during navigation,matching camera return,qualified post-input
completion,zero fence wait timeout and stopped idle RAF/draw activity.

Navigation accounting below includes the whole corresponding ON trial. The old
charge retains3 buffers per published tile;the new charge reports actual live and
in-flight leases. It is not an OS/driver GPU-memory measurement.

| Case | Before ending charge | After ending charge | Evictions before/after | Builds before/after |
| --- | ---: | ---: | ---: | ---: |
|dense-pan|31.76MiB|13.72MiB|84/0|130/58|
|dense-selected-pan|31.76MiB|13.72MiB|109/0|155/58|
|dense-selected-zoom-pan|31.76MiB|13.20MiB|46/0|242/246|
|dense-zoom-pan|31.76MiB|12.94MiB|40/0|199/197|

Warm gap medians are not uniformly lower:OFF22.00->22.75ms,ON28.95->29.05ms.
The assessment retains this finding and first-selected-frame variability;it is
not statistical latency equivalence. Numerical protected guards remain satisfied.
Matched sharp-overlap/Direct pixel oracles pass,but time-weighted low-density
source fractions do not establish matched-pose optical equivalence. The DPR2
warm proxy rises about2 percentage points,DPR3 is nearly flat,and DPR4 remains
predominantly coarse. Continuous-navigation clarity is not closed.

Cold exact tails also remain open. The reverse DPR2 pair is694.4->776.5ms;
DPR4 is7226.2->7300.3ms. At DPR4 the2544x3692 framebuffer is35.83MiB;both the
retained screenshot and streamed composite exceed the32MiB gate. The captured
cold trace spends7044.8ms in CPU coverage after0.3ms cache preparation and full
reference fallback. `checks/g5-gpu-ready-next-preflight.json` binds that next-step
analysis. Bounded/resumable canonical fallback and canonical/navigation source
sharing take priority over formal100K/1M qualification.

## Correctness and provenance

The112-test native set passes107 with the exact same five pre-existing failures:
canvas_rf01_full_rebuild,canvas_fc02_shape_label_scene,G3HeadlessCanvasDemo,
FC00XRuntimeComposition and SketchSemantic. Seven real Chrome GPU modes,31 G5
Node tests and118 boundary contracts pass. Actual GPU provider replacement,
unsupported-completion fallback and deleted-handle discrimination are included.
See the raw outputs and mechanically derived comparison files for counts/identities.

Exploratory evidence is deliberately separate. Revision76ae71fc exposed a real
DPR3 implementation defect:optional navigation screenshot denial escaped as
std::bad_alloc;semantic generation15 applied while presented generation stayed14.
3306d78a fixes optional denial without discarding the canonical frame. The final
10f3ecd8 also corrects unread prior-snapshot attribution and protects new writes;
RED/GREEN budget tests preserve both sides of that contract. The final complete
DPR3/4 pairs pass. The early DPR4 probe changed whitespace during sampling and
is correctly BLOCKED_EVIDENCE despite functional checks;it is not qualification.
Readmask control runs include both33ms and18.4ms for the old implementation;
a single33->19ms pair is not presented as a universal improvement.

`source-inputs.json` and `consumer-artifacts.json` bind source/package inputs.
`comparison.json` and result.json bind all phase results,including unresolved
findings. `SHA256.json` hashes every included proof file. Raw traces are gzipped
with deterministic timestamps. Private GPU probes are not installed/exported.
Pinned primary SDK source references/hashes are in references;no third-party SDK
was rebuilt. Ambiguous context loss conservatively retains charge until its
budget owner closes. CDP freeze is not physical OS sleep/device eviction,and Web
results do not qualify Android/Windows ABI or physical platform behavior.

## Reproduction and interactive review

Use the locked SDK Store/Skia packages and recorded configure/build inputs. The
normal builds use`out/web-sampling` and`out/web-sampling-off`;the private GPU probe
uses`out/web-prepared-probe`. Corpus orchestration is recorded in
checks/g5-gpu-ready-corroborate.py and the checked-in dense_navigation_matrix.mjs.
The evidence compiler reads those durable raw outputs;it does not rerun a browser.
Run python3 verify_evidence.py in this directory to verify hashes,JSON payloads
and all qualifying worker limits/cleanup without signing a Gate result.

Current isolated default Demo:http://127.0.0.1:4226/example.html
Dense manual case:http://127.0.0.1:4226/mixed-navigation.html?dense=1
Both use out/g5-gpu-ready-off/js. Earlier4219/4220/4221/4222/4223/4224/4225
installs/pages were preserved. Normal and dense screenshots were visually inspected.

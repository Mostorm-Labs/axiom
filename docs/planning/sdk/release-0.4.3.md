# SDK 0.4.3 release

The user authorized merging codex/uiux-selection-handles through
`a39a1bdab97564b6e7ffc2b8c57ab9431b665462` and publishing a new SDK. Merge
`53193af8ed3f03f9f7b3647755a967e1bf0829e0` retains published main 0.4.2 and its
controller, Windows verification, bounded conversion and build-version fixes.
The feature branch and its worktree remain available.

This is a patch-level performance/correctness improvement for partial erasing
under cache pressure. RenderGroupManager owns derived append-only mask-prefix
comparison; existing SkiaRenderCache/SkiaRenderer own bounded scratch, ordered
painter work and retained-target validity. Canonical editing, identity, masks,
OperationEngine/History/publication and synchronization remain unchanged.

After a successful presented erase frame, an eligible persistent target can
repaint only conservative new damage using complete resident ink sources or
bounded scratch work. The full sampling grid and mask antialiasing are retained;
clipped feedback is never admitted as a complete source. A fixed-size validity
record rejects changed provider/surface/content/camera/scene/resource/present
identity, host clip/matrix changes, non-prefix/unsupported feedback and failed
presentation. Unsupported targets and legacy non-Compact ink use full replacement.
WebGL partial-update capability follows actual preserveDrawingBuffer. Allocation
or snapshot failure retries without qualifying incomplete output.

No public JS/DLL business API, ABI, transport, Operation/Snapshot schema or
compatibility version changes. Internal Render headers are not ordinary consumer
contracts. Source developer integration remains opt-in. All SDK/platform/Demo/npm
versions are 0.4.3; default32/96MiB budgets and accepted Semantic/Skia locks remain
unchanged. No dependency stack rebuild or new pixel/canonical owner is introduced.

Fresh local verification and exact-source hosted publication results follow below.
Historical full-suite failures (including the macOS provider-rebind path) and
BLOCKED performance records are preserved. Host-qualified motion budgets and
<=2-channel pixel tolerances are not public SLOs, physical input/IME latency,
all-device performance, full-suite or formal G5/P34/Gate acceptance. Actual
Windows external consumers and all six RN native targets remain separate hosted
release obligations; local Web success does not establish Windows ABI correctness.


Release preflight found implicit float/double and device-dimension conversions
in the added damage path. Explicit casts now express the same existing rounding
and arithmetic; finite-area rejection and full-replacement fallback are unchanged.
Strict shadow/integer-narrowing checks pass all four modified production TUs in
the native build. New-line float-conversion diagnostics are zero after the fix;
preexisting diagnostics were recorded separately, not broadly repaired or hidden
by changing project compiler flags. WebGL-only source is compiled in real WASM.

Fresh final local checks pass:40 selected native groups, including the expanded
G5RenderCache suite's9 brush/version profiles x2 affine cases x2 cache limits,
<=2-channel full-image oracle, prefix/target/clip/resource invalidation, failure
recovery and canonical preparation after damage. Logical-input/DPR pixels and
real native Session/Runtime calls pass with exact SDK_VERSION equality. Also
passed:39 G5 Node,15 controller Node,37 packaging,15 server/workflow tests, strict
consumer types,119 Runtime boundary contracts and offline Semantic Store reuse.

Matching rebuilt JS/types/WASM/resources pass actual Chrome external consumers,
16-source repeated erasing at DPR1/DPR2, existing live erase, affine erased-stroke
navigation at DPR2 and multi-stroke erase. Both16-source fixtures pass five atomic
sweep commits, pre-Up preview/commit equality, single Undo/Redo, sparse sweep,
cancel without commit,12 successive sweeps, heartbeat and post-erase pan/zoom
without document mutation. Steady motion p95<75ms and navigation p95<60ms are
host-qualified guards; cold first-feedback maxima remain recorded, not removed
from the samples or presented as solved universal latency. Full Windows SDK/Demo
consumer verification and six RN native targets/publication must freshly pass the
exact tagged source before completion is reported.

GPU lifetime preflight only; no implementation/authority freeze yet.
Continue AFTER61ede04e coverage-work-reuse evidence closes, as user explicitly
selected coverage reuse before GPU lifetime. No user authorization is missing.

Current Nav Job reserves3RGBA buffers+Tile/Job metadata; Tile inherits full lease
until eviction even after GPU done. Completion currently only atomic done;
ctx.checkAsyncWorkCompletion +2-batch backpressure exists. No lifetime split yet.

Pinned Skia SDK commit b6d106297ff9ef2ff8094033695d045e87775581 from lock/manifest.
Primary source reads (not dependency source build):
https://raw.githubusercontent.com/google/skia/b6d106297ff9ef2ff8094033695d045e87775581/src/gpu/ganesh/GrGpu.cpp
https://raw.githubusercontent.com/google/skia/b6d106297ff9ef2ff8094033695d045e87775581/src/gpu/ganesh/gl/GrGLGpu.cpp
https://raw.githubusercontent.com/google/skia/b6d106297ff9ef2ff8094033695d045e87775581/src/gpu/ganesh/mtl/GrMtlGpu.mm
https://raw.githubusercontent.com/google/skia/b6d106297ff9ef2ff8094033695d045e87775581/src/gpu/ganesh/mtl/GrMtlCommandBuffer.mm
GL finished callbacks are checked on submit/checkFinishedCallbacks and drained on
cleanup/abandon. Metal callbacks live on command buffers, last outstanding is
also pinned for no-op flushes; owner polls isCompleted then destroys buffers.
GrMtl destruction synchronizes as existing backend behavior; do not add waits.

Potential minimal design, MUST freeze new P31 and tests before mutations:
- ResourceBudgetManager reduce(id,newBytes), ResourceLease reduce method; only
  shrink an existing token without release/re-reserve. Mutex protects derived
  quota if completion callbacks ever execute on another backend thread.
- Private shared Nav allocation owns peak3-buffer lease, image+metadata portion,
  shared usage counters; final published batch completion shrinks to1image.
  Earlier batch completion must not shrink a still-active Job even if it later
  becomes published. Mark exact finalize pins in that final batch only.
- Pin budget allocations for every GPU use, including resident compose images,
  job writes, reused images, publication, cancellation and eviction. Cancellation
  can drop logical work but callback strong refs retain charges until complete.
  Tickets/usage never refer to destroyed Nav/Runtime/doc. No semantic receipts.
- Use RAII flush scope for early returns after GPU allocations/commands, before
  any compose. Pre-admit ticket/Pin capacity (recorded metadata, bounded by current
  entries+admitted initialization cap), so allocation failure cannot strand GPU
  use without a completion owner. Callback only changes derived lease/counters.
  Set done AFTER clearing pins/finalization (release/acquire ordering).
- Shared usage across Nav resets while same renderer/budget remains; retired old
  batches must still count toward quota. Changing explicit budget owner creates
  a separate ledger; old ticket retains old manager until completion.
- Admission uses actual total leases incl in-flight/cancelled, unchanged32MiB and
  hard96MiB/entry/GPU/time/query limits. Max resident demand derives from1image
  plus bounded staging; temporary denial defers until completion, never overbook.
- CPU raster publishes and shrinks immediately; GPU only finished callback, not
  submitted callback or canonical admission. Context abandon is discard, not a
  CanonicalVisible receipt.
- Avoid permanent idle RAF. During nav existing checkAsync polls. After exact
  GPU completion, common Runtime confirmation path can ask renderer to service
  derived retirements via validated provider acquire/checkAsync/release; do not
  expose GPU handles/business API. Need inspect actual confirm path names first.
  No need a new RAF-only cleanup scheduler if canonical completion polls suffice.
- Metrics distinguish resident cache image bytes, total charged allocations,
  staging, pending/retired in-flight and metadata. Keep sums coherent/threadsafe.
- Tests:shrink/no re-admission, stale/foreign token, concurrent quota, two fences
  (early not final), cancellation/eviction/cache clear/lost/rebind/provider/budget
  lifetime, no release before done, no terminal mutation, no idle draw/RAF leak;
  real GPU pending/completion and current pixel/frontier/quality/receipt oracles.
  Same112 native known5,7GPU,28Node,118boundary, normal packages and fixed dense
  matrix against61ede. High-DPR memory/clarity evidence with exact artifact IDs.

No files in runtime/resources have changed. This is review preparation, not a
claim that GPU residency/staging lifecycle separation is complete.

Further preflight:actual common completion path is
RuntimeCompositionCore::receivePlatformPresented ->
CanvasEditingRuntime::receivePlatformPresented (interaction cpp4939).
After its identity/qualified-feedback checks, a private SkiaRenderer retirement
service can acquire the current canonical provider, checkAsyncWorkCompletion,
release, without draw/present or altering receipt validity. The Web adapter
already polls its GPU fence then invokes this path and returns Retry for one
final owner opportunity; avoid inventing cleanup RAF scheduling.

Safer general callback design:thread-safe derived ResourceBudgetManager plus
shared allocation/usage ledger independent of Nav lifetime. Finished callback can
shrink/release leases even after Nav/cache clear; no raw Runtime/Navigation owner
refs. On GL and Metal pinned source, completion polling/drain is owner-driven,
but do not rely on that across future providers. Manager mutex and coherent
usage snapshot must avoid inverse lock nesting (never hold Usage while locking
Manager). A shared Nav usage ledger under SkiaRenderer survives clearRenderCache
with same budget; explicit budget-owner change may create a new ledger, tickets
retain old manager. This keeps cancelled/retired in-flight bytes counted.

Ledger stats should name image-allocation bytes (includes pending/retired pinned
images), current resident-cache image bytes, total allocation charges/staging and
in-flight remainder explicitly. Do not label all allocated images resident.
Snapshot stats coherently. Owner-only allocations can race only with callback
decreases, so quota checks stay conservative; saturate arithmetic.

RAII batch ticket pre-admission before any GPU allocation/compose is important.
Reserve bounded Pin capacity for current tile entries+two jobs+existing init
limit, charged as recorded metadata; callback ticket itself owns that charge
until completion. Dedup allocation pins, explicit finalization flag for exactly
published allocations. Scope destructor flushes nonblocking even on early return
once GPU work is recorded. No explicit new waits or glFinish. Callback sets done
only after pin/charge cleanup, so owner erasure never races a mutable Pin list.
Context-abandon callback is discard/resource retirement, never presentation proof.

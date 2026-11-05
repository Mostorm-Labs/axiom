# GPU resident images and staging lifetimes

Repository GitHub Mostorm-Labs/axiom, branch codex/geometry-navigation-integration.
Trusted ancestor:3e2903475310a3a333d4a25c03f973c91473f61b, materializing61ede04e
coverage reuse and its independently reviewable evidence. User explicitly chose
coverage reuse before GPU lifetime separation; that dependency is now measured.

Authority:architecture v0.4/current contracts from docs/notion/manifest.yaml,
existing Render/provider/resource and SDK consumer boundaries. State owner stays
shared Render, canonical document/receipt owner stays Runtime/OperationEngine.
No public JS/DLL API/ABI/version, schema or dependency-lock change is authorized.
This package concerns derived navigation allocations first, not every internal
Skia/driver resource. Source-density/cold exact findings stay open and measured.

Review:Nav currently reserves3RGBA tile buffers+metadata and moves that peak lease
into each resident Tile. The full charge survives GPU completion. GPU queue
backpressure already tracks completion of preparation batches, but resident image
uses during present/eviction need their own safe lifetime ownership. Shrinking a
lease on logical publication or CPU submission alone is unsafe.

Pinned SDK source b6d106297ff9ef2ff8094033695d045e87775581 was inspected at Google's
Skia repository:GrGpu.cpp,gl/GrGLGpu.cpp,mtl/GrMtlGpu.mm and
mtl/GrMtlCommandBuffer.mm. Finished callbacks follow prior GPU work; no-op/abandon
paths drain callbacks. These source reads do not authorize third-party builds.
References and exploratory notes are in out/g5-gpu-lifecycle-preflight/notes.md;
materialize exact references/hash evidence with the result.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required_changes:separate the live image+metadata reservation from temporary
  staging after actual GPU completion. Add an in-place shrink-only derived-budget
  operation,never release/re-reserve a token to shrink. Make derived quota access
  safe for completion callbacks; never mutate Runtime/document in a callback.
- required_lifetime:shared private allocation/usage ownership independent of Nav
  object lifetime. GPU uses (job writes,reused images,composition,publication)
  retain charges through their completion. Only the final publishing batch may
  shrink staging; earlier batch completion cannot shrink a live job. Cancellation,
  eviction,cache clear and provider loss must not free unfinished-use charges.
  CPU raster may retire staging immediately. Callback/discard is not a receipt.
- admission:unchanged32MiB navigation/default96MiB shared hard limit,maxEntries,
  entry/time/query/step and128 GPU contour quotas. Track active and retired
  in-flight charges. More images may fit only after staging is actually retired.
  Denial defers/falls back,never overbooks or manufactures valid coverage.
- completion:bounded charged metadata/pins and nonblocking finished callbacks;
  preserve the existing two preparation-batch backpressure policy. Read-only
  feedback lifetime tracking must not block navigation behind optional preparation.
  Use RAII for early-return GPU work. Unsupported nonblocking completion capability
  must retain conservative ownership or fail closed,never add production waits.
  Common validated platform-completion servicing may drain derived retirements;
  no permanent idle RAF/worker loop or new public GPU/Skia handle seam.
- forbidden:canonical mutation/identity/ARC receipt changes,second renderer,
  Brush/AA/raster clip/grid/order changes,larger limits,early staging release,
  synchronous GPU waits/glFinish,new pools,SDK/dependency production,old Gate edits,
  user-page/document mutation. Preserve4219/4220/4221/4222/4223 installs/pages.
- required_tests:RED/GREEN shrink/grow/stale-token and concurrent quota behavior;
  early versus final completion,ticket lifetime/cancel/evict/clear/denial,budget
  owner and provider/lost/rebind generations,no release before done/no leak;
  actual Chrome GPU pending/completion,pixel/frontier/quality/receipt checks;
 112 native with exact5 known-failure comparison,seven GPU modes,28+ Node and
 118 boundary checks,normal36-file ON/OFF package/consumer/UI smoke.
- required_measurement:paired same dense matrix against frozen61ede04e ON/OFF
  out/g5-reuse-final-{on,off}/js,selected/unselected pan/zoom,warm,idle/freeze,
  reverse selected60s,exact source/package/raw/cleanup identities. Add bounded
  corroborative DPR3/4 observations and inspect source-density/lease accounting.
  No builds/tests during timing;180s/3GiB owned workers. Missing/impossible physical
  evidence stays blocked,not inferred from an adapter or quota arithmetic.
- performance_oracle:preserve restored default zoom submission gap<=150ms,warm
  exact wait<=300ms and ON CPU coverage<=80ms,no repeatable navigation regression.
  Demonstrate peak->resident charge reduction only after completion,better useful
  residency/quality under the same budget;report all quality/cold-exact tails.
  Density is a source proxy,not optical same-pose or physical latency proof.
- evidence:locally committed implementation and standalone proof compilation in
  verification/evidence/performance/g5-gpu-lifetimes/,including negative trials,
  primary SDK references and exact artifacts. Separate new demo after clean build.
- terminal_success:all required oracles/evidence,READY_FOR_CONTROL_REVIEW for this
  slice with explicit remaining findings;then preflight complex-editing/scale.
  No official Gate/G5/P34/platform PASS.
- terminal_blockers:source/identity drift,missing locked tool/artifact/browser,
  authority/ownership/verification conflict,new correctness/resource regression or
  failure to improve after systematic investigation. Preserve exact work/evidence
  and return to the owning layer;do not relax constraints to label success.

Implementation preflight must settle bounded completion-ticket ownership and
nonblocking provider capability before GPU-path mutation. Semantic/API decisions
are not missing. Begin with shrink-only resource quota tests,then allocation and
completion tests before adapting renderer production flow.

GPU-path design reconciliation before mutation:
Use manager-owned GPU epochs instead of per-allocation pin lists. Pixel-backed
reservations (Raster/Scratch/Tile/Navigation/new NavigationTile class) used by a
scope,including new reservations,remember that scope's epoch. Releasing a lease
retains its charge until completed GPU epochs cover its last use. A monotonic
completion watermark handles out-of-order contexts conservatively. CPU Recorded
metadata is not GPU-pinned. This also protects canonical scratch/snapshot uses
and early-return releases without a second allocation/cache owner.

A final publishing callback carries a fixed shrink permit,applies it before
completing that epoch,then retires pending releases. Earlier callbacks cannot
apply its permit. Navigation used/staging/pending-release metrics come from this
single manager ledger. Add a resident floor to its reservation,so peak3RGBA may
shrink to1image+metadata after completion. CPU publishes shrink immediately.

Bound pending epoch records/tickets at1024(default2*512 cache entry envelope),
with charged ticket metadata before GPU work; keep the existing two preparation
batches and128 contour quota. Read-only feedback lifetime records are not new
preparation work. No per-frame resource pin vector or raw Runtime owner pointer.
Callbacks hold the budget owner independently of renderer/cache destruction.

Extend only the source/internal provider contract with nonblocking completion
capability and owner-thread service(no draw/present/readback,editing,reentrancy
or borrowed GPU handle return). Default is unsupported/no-op. WebGL2 forwards
its existing context check. Registry services active and inactive providers,
including preview,at common owner opportunities/qualified completion; renderer
prunes finished ticket metadata. Thus inactive contexts progress without a new
idle RAF. Unknown GPU providers use the conservative Direct cache fallback;CPU
raster does not need the capability. Normal JS/C DLL consumer ABI stays unchanged.

Required RED tests now include epoch begin/end/duplicate/stale behavior,release
held before completion,early versus final shrink,out-of-order contexts,CPU
metadata immediate release,bounded epoch backlog and quota snapshots. Source
provider additions must document lifetime/thread/error behavior and be covered
by existing provider/surface/idle and exported-root package verification.

Context ownership reconciliation:the renderer's retirement queue retains an
opaque shared completion service independently of a replaceable provider. The
WebGL service owns its Ganesh context; callback tickets retain only quota/state,
not the context (avoiding self-cycles). Actual physical loss may discard charged
storage. Logical abandon retains orphan charges,until a sealed budget owner
closes;close denies all future admission. Renderer services all retained contexts
at existing owner opportunities,including inactive/replaced providers. WebGL2
capability is validated from actual context attributes. No new idle loop.
Ticket rows use charged bounded list nodes,not an uncharged retained vector pool.
The render cache remains private;normal root SDK/DLL contracts do not change.

Selective-use clarification:GPU epochs tag new pixel reservations immediately;
existing navigation tiles are stamped only when sampled or resumed as a job.
Unrelated eviction candidates can return completed storage immediately,without a
new frame pinning the entire cache. Canonical pixel classes retain conservative
scope marking. A deleted Emscripten context handle is ambiguous logical abandon,
not proof of physical GPU completion; retain orphan charge until owner close.
DPR2 remains the default paired matrix;validated AXIOM_NAV_DPR selects1..4 only
for additional corroborative measurements. No third-party build/SDK flag change.

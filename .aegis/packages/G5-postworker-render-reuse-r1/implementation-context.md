# Post-Worker Render reuse: current execution context

Role: implementation producer; control owns scope and independent review.
Authority: authority.lock.json, human-requested slices2/3, existing Runtime and SDK contracts.
Objective: reduce measured transient full-scene replay and canonical repeated/irrelevant replay while improving immediately observable editing/navigation continuity.
Non-goals: more threads, legacy-union migration, other brush support, main integration, releases and broad platform/scale qualification.

Current anchor is ced1c27c on codex/g5-cpu-worker. The HEAD/index tracked files are clean; unrelated untracked historical results must be left alone. CPUW7 is already committed/pushed; do not reproduce its checkpoint-scratch fix. Main integration was assessed only and is not part of this package.

CanvasEditingRuntime freezes/queues accepted intents and owns contact lifecycle/order. OperationEngine applies canonical Operations atomically. SceneCompiler derives the scene. SkiaRenderer owns the cache, disposable display targets and GPU completion/retirement. The CPU Worker transports bounded original C++/WASM geometry jobs only. Public root editing contracts remain unchanged.

Known expensive flow: renderTransientOverlay() -> prepareBodies()/prepareWorkingSet() or drawTransientScene(), then disposable snapshot/publication; persistentErase damage reuse is conditional and staged paths do not automatically use it. Canonical SkiaRenderCache::prepare() resumes admission, entry and CompactStrokeRasterJob state. Do not assume repeated attempts mean resets: classify preserved useful steps, irrelevant spatial replay, repeated dependency verification and actual invalidation separately. skia_ink_layers and existing ordered run/damage reuse are capabilities to extend, not replace with another renderer.

In W7 diagnostic C: transient call ~9.593s; canonical calls ~6.373s; stamp ~1.357s. Canonical617 attempts/162334 steps/7.327M replay inputs. Body builds/compile0 is compatible with substantial replay. Raw76 Worker envelopes total178.7ms including149.5ms compute, not tens of seconds; do not shift blame to transport/GPU without evidence.

Questions to resolve before behavior edits: which transient path ignores valid retained content; which canonical components are replayed outside the demand or redundantly across attempts; what exact source/camera/mask/context proof makes reuse legal; what still needs drawing when zooming out exposes nonempty content. Reuse the W7 diagnostic and focused/native tests first. Record a concise causal design and RED, then continue to both Render slices without waiting for control acknowledgment.

Required analysis: exact current owner/prepare/render/publish/failure path; invalidation dependency map for body/mask/pixels/ordered composition; useful work and resumable cursor audit; no blank/stale/partial output under failures.
Required output: scoped source/test change, compact result with actual primary/C/outward-feedback timings, original semantic/pixel/progress checks and fresh candidate assets only if qualified. 3s/5s are bands, not automatic closure. If no end-to-end benefit, report that and preserve default4331.
Quality/evidence: verification.lock.json RR1..RR4 and workload.json finite windows. Preserve original masks, z-order, alpha, grid, AA rule, actual receipts and original full primary input/capture order. Never wait for settle before immediate navigation probes. Mixed fixture includes chalk/crayon/watercolor and deliberately exercises legacy synchronous erase; report actual mode.
Handoff: return to control thread01a1c344-e48c-7db3-8c35-39732cb81dc5 with result+SHA and proactively notify it on completion/blocker. Human authorized bidirectional pairing. No routine CLEAR/RED/native-green control round-trip.

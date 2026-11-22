# Scoped Render implementation

DS0 establishes two separate paths. Erase Down enters full transient mapping;
v5/v6 misses synchronously enqueue procedural coverage, then Ganesh flush CPU
API dominates the callback. After Up, masked v5/v6 canonical admission chooses
Direct, also bypassing cooperative preparation. Hardware GPU time is unknown.

Keep the existing mapper, per-device tile grid, pressure shaders, ordered
composition and mask evaluators. Extend the existing PreparedGeometryCache
metrics job to v5/v6 (32 knots per advance). Extend SkiaInkLayers with retained
pressure coverage produced by the existing CompactStrokeRasterJob. This is
white body coverage only, before material, opacity and historical/live masks.
Canonical and transient preflight it before touching their provider. Their
existing synchronous mapper then consumes ready coverage; a missing tile is
pending, never a hidden full replay. Null cache/prepared reference stays separate.

One aggregate PreparationAllowance counts actual pressure commands (128 per
owner invocation), time and existing steps across all bodies/tiles. Pending
keeps the exact raster cursor and completed tiles. Flush bounded offscreen work
without a CPU fence, preserve last successful visible frame, use existing retry.

Coverage identity includes pinned complete body identity (content/config/seed),
exact matrix, original grid, context, target format and surface/resource owner
basis. Mask changes do not invalidate body coverage. New camera requests may
reuse only an identical sampling identity. Target/resource changes clear it.
Sources remain charged; tile leases transfer from CompactCoverageTargets rather
than charging a borrowed image as a fresh allocation. Pending jobs retain their
body and leases; cancellation/supersession releases their storage through the
existing GPU retirement owner.

One pressure cache entry is one body/sampling basis, with bounded tiles. The
unchanged maxEntries applies to these body entries; maxEntryBytes applies to all
their tiles plus metadata, maxBytes to all body entries plus existing ink layers.
The 811 device tiles in the reproduced scene are not 811 separate stroke cache
entries. No budget, SDK/API or brush/AA policy changes are needed. Oversized
optional demands retire pressure residency and enter the existing ordered
staging path; they do not become document limits or synchronous CPU replay.
The staging latch uses the same demand count as prepareBodies, preserving
the pending cursor across retries. An incomplete capture immediately detaches
its pixel lease from the coverage pool, before any other producer can borrow it.

Historical pressure material masks use the existing NavigationMaskPlan producer
and replay cursor on the pending scratch/composite. Body completion and mask
completion are separate. At most 32 mask draws per step and 128 per aggregate
invocation are admitted; no completed body is cleared while a mask is pending.
One-step preparation, full ordered destination-clear semantics, target invalidation
and null-cache reference remain unchanged.

r3 disposition: selectively use its metrics/group culling and pressure producer
mechanics after current actual-owner RED; do not import its alias proof,
navigation exposure, source/SDK/Interaction or presentation changes. Its short
callbacks and roughly 8s recovery remain historical findings, not this result.

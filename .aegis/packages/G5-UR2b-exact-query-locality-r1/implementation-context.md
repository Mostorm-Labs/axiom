# UR2b exact-query locality

The previous UR2a source/receipt/mask-isolation repair is independently accepted
and now published; start from the exact publication receipt in authority.lock.
Do not restore old shelved UR2. Control designs/reviews; adjacent implementation
is the sole product/index writer. This slice is CPU exact-query reuse/locality.

Actual owner path is CanvasEditingRuntime.eraserFinish -> partialEraseTargets
-> SkiaRenderer.intersectEraseGeometry -> EraseMaskPathCache.query ->
eraseGeometryQuery -> geometry(record,false). Currently geometry unconditionally
validates every CompactStroke knot, compiles full v4 contour, builds every ribbon
and cap before the query can reject irrelevant components. Mask paths are cached,
but body preparation is not connected. Existing shared PreparedGeometryCache has
qualified immutable bodies, path groups and bounded compiler checkpoints for
rendering. Reuse it; add private exact iteration/validation proofs, not a new cache.

Keep object-local mask geometry, independent historical-mask ownership and the
original lazy failure/no-op/Unsupported behavior. Unknown content must remain
fully validated; no ObjectId/hash-only proof. Existing CPU component fallback
and separately frozen preoptimization binary own query truth; GPU/LOD/bounds
cannot decide a semantic hit. No Scene/mask algorithm/thread/transaction rewrite.

Resolve implementation design and independent batchoracle/probe/corpus schema
before routing changes. Qualification-only actualcost counters can first expose
the old fullcompile despite a resident drawbody (real structuralRED). Run full
required native/SDK/correctness and all paired timing workers within the exact
budget. Readback/profile is separate from normal timing. No commit/push. At READY
or BLOCKED actively notify control with exact artifacts andremaining findings.

# UR1c1: shared prepared body owner and hot lookup index

Accepted UR1b source d04e31f808d76d0d2f7500c15e81cce2a7cda6a9a69ff8bd5480f1bfa240caa0. Its control review1b9c2d41 accepts the scoped real whole-v4 canonical/transient decision and actual target truth. UR2isstillquarantined. Keep4284normal SDK and oldDemos unchanged; no commit/push.

Current facts: PreparedGeometryCache has per-ObjectId entries, a cross-entry complete ReferenceCommand equality scan and no remembered alias entry on a cross-objecthit. It calls SourceRetentionPool.get before lookup; SourceRetentionPool computes source/chunk footprint before checking retained ownership. Navigation tiles and SkiaInkLayers each own a separate preparedcache, although their source-retention pool may already be shared. SkiaRenderer is the existing assembly/resource owner.

First complete just this foundation: share one existing PreparedGeometryCache under the renderer for the existing navigation/retained-ink consumers and replace repeated hot identity/content scans with bounded, fully verified indexes and remembered aliases. ImmutableValue.alias supports strong ownership sharing, but keys must distinguish aliaspointee/type as needed, preventABA and verify different-allocation equal-content completely. Fingerprints only index; no semantic ID/schema changes.

Body transforms/masks stay independent. Retain source allocations and charge geometry/index metadata once per actual owner, not once per alias; dropping one consumer cannot invalidate another. Shared replayOutput is owner-thread scratch: no parallel jobs/threads. Unknown source footprints still validate; only proven immutable identity may reuse a known footprint.

Do not make a longfirstdraw worse by plugging the synchronous existing get/build into every new canonical/Direct leaf. Ordinary cooperative canonical full-prepared-body integration is the subsequentUR1c2 slice. Here wire and prove only currently prepared consumers and hotlookup benefits. Keep explicit futuregap instead of calling allUR1c done.

Native baseline/RED:7199knots,1/8/16/32copies and48warm queries; mechanism work counts separate coldadmission, pointer/ownerfast hits, fingerprint bytes and deep comparison. Independent pixels/recordforks/hashcollisions/alias/eviction/quota tests. Two functional workers qualification+normal use sameS0/S2, plus settledsingleerase/navigation prepared consumer; noUR2immediate feature. Failures preserve all cold bytes and existing independentAAoracles.

Output .aegis/results/G5-UR1c1-prepared-body-index/result.json, exact source/artifact/routes/counts and newportDemo. Notify control01a1b6f3-e69a-7fe2-8d9b-0222e8741f03 at readyor explicitblocker. Do not waitfor fullUR0.

Publication reconciliation: user explicitly asked the implementation thread to commit/push the acceptedUR1a/b work. Current HEAD3437b5c09ebf6de938e596dcb2780e612e1c8c37 is a verified descendant of14ce, with acceptedUR1b source bytes unchanged. Use3437b5 as anchor/cursor. Old d04e31 is the historical exact result identity, not an equality assertion for a source tree that now includes published metadata and this new package. Preserve accepted product bytes; do not repeat prior execution or reject just becauseHEAD advanced.

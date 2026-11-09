# UR1c1 current private Render contract

The existing SkiaRenderer owns one PreparedGeometryCache shared with the same SourceRetentionPool and ResourceBudgetManager. Existing navigation tiles and retained ink layers receive this service. A consumer clears its own pixel/source state without clearing the owner service; renderer owner reset and budget-owner replacement retain their explicit reset rules. No new SDK capability, semantic identity, document owner, mask algorithm or synchronous ordinary canonical/Direct preparation is introduced.

The retained alias key contains strong immutable storage ownership, aliased pointee, type discriminator and command discriminator. Known identity hits precede fingerprinting, complete equality and source footprint traversal. Unknown content uses a bounded first/middle/last fingerprint only to select candidates; complete content equality remains the sharing proof, including config, seed and knots. Transform, masks and device pixels have separate existing validity rules.

Aliases and bodies use the original maxEntries limit; index metadata is positively reserved under the existing resource budget. Distinct retained source allocations remain charged even when equal geometry shares. Alias geometry references are weak, and old live consumers retain source/body leases under eviction or owner replacement. Two fixed initial metadata reservations stay with an in-flight body; later aliases remain independently evictable. Source owner/view nodes are individually charged and removed at last release; no new1024-owner cap exists. Owner-thread replay scratch remains private to the same render owner.

Counters distinguish actual get/ordered-index operations, known identity hits, bytes actually fingerprinted, complete-equality invocations, candidate checks, actual footprint lambda/chunk visits, compilation and source/geometry/index quota accounting. Index queries do not claim measured tree comparisons, complete-equality calls do not claim visited bytes, and index reservation bytes are conservative bookkeeping rather than allocator RSS or measured copies. The service pointer in qualification records is diagnostic within one worker, not canonical identity or an across-process key.

The old/new native matrix keeps its raw component timings and performs no extra content walk merely to count work. These timings do not establish browser relative speed or an overall performance verdict.

Only AXIOM_ENABLE_FRAME_TIMING qualification builds emit bounded per-root prepared aggregates. Normal SDK output contains no prepared/execution trace records in either case set. Actual WebGL2 and recordingContext coverage are observed; Skia internal CPU preparation remains UNKNOWN.

Actual native Metal navigation followed by full transient retained-ink shares one body at DPR1/2/4, preserves transformed/transparent/masked mixed-scene output against independent Direct pixels, and survives consumer invalidation. In the fixed Web cases, navigation gets are observed but retained-ink gets are not. Therefore this is an explicit U1C1-06 blocker, not a complete Web sharing proof.

Remaining scope is unchanged: UR1c2 ordinary cooperative canonical/Direct prepared consumption; UR1d resource/recovery closure; UR2 post-Up, immediate re-erase and navigation handoff.

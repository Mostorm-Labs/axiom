# Pending-release arbitration

Stable Render code is retained after the material experiment. SkiaRenderer handles actual critical denial by dropping navigationCache_ then immediately asking SkiaNavigationTiles to reclaim against total used bytes. Released GPU leases remain charged;the future release can already satisfy the minimum while this path evicts additional cells.

Keep the existing source-before-cell priority,but first wait if existing pending release suffices. After retiring the optional whole frame,recheck pending quota before reclaiming cells. Nav mandatory victim selection uses future charged usage only as a hint;reserve still sees all queued charges. Optional replay remains unchanged.

Composition/state owners,source/target/full generation stamps,FramePlan,Brush and OperationEngine contracts are unchanged. Add no new public API. Shared Render internal helper may deduplicate scheduling-only projection. No new resource pool or global soft-cap semantics.

ImplementationDesignPreflight:current prepare/deny/retire/reclaim/yield/poll/reserve flow is retained;only eviction decisions account for existing pending release. Abort/lost never fabricates completion. Tests bind both Renderer and Nav owners with real GPU objects and delayed actual completion. unresolved_semantic_decisions:[];authority_conflict:false;verification_conflict:false;package_scope_conflict:false.

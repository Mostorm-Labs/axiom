# Implementation Design Preflight

State owner:SkiaRenderCache owns admissions,ordered offscreen composite,source validity and preparation cursor. CompactStrokeRasterJob owns canonical coverage progress. ResourceBudgetManager plus SkiaGpuLifetime own resource admission/retirement. SkiaRenderer/provider owns completed presentation and receipts. No Platform/SDK changes.

Prepare:validate FramePlan/current provider;freeze source/target/camera stamps;classify only eligible overflow ink;admit one composite and conservative material/coverage working headroom. Step:compile/replay coverage without a new clip;reserve transient layer;save/reset/clip integer material region;saveLayer with null paint/filter;check actual top device;set original matrix;run unchanged material helper;restore twice;then advance tile cursor. GPU charge survives logical layer release.

Publish:all ordered entries must complete before immutable frame/present. Abort:denial/lost/stale/failure does not advance material tile/canonical receipt;cancel discards derived progress using existing cleanup. No layer spans a yield.

Eligibility is frozen in authority.lock.json. Material-only clipping does not alter coverage raster clip. A pixel mismatch is terminal verification failure,not permission to change AA or tolerance. A Ganesh layer-allocation NoPixels device loses its recording context and must yield;Runtime quota denial is injected and tested. Physical GPU-OOM is not injected;late driver failures retain existing provider/loss behavior.

Obligations capacity/parity map to admission+material helper and Direct pixel oracles;lifetime maps to reserve/restore/epoch tests;regression/performance map to unchanged installed root SDK and frozen worker corpus.

unresolved_semantic_decisions:[]
authority_conflict:false
verification_conflict:false
package_scope_conflict:false

## P31 qualification correction

The native characterization rejected the initial all-backend assumption. Scope is now Ganesh GPU only;CPU keeps the original path. This is a package correction before final implementation,not a relaxed pixel oracle. The same8 geometric fixtures must remain exact on both backends;the constrained5MiB capacity obligation applies only to qualified GPU. Earlier experimental evidence remains durable and is not marked PASS.

Mandatory quota mapping:SkiaRenderer owns the navigation alias and reclaim callback. Before dropping it,account already-pending GPU releases and reclaim disposable world tiles. A failure caused by retiring transient material must yield for completion. Add a real GPU delayed-poll test:pending exact work cannot discard the still-qualified sharp navigation frame or emit a canonical receipt. This completes the existing retained-frame/lifetime obligation;no new performance finish line.

## Working-window mapping correction

The complete paired4a092334 performance matrix fails the frozen cold/continuity oracle. DPR4 after-freeze has503 exact attempts versus75 baseline despite a7.8ms longest attempt. Single-layer headroom produces one tile perRAF. The existing mandatory-resource obligation must reclaim a bounded preferred working window after actual denial,not just the next layer. SkiaNavigationTiles must count logically released GPU bytes as future capacity when selecting victims;actual admission remains exclusively ResourceBudgetManager.reserve. This is a private owner/method mapping correction for the frozen performance/lifetime obligations,not a changed acceptance threshold or resurrection of optional replay. Preferred headroom cannot retire the sharp whole-frame source or useful frontier.

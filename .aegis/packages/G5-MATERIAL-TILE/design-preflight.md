# Implementation Design Preflight

State owner:SkiaRenderCache owns admissions,ordered offscreen composite,source validity and preparation cursor. CompactStrokeRasterJob owns canonical coverage progress. ResourceBudgetManager plus SkiaGpuLifetime own resource admission/retirement. SkiaRenderer/provider owns completed presentation and receipts. No Platform/SDK changes.

Prepare:validate FramePlan/current provider;freeze source/target/camera stamps;classify only eligible overflow ink;admit one composite and conservative material/coverage working headroom. Step:compile/replay coverage without a new clip;reserve transient layer;save/reset/clip integer material region;saveLayer with null paint/filter;check actual top device;set original matrix;run unchanged material helper;restore twice;then advance tile cursor. GPU charge survives logical layer release.

Publish:all ordered entries must complete before immutable frame/present. Abort:denial/lost/stale/failure does not advance material tile/canonical receipt;cancel discards derived progress using existing cleanup. No layer spans a yield.

Eligibility is frozen in authority.lock.json. Material-only clipping does not alter coverage raster clip. A pixel mismatch is terminal verification failure,not permission to change AA or tolerance. A layer-allocation NoPixels failure is observed by the same public Skia APIs on CPU/GPU;late driver failures retain existing provider/loss behavior.

Obligations capacity/parity map to admission+material helper and Direct pixel oracles;lifetime maps to reserve/restore/epoch tests;regression/performance map to unchanged installed root SDK and frozen worker corpus.

unresolved_semantic_decisions:[]
authority_conflict:false
verification_conflict:false
package_scope_conflict:false

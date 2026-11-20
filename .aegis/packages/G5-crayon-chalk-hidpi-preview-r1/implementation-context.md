# Wax/chalk high-DPR preview, scoped implementation

User authorized this package; Control designs and reviews, coding chat 01a1c345-6500-7ea0-984d-707b72878bd1 executes and reports to 01a1c4c7-7796-7380-9f3e-bd0095fc9372. Start from 5c26fdee053c7ee7eee901a885ac9ccd4a3c3f1c in a new isolated worktree. Repository identity first.

Current flow: public SDK pointer facts -> existing CanvasEditingRuntime -> BrushSession/CompactBrushSession stable immutable chunk chain plus replaceable tail -> PreviewSurfaceController -> SkiaRenderer::renderPreview / CompactCache -> Skia GPU Surface -> present; Up commits through original OperationEngine and only matched CanonicalVisible retires preview.

Current preview has GPU for algorithms5/6, but a fixed16MiB surface cap and32MiB aggregate gate cause full chain concatenation and full stroke draw above that cap. At1431x1010logical DPR2 oneRGBA is23,124,960bytes and DPR4 92,499,840bytes. Existing configured512/1024/128MiB budget can admit bounded reuse. Do not change shader/brush model or merely raise an uncharged constant.

Render owns all new cache/resource policy. Prefer compatible full surfaces if affordable, using real ResourceBudget leases and immutable snapshot/COW/lastuse proof; allow local/tile scratch only with exact original coordinate/order/coverage semantics. Ensure session/document/seed/context/provider/surface/DPR/camera identities and stale-tail replacement invalidate correctly, warm retries do not replay fullhistory, failure never publishes incomplete frame, and memory-pressure recovery makes progress. DefaultOFF private counters may record real work/bytes/targets.

Read package then execution-contract/verification/workload/metrics. Freeze baseline driver/probe/oracle before B; actualGPU native RED must observe repeated past work above16MiB. Existing InkV2RetinaPreview only tests1000x650DPR2 (~10.4MB), below the currentcap. Original verify_crayon_latency waits2RAFs perpoint, unsuitable for ordinary cadence. Use controlled rootSDK consumer/noforcedframes. Qualify native smallRaster material exactness and realWebGPU highDPR, with screenshots in a separate lane. Follow frozen finite resources and return policy; no intermediate ACK.

Protected4348/4349processes are currently absent (hostPIDs from priorturn no longer exist); preserveactualbefore state and immutableassetfiles. Do not inspect/reload user's IAB. Start own normalpreview firstfree>=4350onlyafterCfunctionalrootSDKsmoke and notifyControl, continueallremainingwork. No commit/push/tag or mixedbrushasyncerase work. OldEFF4remainsopen.

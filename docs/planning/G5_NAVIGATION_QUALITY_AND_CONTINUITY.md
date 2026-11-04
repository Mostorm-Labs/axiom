# Navigation quality and continuity, combined second slice

User-authorized continuation in GitHub Mostorm-Labs/axiom, controlled branch
codex/geometry-navigation-integration. Anchor (ancestor):
3906372c74ade39b85079c0394d17df4b3972b8d. Consumes the existing G5 world navigation,
exact-contour and Runtime/Scene/Skia contracts. This joins cold-frontier/lifecycle
continuity with the reported blurry trackpad navigation; it does not replace
semantic, Brush, SDK or Gate authority.

Owners remain: Runtime camera/Scene queries/receipts; Render derived sources,
quality selection, cooperative jobs and residency; Skia drawing and platform
provider lifecycle/present. No consumer JS/DLL API change. Current source paths:
SkiaRenderer::renderNavigation -> SkiaNavigationTiles, and cooperative exact
preparation through SkiaRenderCache/CompactStrokeRasterJob.

Diagnosis: complete coarse coverage currently wins before retained canonical
pixels. Parent selection may reduce the entire viewport to four cells. Retained
pixels cannot contribute a partial region when new content enters the viewport.
Fine work is cancelled when a new uncovered frontier appears. Exact preparation
also clears world tiles/sources wholesale to obtain conservative headroom.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: preserve matching clean-anchor ON/OFF SDK artifacts; establish RED
  pixel oracles for sharp retained content replaced by coarse tiles, and sharp
  overlap plus a nonempty cold frontier. Keep source identity and all tails.
- required: choose the best valid source per region, with a bounded disjoint
  coverage partition. Retained canonical pixels have their original world bounds
  and texel density; unknown outside regions require valid tiles or Runtime
  empty-background proof. Fine ready content must not be replaced by coarser
  content at the same camera. No incomplete target present.
- required: prioritize missing visible coverage, then visible resolution debt;
  preserve applicable fine-job progress during frontier service with at most
  one additional suspended job. Keep time/step/GPU/query bounds and generation,
  source/resource/surface checks; discard obsolete work safely.
- required: replace blanket exact-time eviction with actual optional-record
  admission reclamation. Evict rebuildable offscreen/coarse/old residency first,
  only as needed under existing budgets; retain complete-image feedback. Denial
  keeps the existing exact fallback and canonical pixels/receipts.
- required diagnostics: presented source-resolution low-area fraction, maximum
  device-pixels-per-source-texel, retained-area fraction, and time to adequate
  resolution. A private1.25x density threshold classifies resolution debt; it is
  not an optical/physical SLO. Coarse >2x feedback is temporary fallback for
  regions with no better complete source, never an override of sharp content.
  Old diagnostics without these fields remain unavailable, never invented zero.
- blocking native/real-GPU oracles: sharp-overlap pixels; full cold-frontier
  coverage and eventual adequate refinement; zoom-level changes; applicable fine
  job progress under small repeated camera motion; cache retention, budget denial,
  reclamation and cancellation; unknown exposure/source failure and stale stamps;
  exact Direct pixel equality, no navigation canonical receipt/ARC retirement.
- blocking Web evidence: matching public-root-SDK before/after mixed nine-object
  and offscreen-content routes; selected/unselected, pan/zoom across levels,
  repeated/idle/freeze recovery, actual submission gaps and final pixels/state/
  receipts. Default/timing builds remain distinct. Optical ROI readback and
  screenshots run separately from latency sampling and are labeled intrusive.
  Quality improvements must not knowingly introduce a repeatable material
  continuity or ordinary-exact regression; investigate and retain outliers.
- checks: focused Render/Runtime/cache/source tests, Node metric tests, real
  WebGL quality/parity modes, public install/boundary checks, full configured
  native and Node regression with exact baseline failure names and tooling limits.
- forbidden: larger frame/GPU/resource budgets, alternate Brush geometry or
  renderer, new public debug/SDK access, canonical approximation/early receipts,
  dependency rebuild/lock or semantic/snapshot edits, old Gate changes, user Demo
  reload/document replacement, unrelated Scene/index/large-scale refactoring.
- evidence: committed local source/input/package SHA256, RED/GREEN logs, raw
  comparison and quality results, findings and a separate runnable Demo. Hosted
  CI and physical platform qualification are optional, not inferred from host.
- terminal_success: all applicable implementation, bounded proof and evidence
  closure with explicit remaining findings; READY_FOR_CONTROL_REVIEW only.
- terminal_blockers: authority/oracle conflict, missing locked toolchain/artifact,
  source drift, new correctness regression, or no improvement in the frozen
  navigation-quality reproduction. No official G5/P34 or physical PASS.

The original cold-frontier/level/eviction/idle/selection cases stay in scope.
Sharpness adds a missing dimension to that same policy, rather than starting a
separate renderer or relaxing the original continuity/canonical requirements.

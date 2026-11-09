# UR1c2 working-set repair r1 — READY_FOR_CONTROL_REVIEW

Implementation candidate only. The independent control review owns acceptance; no whole UR1/G5/P34/performance PASS is issued.

Controlled source: `codex/geometry-navigation-integration`, HEAD `e69f2377cd93ddbe89d1fb1227ae8397c977b287`, accepted dirty C1/C2 plus this frozen P36 repair. Source tree: `9eb88bcc7c25d7647f90375430ba6d109f196084023d0111f4dc40180dbb757a`. No commit/push. Old C2 result, bound SDKs, source and delivery payloads are verified unchanged.

## Change and ownership

- Same renderer-owned PreparedGeometryCache, SourceRetentionPool and ResourceBudgetManager. Qualified ready dependencies retain complete owner+pointee+type+command identity and positively charged metadata through actual canonical/Direct use. They do not form another compiled-body cache. The demand reference count is capped by the existing512-entry setting.
- Requests exceeding discoverability/residency use existing ordered composite/scratch and CompactStrokeRasterJob staging. One qualified body is prepared and consumed before progressing/reclaiming. Stable retry retains cursor, current raster job, source leases and world preparation. Complete staging alone can write/present the provider.
- Standalone private prepare() also enters the same bounded admission path. Supported missing bodies cannot take a hidden whole support/length/compiler helper. Actual retained-ink consumers receive held geometry instead of synchronous rebuilding after a lost alias.
- Pending job discoverability is protected; ready indexed discovery can retire while an in-use body keeps charged leases. CPU completion shrinks the original admitted lease in place, avoiding a false soft-quota failure while reducing usage.
- Existing source, mask, transform, opacity, device-grid and z-order semantics retained. Creation staging reuses the existing non-ink / creation / ink drawing order. Shared mapper remains physically in skia_headless_backend.cpp.
- No SDK/Input/Interaction/Composition/Scene/Semantic/History/Operation/Brush/ARC edits. Default32/96MiB,8MiB/512 entries and maxGroups/coverage/time settings unchanged. GPU/AA decisions, independent null-prepared/CPU reference and dependency locks unchanged.

## RED / GREEN

Original final valid projectRuntimeScene probe is preserved and rerun. Before: long7199 first +512short64 default rebuilt1026 bodies, scanned14398 legacy support and compiled7199 legacy inputs. One-step8192 attempts had no present.

Final same probe:

| Native demand | Calls | Body builds | Legacy support / compile | Present |
|---|---:|---:|---:|---:|
| 32-distinct | 21 | 32 | 0 / 0 | 1 |
| 513-short | 257 | 513 | 0 / 0 | 1 |
| one-long-512-short | 257 | 513 | 0 / 0 | 1 |
| one-step | 4263 | 513 | 0 / 0 | 1 |

Additional real Metal regression covers513 distinct sources, >512 equal sources in different allocations, one-step aggregate<=32 source inputs, actual canonical and Direct consumption, independent pixels, masks/transforms/opacity, pending target preservation, creation order, held ready aliases/current job, reclaimable ~75MiB pressure, cancel/last-release. Checkpoint replay remains separately counted; no zero warm compilation claim for checkpoint rendering.

## Verification

- Native48/48; Node36/36; Python21/21 (original17 +4 repair ledger checks);120 boundary contracts. Exact final commands/logs are preserved.
- New matched normal and qualification SDKs each have36 immutable files; native/WASM builds consume locked prebuilt Semantic/Skia. No dependency source rebuild.
- Exactly two new serial browser workers: UR1C2R1-Q1 and UR1C2R1-N1, six S0/S2 canonical / drag-cancel / settled-erase-navigation cases each. Both OBSERVED; actual canonical and Direct consumers are observed in all required qualification cases. Normal is trace-free.
-28 cold stages and8 roundtrips QUALIFIED against the unchanged ab7a1a3 independent CPU oracle / saved exact snapshot+request+resource identities. All cold buffers and references are saved. Some raw byte-hash roundtrip predicates remain FAILED as originally recorded; the frozen independent CPU geometry AA rule qualifies those differences. No tolerance or raw predicate rewrite.
- Source/artifacts/protected bindings stable. Both owned process families fully cleaned; remaining_owned_pids=[]; remaining workers NONE.

## Functional observations (not paired performance acceptance)

Instrumentation content-write latency/gap and post-phase settled checks are in functional-observations.json. Content writes are not CanonicalVisible receipts. Exact Up-to-matching-generation-visible timestamp is NOT_OBSERVED and remains null; the extra final settled check is not relabeled as Up latency. Cold generation/operation/presented-state receipts and unchanged history predicates are saved.

| Normal erase case | First instrumented content write ms | Worst phase content-write gap ms | Maximum input handler ms |
|---|---:|---:|---:|
| S0-erase-navigation | 106.80 | 165.40 | 98.10 |
| S2-erase-navigation | 123.70 | 332.10 | 331.20 |

These are new functional observations from this source, not comparisons with old unpaired samples. No performance SLO, formal document-scale qualification, or successor UR1d/UR2 work is claimed.

## Closed browser budget

History preserved:52 starts /2004 seconds. Q1 elapsed48.658746s, charged49s, peak1828976KiB. N1 elapsed53.353342s, charged54s, peak1697056KiB. New2 starts /103s; cumulative54 starts /2107s of8370s, remaining6263s. Both180s/3072MiB guards completed with cleanup=true. No seed/smoke/retry or extra browser worker.

## Delivery

Normal Demo: http://127.0.0.1:4287/example.html (server session40242). Old4284/4285/4286 and bound old artifacts preserved. Exact source delta17 files (14 existing Render/test files +3 new verification files) and binary-patch byte roundtrip are materialized. Source, native proof, matching SDK and Web raw archives have per-member SHA256 manifests.

Remaining: independent control review; UR1d/UR2, overall Gate/P34/performance acceptance and new browser authorization remain outside this repair. No additional worker is available in this package.

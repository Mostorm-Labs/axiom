# UR1c2 resolved implementation design

The existing SkiaRenderer owns the PreparedGeometryCache, SourceRetentionPool and ResourceBudgetManager. No owner, public API, SDK, algorithm, GPU decision, or budget default changes.

## Lookup, request, advance

Lookup only resolves the already qualified immutable owner+pointee+type alias and returns Ready; it never builds or drains a job. Request performs the existing bounded fingerprint and full unknown-allocation equality/retention validation and admits one shared in-flight body. Advance performs one bounded source/geometry quantum (at most 32 source knots). A compatibility get may build a fresh standalone body as before, but must return unavailable for an existing incomplete job rather than synchronously drain it. Equal independently allocated sources share the same job only after complete equality; each source allocation retains its own quota.

Jobs live in the existing body index, with the same body/alias LRU and maxEntries. They hold an immutable ReferenceCommand, its source leases, charged job/staging/output/index allocations and compiler state. The world-body basis is full source/config/seed equality, independent of camera, masks, opacity and target. Surface/provider/resource/metrics and plan identity still qualify device raster jobs and publication in the existing renderer/cache. Camera-only changes can discard device pixels while retaining valid world-body progress. Owner reset and entry/source replacement cannot publish a stale body or frame.

## Bounded preparation

Support and prefix distance use one source quantum at a time. Compiler append uses <=32 knots; path construction and group/bounds construction process bounded primitive batches. Checkpoint representation is an alternative charged realization under the same per-entry cap, with checkpoint construction split into <=32-knot advances. Visible checkpoint replay remains real compilation and is counted separately from unique full-body preparation. No whole-history scan is hidden in a constructor, footprint call, request or draw-leaf get. Unknown allocation retention/equality remains measured separately as an admission validation, not zero work.

One frame-owned allowance starts before body preparation. Every advance consumes an existing maxCoverageSteps unit and elapsed maxMicroseconds; the remainder is passed to existing canonical coverage preparation. No per-object 8ms allowance. One-step tests observe actual support/distance/compiler/path/group progress and target pixels. Pending/resource denial preserves job progress and dirty/retry behavior and returns before final provider target writes or present.

## Actual consumers

Canonical admission reads ready world support rather than scanning knots. CompactStrokeRasterJob pins the ready body, reads its support/length, and uses the same geometry on its original device-grid coverage targets. Ordinary Direct/isolated mapper leaves and v4 damage-miss pass lookup-only ready geometry to the existing compact material helper; no facade-only query and no new renderer. Final target drawing begins only after the frame preflight has qualified all required misses. Complete resident pixels may skip geometry and coverage entirely. Existing navigation/retained-ink consumers keep using the same service.

## Independent reference and accounting

Null-cache/null-prepared Direct and explicit Raster/range/legacy routes remain independent. GPU AA, masks, material, opacity/z order and shader/device-grid origin do not change. Actual support, length, unique compile, bounded replay, canonical/Direct ready use and admission-validation counters are recorded at their loops/calls. Native mechanism RED uses the accepted source with instrumentation only; missing symbols are not RED. New normal and qualification SDKs are frozen with drivers/resources before the only two new browser workers. No old evidence/ports/artifacts or UR2 archive is modified.

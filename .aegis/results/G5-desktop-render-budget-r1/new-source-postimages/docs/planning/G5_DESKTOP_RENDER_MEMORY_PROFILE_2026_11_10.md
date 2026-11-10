# Internal render memory profiles

The ordinary SkiaRenderer uses the existing RenderCacheOptions,
ResourceBudgetManager and GPU retirement owner. Desktop and Web builds default to
256 MiB optional cache, 512 MiB shared derived resources and 64 MiB per cache entry.
Android and iOS builds default to the conservative 32/96/8 MiB profile. These are
ceilings per renderer, not upfront allocations, document storage limits or total
browser/GPU memory measurements.

Select a build profile with `AXIOM_RENDER_MEMORY_PROFILE=DESKTOP` or
`CONSERVATIVE`. The internal overrides `AXIOM_RENDER_CACHE_MIB`,
`AXIOM_RENDER_SHARED_MIB` and `AXIOM_RENDER_ENTRY_MIB` accept integers 1–4096,
with entry <= cache <= shared. Existing explicit 32/96 builds keep those values;
when entry is absent its profile default is clamped to the effective cache limit.
Explicitly inconsistent overrides fail configuration. Existing CMake cache values
persist: select all three overrides when changing a previously configured build.

Explicitly constructed RenderCacheOptions retain their conservative defaults.
Tests and tools can still set lower quotas. The normal renderer applies all three
configured limits; consumer JS/DLL APIs have no new budget fields.

The larger working set permits two full viewport RGBA layers at desktop DPR2.
It does not change object eligibility for static reuse, geometry, AA, masks,
preparation work limits, receipt rules or GPU completion. Leases remain charged
until actual GPU completion. Unaffordable work retains its existing retry policy.
Old 32/96/8 evidence and Demo 4316 stay frozen and are not reclassified.

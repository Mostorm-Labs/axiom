# SDK 0.3.12 release

The user authorized integrating codex/geometry-navigation-integration through
`fdedcf63c58f5fa9984481d7e571b974e04ca1c7` and publishing a new SDK. Merge
`ee4b5e034a8c04d8b1df7584d5698cd2199527e9` preserves main's published 0.3.11
history and its MSVC shadow fix. The feature branch already has subsequent work;
this release uses the explicitly selected commit as its boundary. Its branch,
remote reference and worktree remain available for continued development.

This is a patch-level performance/correctness improvement. Runtime/OperationEngine
retain document, camera and receipt ownership. Render and the existing resource
budget/provider owners manage derived coverage, GPU use and completion lifetimes.
No public JS/C SDK capability, API/ABI, Operation/Snapshot schema or dependency
lock changes. Consumer API/ABI and transport remain version 1.

- Localize optional recorded exact coverage and use the verified Skia alpha-mask
  realization for supported v4 coverage, retaining complete Direct fallback.
- Reuse qualified coverage/frontier work and masks across compatible camera demands
  while preserving clip, sampling, order, valid regions and work/resource limits.
- Separate resident image and staging charges through actual qualified GPU use;
  retain unfinished charges across eviction, cancellation and provider replacement.
- Preserve canonical presentation when optional navigation snapshots are denied,
  and distinguish new canonical snapshot writes from prior retained-image reads.

All Web/API/RN/platform package versions and RN dependencies are 0.3.12. Existing
builds and accepted prebuilt Semantic/Skia SDKs are reused. Ordinary consumers still
use the root JS facade or opaque C/POD ABI; new provider/lifetime machinery is private.

Fresh local verification: 34 focused native cases including tile work, provider,
preview/presentation/frame state, cache, shared Scene, export, selection and table
paths; 31 G5 Node tests; 37 packaging and 15 workflow/server tests; strict consumer
TypeScript; 118 Runtime boundary contracts; offline Semantic resolution; strict
prepared-geometry shadow compilation; rebuilt matching JS/WASM/resources with real
Chrome external-consumer, selected navigation and feedback checks. Dense DPR2/DPR4
pan, idle and freeze/recovery workers complete with unchanged document/selection/
operation state, nonblocking fences and owned-process cleanup inside 180s/3GiB caps.
These are functional/release checks, not new performance acceptance measurements.

Historical evidence under g5-canonical-coverage, g5-restore-navigation,
g5-coverage-work-reuse and g5-gpu-lifetimes remains source-bound and unchanged.
The recorded 112-case native set retains five baseline failures. Overall restore
performance guards, continuous source clarity and high-DPR cold exact fallback
tails are not declared closed. At DPR4, optional retained/composite images may
exceed the existing gate and use expensive complete fallback. Subsequent canonical
budget work after the selected commit is intentionally outside this release.
No full-suite, physical platform, 100K/1M, G5 or Gate/P34 acceptance is inferred.

Hosted publication must verify the exact release source, Web/Windows consumer
archives and demos, all six RN native targets and all six npm packages before
completion is reported. Existing published versions/tags/assets remain immutable.

The first unpublished candidate at `3f9cbe987998010114dda05b382456dc43abfaee`
failed MSVC /W4 /WX on three shadowed navigation-tile locals. Only identifiers
were renamed: the work snapshot to workBefore and inner coverage flag to
gpuCoverage. Host production-source -Wshadow -Werror=shadow fails before and
passes afterward; warnings, quotas and behavior are unchanged. Modified production
translation units are additionally checked before the new hosted verification.
Original runs stopped without executing publication jobs and no Release existed.
Exact source, tag identity and diagnostics are retained in
verification/evidence/releases/0.3.12/msvc-shadowed-locals.json. Update only this
owned unpublished candidate with an exact old-ref lease and rerun both complete
release workflows. Keep every published version unchanged.

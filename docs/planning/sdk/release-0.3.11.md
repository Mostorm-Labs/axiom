# SDK 0.3.11 release

The user authorized merging codex/geometry-navigation-integration through
`053e22c6fef4e70c59afc7db26275e8a81dedc07` into main and publishing a new SDK.
Merge `8e37a818c46c1b5a9f86df38be19d802c865a506` retains both the feature
history and main's published 0.3.10 metadata. The feature branch and its worktree
remain intact for continued development. This is a patch-level improvement.

Runtime/OperationEngine retain canonical mutation, camera and receipt ownership.
Scene owns immutable payloads, projection snapshots and derived chunk indexes.
Render owns bounded source retention, prepared contour replay, coverage jobs and
Skia presentation. No public JS/C capability, ABI, semantic schema, Operation or
Snapshot format is added; SDK API/ABI and transport remain version 1.

- Share immutable Scene payloads and copy-on-write snapshots; query conservative
  local stroke-chunk indexes while preserving original draw order and fallback.
- Retain source leases through group rebinding and account shared storage through
  the lifetime of cached and in-flight readers.
- Reuse bounded exact contour draw streams and seek replay checkpoints for large
  navigation sources without changing Brush geometry or canonical pixels.
- Preserve sharp overlap and refinement progress, stabilize coarse levels during
  pan, and prepare bounded valid exposed regions before whole-cell coverage.

All platform/API/Web/RN versions and RN dependencies are 0.3.11. Locked Semantic
and Skia SDKs and existing build directories are reused; dependency locks and
published dependency stacks are unchanged.

Fresh local checks pass: 28 native cases including all six shared Scene cases,
cache/prepared geometry/navigation/visibility, public image export, region
selection and table semantics; 37 packaging tests; 15 workflow/server tests;
25 G5 Node tests; strict consumer TypeScript; 118 Runtime boundary contracts;
offline Semantic Store resolution; rebuilt matching Web/WASM/resources with real
Chrome external-consumer, selected navigation, feedback and bounded dense-scene
lifecycle checks. The dense worker completed and cleaned its owned process family
within its 180-second/3-GiB cap. Local evidence is under out/release-0.3.11-*.

Historical source-bound evidence is retained under verification/evidence/performance,
including g5-shared-scene, g5-exact-contour-replay, g5-large-navigation and
g5-dense-navigation. The dense implementation reports 107/112 related native
checks with five existing baseline failures and no new failure names. Its zoom
and freeze-recovery tails, conservative reservation limits, remaining exact
convergence costs and unqualified physical/100K/1M scenarios remain findings.
Neither those measurements nor this focused release regression establish a
full-suite, physical platform or formal Gate/P34 pass.

SDK 0.3.10's Web/Windows/GitHub/Web npm publication remains immutable. Its RN
attempts did not reach package publication after hosted macOS runner acquisition
failed. This version uses an independent exact-source hosted release run for all
six native targets and the Web/Windows consumer archives, then npm publication.
Report actual hosted build/publication results separately from local checks.

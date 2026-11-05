# SDK 0.3.13 release

The user authorized integrating codex/geometry-navigation-integration through
`8c4ab045a635a60d9e22f6bcbf30b55962d0493c` and publishing a new SDK. Merge
`d5346293b38cb18c56b9191b3e7267e58f1c7983` retains main's published 0.3.12
history and MSVC fixes. Subsequent feature-branch documentation/development is
outside this selected boundary; its branch and worktree remain available.

This is a patch-level rendering performance/correctness improvement. Runtime and
OperationEngine retain canonical document/camera/receipt ownership. Render's
existing cache, coverage job and resource-budget owners coordinate preparation.
Public JS/C APIs, ABI, transport and Operation/Snapshot formats are unchanged;
consumer API/ABI and transport remain version 1.

- Separate optional cache capacity from mandatory cooperative preparation and the
  shared derived-resource hard cap. Default production capacities stay 32/96MiB.
- Keep oversized canonical preparation cooperative instead of escaping to
  synchronous full-frame Direct because an optional cache cannot hold the image.
- Yield expected coverage-quota denial without unwinding the WASM frame loop;
  resume after quota becomes available and reject structurally impossible buffers.
- Retain completed hybrid correction through snapshot retries and include required
  coverage in composite-only feasibility checks. Partial output never qualifies
  as a completed canonical presentation.

The 64/192MiB CMake profile is a private source-build comparison experiment, not a
new JS/DLL budget selector or the normal release configuration. Both reused local
release builds were checked at 32/96MiB. All Web/API/RN/platform package versions
and RN dependencies are 0.3.13; locked prebuilt Semantic/Skia SDKs remain unchanged.

Fresh local checks pass: 34 native cases (the cache suite now includes oversized,
quota-denial/recovery, exact-pixel and partial-presentation guards); 31 G5 Node
tests; 37 packaging and 15 workflow/server tests; strict consumer TypeScript;
118 Runtime boundary contracts; offline Semantic Store resolution; strict shadow
compilation of the four modified production translation units. Matching rebuilt
JS/WASM/resources pass real Chrome external-consumer, selected navigation and
feedback checks. Dense DPR2/DPR4 workers complete pan, idle and freeze/recovery
with stable source/artifacts and document/selection/operation state, nonblocking
fences, and owned-process cleanup within their 180s/3GiB caps.

The g5-canonical-budget evidence retains the independent default/expanded profile
comparison and failed exploratory candidates. Its 112-case native set preserves
the same five baseline failures. Cold exact convergence and navigation detail
remain open findings; expanded capacity is not a universal device recommendation.
Neither these local release checks nor the historical measurements establish a
full-suite, physical-platform, 100K/1M or formal G5/Gate/P34 pass.

Before reporting completion, hosted gates must verify this exact release source,
Web/Windows SDK and Demo archives, all six RN native targets and publication of
all six npm packages. Published versions, tags and assets remain immutable.

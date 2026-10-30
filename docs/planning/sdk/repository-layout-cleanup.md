# SDK artifact and obsolete-host cleanup

The public JavaScript distribution has one entry/loader module, `axiom.mjs`,
with `axiom.wasm` and `axiom.data`. Emscripten appends the public API to its
generated module using `--extern-post-js`; no separately named Runtime loader
needs to be deployed. Types, input helpers, examples, server and licenses remain
in the distribution. Packaging and external-browser verification use these new
names. Previous release archives are immutable and keep their original names.

The `runtime/<module>/include/canvas/<namespace>/` source layout remains
intentional. These suffixes are qualified include paths, not DLL/file suffixes:
both source consumers and installed SDK consumers use the same
`<canvas/runtime/runtime_composition.hpp>` and `<canvas/platform/...>` headers.
Foundation and Composition also publish headers into shared `canvas/runtime`.
Flattening only the source tree would require generated aliases or widespread
include changes; flattening the installed tree would break current consumers.
This cleanup preserves those public paths and C++ namespaces.

Arc and its FastInk experiment (`pocs/fastink`) have been retired, together with
the Arc workflow and the Arc-dependent Playground Windows/Android applications.
The old `apps/ink_playground` directory no longer exists. Its still-used fixture
library, native regressions and qualification runners moved to
`verification/runtime_qualification`. CTest IDs remain unchanged.

The new build switches are `AXIOM_BUILD_RUNTIME_QUALIFICATION` (tests) and
`AXIOM_BUILD_QUALIFICATION_WEB_HOST` (default OFF, old Web qualification fixture
only). Ordinary product/SDK builds with `BUILD_TESTING=OFF` do not build that
fixture library. Current products remain Minimal Host, Canvas Demo and SDK
examples. Explicit requests to build retired Arc/FastInk fail with a diagnostic.

G4.5 retains its brush qualification jobs without the retired Playground native
application. G4.6-C/G4.7 retain native runners and the opt-in Web fixture; their
Arc-dependent Android application jobs were removed. This is retirement of an
old application, not a new Android acceptance claim. Historical `.aegis`
packages, verification corpus and evidence have not been rewritten. Sources
for the old hosts remain available through Git history (pre-cleanup source:
`eaccff7a35f38991a2826216f2cca4a704554bec`).

Validation uses the existing Windows strict build, relocated fixture regressions,
actual external Windows SDK window consumer and headless Chrome JS SDK/browser
consumer. Physical input/IME acceptance is separate from these automated checks.

Observed results for this cleanup:

- Windows strict build and Web/JS plus optional Web qualifier build: exit 0.
- Fresh installed JS package in actual headless Chrome: 51 checks and the
  shipped example's pointer drawing/selected-button checks passed.
- External installed C++ consumers: three lifecycle/style tests passed; the
  separate D3D12 Windows window consumer returned `PASS_SCOPED`.
- Python packaging/server/workflow checks: 16 passed. Module boundary and F08
  static checks passed.
- CTest discovery: identical 355 names before/after relocation. Fifty retained
  fixture tests plus six current Runtime tests ran: 44 passed, 12 failed, exit 8.
  All twelve failures were reproduced by rebuilding and running the exact
  pre-cleanup revision `eaccff7a35f38991a2826216f2cca4a704554bec` in an isolated
  worktree with the locked SDKs. Eleven assertion failures matched; the same
  `G46CQualificationBundle` timed out at 120 seconds in both runs. All fifty
  relocated fixture source files are byte-identical to that revision.

The retained qualification suite is therefore not claimed green. No old expected
values or assertions were relaxed to accommodate the cleanup, and this task does
not repair those pre-existing qualification failures or the deferred D07 OOM
issue. Local command logs and the per-case baseline comparison are under
`out/cleanup-*.log`, `out/cleanup-*.json` and `out/cleanup-baseline/`.

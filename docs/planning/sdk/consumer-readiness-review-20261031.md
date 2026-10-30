# Consumer SDK readiness review — 2026-10-31

This is a source/packaging/feasibility review, not a Gate or stable-release
acceptance record. Source audit base:
`3ad45b8134ca075e91e2ebae438763b114d789b8`, branch
`codex/sdk-cpp-first-multi-bridge`. The working changes repair the JS npm
whitelist and extend release verification; they do not implement a production
Windows DLL ABI or modify Runtime/schema/dependency locks.

## Windows delivery

The current Windows package is a **static development SDK**.
`cmake/AxiomRuntimeConfig.cmake.in` imports ten component archives and the
Windows surface archive, calls `find_dependency` for Semantic/Skia, and requires
the exact producer compiler, Release and `/MT`. `tools/release/preview_package.py`
then copies the complete dependency SDKs into the consumer archive. This is not
the intended simple binary consumer experience.

The internal archives are still legitimate inputs to building Axiom. They should
stop being mandatory application inputs once the production DLL boundary exists.

A local feasibility probe linked the **existing installed SDK**, its locked
static Semantic/Skia dependencies and real Windows adapter into one `axiom.dll`.
The probe exports one deliberately experimental C function which runs the
existing external Windows consumer scenario inside the DLL. No STL object,
exception or allocation ownership crosses that experimental entry. The
application compiles with `/MD`, the DLL with `/MT`; this does not claim arbitrary
compiler compatibility for a future production API.

Observed proof:

- Strict MSVC build: exit 0; one `DllOnlyConsumer` CTest: PASS.
- A second run from a fresh directory outside the checkout, with only DLL,
  executable, `icudtl.dat` and font assets, and a System32-only PATH: exit 0.
- Both calls observed `objects=2 generation=3 work=6` after real Ink/Shape,
  selection, snapshot/restore, stale-contact rejection, resize and surface
  loss/recovery checks. Runtime/adapter shutdown completed before unloading.
- Prototype DLL size: 13,304,832 bytes; SHA-256:
  `bd4d6e61c9ea1a28d90a5e362b527b87125f9e8ae0077eb944bc10e2dbec32df`.
- PE imports: D3DCOMPILER_47, d3d12, dxgi, dcomp, USER32, KERNEL32 and dbghelp.
  No separate Skia, Protobuf or Abseil DLL was imported.
- Actual commands, prototype sources, PE exports/imports, JUnit and external-run
  output remain under `out/sdk-dll-feasibility/`. This is not a redistributable
  full-function SDK and is not added to the release workflow.

The initial probe incorrectly treated `hostObservation.lastCanonicalFrame` as a
presentation counter and observed zero after successful adapter presentations.
The proof now records the real prepared work ID and successful `present()`
receipt instead. The diagnostic field's meaning/update path needs a separate
check; it must not be advertised as physical-presentation evidence.

Recommended production consumer layout:

```text
bin/axiom.dll
include/axiom/...       # small public C++ facade and DTOs
lib/axiom.lib          # optional import stub, not dependency implementations
assets/...            # required ICU/fonts, or an explicitly supported embedding
licenses/...
example/...
```

Applications using `LoadLibrary` do not need the import stub. A compile/link
consumer normally needs public headers and the small import library. ICU/fonts
are runtime data, not intermediate libraries; a literal DLL-only distribution
requires separately implemented and verified resource embedding/loading.
Resource discovery must not depend on the application's working directory.

The production boundary should preserve a typed C++ consumer API while using
opaque handles, versioned POD values and caller-owned copy buffers or explicitly
released SDK-owned buffers at the binary boundary. A narrow C calling boundary
can be the implementation detail behind that C++ facade. It must not blindly
export the current classes with `std::vector`/`std::string` ownership across two
CRTs. The actual handle/version/error/callback contracts still need to be bound
before claiming a production DLL API.

All commands still route through the same RuntimeComposition/RuntimeFacade,
input/text/Scene and History owners. This does not introduce another Runtime,
selection store, transaction system or semantic schema. Windows attach, input,
IME and dual-surface presentation must be included, and the default consumer
CMake config must no longer find Semantic/Skia or expose component archives.

## JS findings and priority

| Priority | Finding | Current status / required work |
| --- | --- | --- |
| P0 | Pending-frame/new-input handoff is not robust | Previous single-file-loader experiments exposed it; retaining the separate loader reduced its occurrence but did not resolve it. This review reproduced `Example timeout: visible rectangle` in the npm browser consumer while all 51 core checks passed. Investigate real JS scheduling, Web adapter pending work/completion and Runtime frame identities together. Add deterministic pending-frame/input/resize/context-loss regressions. Do not fix by fabricating completion or ignoring stale/error receipts. |
| P0, packaging fix made | `npm pack` omitted `internal/axiom.mjs` | Confirmed by actual pre-fix npm pack (36 entries, loader absent). The whitelist now includes `internal/**`. Release verification packs, inspects hashes/licenses, installs the local tarball offline, typechecks a real `@mostorm/axiom` consumer, and runs the real browser consumer against the extracted npm package. No registry publication occurs. |
| P1 | Production SDK source depends on example/test owners | `sdk/web/CMakeLists.txt` takes text bridge/input code from `apps/axiom_canvas_demo` and pointer input from `verification/sdk_web_external/consumer`. Move shared platform collectors/adapters to a production owner reused by SDK and demos; preserve existing input/IME semantics and byte-level parity where applicable. |
| P1 | Document compatibility still has a candidate extension | `section-snapshot-compatibility.md` still marks Section snapshot schema 3/capability support as candidate. Close the version/reader rejection contract and external restore/remote/resource ordering evidence before promising a stable persistence format. Files, databases and networks remain application responsibilities. |
| P1 | API/behavior evidence is narrower than the surface | Browser checks cover key real paths but not every public facade method/combination. Add external-consumer rich-text/IME, Sticky overflow/resize, connector attachment/route, hierarchy/History and JPEG/WebP/Crop coverage. Preserve explicit multiselection and stale-revision rejection. |
| P1 | MSE-20/23/26 provider evidence remains incomplete | Real per-object permission/provider/reservation-fault cases remain missing. Explicit local-single-user mode is useful and already implemented; it does not establish fine-grained permission or failure containment evidence. |
| P1 | Browser/deployment support needs a concrete matrix | Current real execution is headless Chrome/WebGL2, including synthetic and CDP pointer input. Add one actual bundler consumer, assetBaseURL/CORS/CSP/MIME/cache/version-failure cases and supported-browser bounds. Worker/Node rendering is not currently promised. |
| Physical verification | External SDK IME/touch/lifecycle | Internal Minimal Host results do not establish external SDK physical acceptance. Verify real composition/candidates, native focus/Esc/close, contact generations, multi-touch, DPI/resize and background/context recovery after fixing the P0 behavior. |
| Scoped future capability | Arbitrary live font additions | The current builtin catalog is startup-oriented; resource byte delivery fills explicitly declared resources. This does not promise arbitrary new font registration in an already populated document. Define that scope separately if required. |
| Deferred | D07 allocation-failure hang and historical fixture failures | Keep the Protobuf/Abseil OOM issue and the twelve previously reproduced legacy qualification failures visible. This review does not reopen their implementation or claim they are fixed. |

The frame finding is an **open issue**, not a conclusion that the final npm
package is corrupt: the ZIP and npm runs used matching engine/input hashes;
the same 51 checks passed and an earlier npm browser run also passed the example.
The later strict full run failed on example rendering, so the overall result is
FAIL and must not be published as an all-green stable SDK.

Current checks in `out/sdk-npm-readiness/`:

- ZIP core checks (51) and shipped example: PASS in the recorded final pipeline.
- npm pack, required-file/hash/license comparison and offline install: PASS.
- External `@mostorm/axiom` TypeScript 5.9.3 positive/negative consumer: PASS.
- npm core checks (51): PASS; its shipped-example visible-rectangle test timed
  out. `verify_js_bundle` exits nonzero and preserves the failure in the log.
- Packaging/server Python tests: six PASS. No new physical acceptance claimed.

## Release reality and next slices

Read-only GitHub API and remote-ref checks found the latest application SDK
preview still at `fc536ad0586d58834d7abec0caeee1ea177ab9d2`:
[release](https://github.com/Mostorm-Labs/axiom/releases/tag/axiom-sdk-preview-20261030-fc536ad0).
It has four SDK/demo ZIPs and SHA256SUMS, with no standalone JS SDK asset. The
local standalone-JS and naming changes are not present in that release.

Recommended order, respecting JS priority:

1. Resolve JS/Web frame handoff with real deterministic regressions; retain the
   npm packing/install/type/browser checks as release blockers.
2. Close the minimum JS public/deployment/document contract gaps and consolidate
   shared SDK platform source ownership; run external feature/input evidence.
3. Implement the complete typed Windows DLL facade/adapter/lifetime boundary
   using the demonstrated static linkage, then test an external application
   without any Semantic/Skia/component headers or libraries.
4. Change the default Windows release asset to the consumer DLL package and
   publish matching Windows/JS preview assets from the same exact revision only
   after required checks pass. Keep old releases/evidence unchanged.

No main merge, SDK lock change, global Authority rewrite or new release was
performed by this review.

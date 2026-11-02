# Windows RNW 0.84 integration: feasible, physical acceptance incomplete

Date: 2026-11-02 (Asia/Shanghai). SDK 0.2.0 development/private/NOT_QUALIFIED.

A real RNW/Fabric Windows app built and launched with AxiomNative,
AxiomCanvasView and the actual native engine. Controller state reported Ready,
an attached 946×625 surface, objects 0 and generation 0. The user confirmed that
buttons worked. This establishes RN as a feasible Windows product-shell integration
path. It does **not** establish working ink/preview or complete physical acceptance.
No Web, WebView, old standalone DLL demo or host test substitutes for this app.

## Exact versions and provenance

- RN 0.84.1 / RNW 0.84.0 / React 19.2.3 / Node 22.13.0; Fabric/New Architecture.
- VS 2026, MSVC v145 (14.51), Windows SDK 10.0.26100.0; Windows 10 x64.
- Original native DLL source: `1c8aebc4909c410465cd1bd3c554d01b64be170a`.
- Physical app used local binding changes on that source. Their exact Windows
  file hashes are in `binding-sources.json`; artifact hashes are in
  `artifact-hashes.json`. It was **not** a clean same-revision release package.
- Before submission the branch fast-forwarded to
  `f4d179c3284022cca46d87dba348e086bdd8b207`, preserving upstream macOS/Runtime fixes
  including native trackpad navigation.
  Historical binaries have not been relabelled with this newer revision.
- Original native/package manifests are retained. The RN package was locally
  patched, so its original package manifest is historical and does not attest
  the patched package. No clean post-sync native/RNW rebuild or physical run was
  performed for this evidence submission; those remain pending.
- Semantic `windows-x64-msvc-static` and Skia `windows-x64-d3d12` were consumed
  from locked published SDKs. No dependency or descriptor locks changed.

## Findings and bounded fixes

1. Windows C++ binding namespace qualifiers, provider registration arguments,
   generated string props and Cxx PCH wiring were incompatible with the initial
   consumer. Minimal binding/build fixes are retained.
2. CDB captured `0x80040154` at
   `RegisterAutolinkedNativeModulePackages -> MostormAxiom.ReactPackageProvider`
   activation. The project emitted `AxiomReactNative.dll`, while C++/WinRT's
   namespace fallback searches `MostormAxiom.dll`. Setting TargetName to
   `MostormAxiom` fixed activation and the real app reached Controller/surface
   startup. See `logs/rnw084-first-exception-cdb.log`.
3. The user then reported **buttons work, ink does not, no preview**. A read-only
   HWND hit-test reproduced Canvas center hitting RNW's sibling ContentIsland.
   `HWND_TOP` within the Canvas layout rect changed that hit to AxiomRNCanvasV1.
   See before/after JSON. This proves window hit routing only.
4. Physical Escape stopped Computer Use immediately afterwards. Drawing/preview
   after the Z-order change was **NOT_RUN**. No fix claim is made for that symptom.

Earlier run 002's session/Promise root-cause attribution is unsupported and
superseded by the activation stack. RNW supports the retained by-value
ReactPromise; no Promise workaround or fake token is in the final source.

## Executed scope

| Check | Observation |
| --- | --- |
| RNW Release builds | Three complete builds exit 0, logs and exit files retained |
| Real RNW app startup | Controller Ready, attached 946×625 surface, actual D3D12 providers |
| RN toolbar | User confirms buttons respond; individual command receipts unavailable |
| Mouse ink / preview before final fix | FAIL per user observation |
| Canvas sibling-window hit routing | FAIL before / PASS after; host diagnostic only |
| Mouse ink / preview after final fix | NOT_RUN after user stopped Computer Use |
| Screenshots / recording | Unavailable: Windows.Graphics.Capture timeout |
| Six brush, eraser, History, attachment and lifecycle checklist | NOT_RUN / incomplete |
| Touch, pen, pressure, mixed DPI, device loss | Unavailable or NOT_RUN |
| Native RN IME | OPEN/BLOCKED: not implemented |
| Release/package Python tests | 27 tests, exit 0; not physical evidence |
| Controller/transport Node tests | 12 tests, exit 0; not physical evidence |

Canonical/preview identity comes from actual D3D12 initialization logs and the
Windows provider source, not invented Controller counters. No public diagnostic
API was added. Capture text is a labelled transcription of tool output, not an
image. An ICU executable-root lookup warning also remains visible in runtime logs;
AxiomAssets/icudtl.dat exists, and no text/ICU qualification is inferred.

## Reproduction

The `consumer/` directory retains App.tsx, package-lock, official-template project,
Metro watchFolders/nodeModulesPaths and autolink configuration. Dependencies use
only the package root Controller/AxiomCanvasView exports. Use Node 22, app-local
Windows App SDK properties and `MostormAxiom.dll` from the SDK project. Build with
VS MSBuild, Release/x64/v145, SDK 10.0.26100.0. Exact commands and exit codes are
in `commands.json` and build logs. Preserve old packages; stage a fresh package
from a clean source with the official release tools for a new run.

The next physical run must verify live preview, committed ink, six brushes,
erasers, Undo/Redo, detach/reattach and input/lifecycle isolation before any claim
of drawing or platform qualification. No Gate, G4, P34, PHYS or release PASS.

## Submission reconciliation

The local result was rebased onto remote f4d179c3284022cca46d87dba348e086bdd8b207
before publication. The new upstream native navigation API is preserved; the old
physical-run DLL predates it and is not evidence of its Windows ABI conformance.
Post-rebase Python release/package tests: 27/27, exit 0. Post-rebase Node shared
Controller/transport tests: 12/12, exit 0. See latest-*-tests.log. These are tooling
and JS tests only. Raw historical compiler logs retain original whitespace;
source/document changes were checked separately without rewriting raw evidence.

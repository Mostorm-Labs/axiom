# Consumer SDK delivery — 2026-10-31

## Scope and owners

Authorized by `AXIOM-SDK-CONSUMER-20261031-v0.1`, following the committed JS
frame fix at `43f34f4ec397bd64f25a72df5acd55d557588470`. SDK version is
`1.0.0-preview.1`; transport and native POD ABI version are 1.

`sdk/common/Session` is a private shared transport decoder over the existing
RuntimeComposition/RuntimeFacade. It does not own another canonical model,
History, hierarchy or selection. Web consumes the same decoder. Its native
text snapshot/intent adapter reuses the same text helpers as the Minimal Web
Host. Runtime semantic/schema/SDK lock and qualification-hook owners are
unchanged.

`sdk/windows/` owns DLL handles, buffers, callback/thread admission, and the
platform adapter. The application owns its parent HWND/message loop; the SDK
owns a child canvas and reuses current Win32 mouse/pointer/IMM32 collectors.
Public C++ facade code compiles in the caller. Native /MT and caller /MD memory
never crosses as an STL allocation. Controlled close is distinct from forced
abandon. App-local format focus retains editing; callbacks reject reentry.

The Windows consumer install contains one implementation DLL plus its small
import stub, public headers, runtime assets, CMake config and licenses. The
current binary check measured about 13.6 MB for the DLL and 4.1 KB for the
import stub; exact build hashes/sizes belong to each release manifest. Only 13
explicit C/POD symbols are exported. No Semantic/Skia/Abseil/CRT implementation
DLL imports or internal static archives are part of the consumer profile.

## Automated evidence

Local pre-release coding validation (dirty-tree artifacts are not release assets):

- Strict MSVC 19.51 /MT /WX DLL build.
- `verification/sdk_windows_dll/run_external.py --prefix <install> --output <json>`:
  fresh external /MD application, actual HWND messages, operation delivery and
  callback reentry rejection, independent owners, wrong-thread/closed/foreign
  handles, pointer generation rejection, buffer lifetime, native UTF-16 input,
  editing/format-focus, Group/Ungroup/Section, orthogonal Connector, image
  PNG/JPEG/WebP import/replace, Crop, snapshots, real remote apply/dedupe,
  surface loss/recovery, controlled shutdown. Pure C LoadLibrary/unload/reload
  and version rejection run separately. Imports/exports are inspected with
  dumpbin. All commands, exit codes, raw output, DLL hash and actual build
  identity are recorded in the result JSON. The test starts from an unrelated
  CWD and consumes only the installed SDK.
- 28 exact existing Runtime/History/Text/Image/Connector/Section/selection/
  Windows-adapter CTest entries: PASS, no skips. The release builder checks the
  exact required JUnit name set; a partial or missing selection fails.
- `verification/sdk_js/verify_browser.mjs <sdk> <chrome> <json>`: 52 actual
  engine consumer checks, five delayed-frame/loss/resize/idle scenarios and
  shipped example with actual CDP mouse/visible pixel evidence: PASS.
- Nine package/server Python tests: PASS. TypeScript positive/negative compile,
  real npm pack, offline application install and another actual packed-engine
  browser run are mandatory in the final release builder.

Image fixtures are byte-identical copies of existing FC03 JPEG/WebP inputs;
independent dimensions are 138x138 and 550x368. The generated PNG fixture is
4x4. These fixtures exercise the DLL transport, not alternative codecs/oracles.
Native messages and synthetic DOM input do not establish physical IME/touch.

## Exact release binding

`tools/release/build_preview.py` refuses dirty or mismatched source revisions.
The actual DLL and WASM `version` readback must report that clean source and
SDK version. The ZIP and npm consumers rerun against extracted shipped bytes.
Both platform outputs share one exact revision and `sdk/version.json` version.
The release verifier rejects extra packages, mismatched versions/revisions,
missing licenses/assets, corrupted members and internal libraries. CI publishes
four consumer SDK/demo archives, SHA256SUMS and release.json only after both
jobs pass. Raw external/native/browser reports and JUnit are retained in
separate CI evidence artifacts.

## Limits retained

No stable release, Gate, DEV or PHYS acceptance is implied. Physical IME,
touch, DPI/multiple monitors and long-running validation remain pending.
MSE-20/23/26, known historical qualification failures and the independently
deferred D07 Protobuf/Abseil injected-OOM hang retain their prior status.
Applications own persistence/network/durable close and resource blob storage;
snapshots omit Undo history and resource blobs. A transport failure after
invocation is not evidence of canonical rollback. Earlier releases remain
immutable. The old static developer install is opt-in, not the default consumer
payload; existing source C++ interfaces remain available for those developers.

## Final transport admission correction

Before publication, real external DLL testing reproduced a numeric-enum wrap:
`256` could become tool `0`. The first `3ff3310a` tag's workflow was cancelled
before publication; that tag and its evidence remain immutable. The shared
transport now validates integer type and range before narrowing, including
creation/selection/fit, permission, text kinds and Section actions. Native
admission checks precede editor completion, so rejected enums preserve the
active text session. Unknown multi actions reject rather than fall through to
property mutation. External regressions cover four invalid scalar classes
across five controls, text-session preservation, unknown multi rejection,
real property application, stale rejection and Undo. This is SDK transport
validation; no Runtime semantic owner, schema, SDK lock or frozen corpus changed.

The external qualification explicitly uses /MD. Exported demo binaries are then
built and executed with /MT, and inspected for VC runtime DLL imports, so the
ready-to-run demo does not require a VC redistributable. Both variants consume
only the same installed public SDK. Foreign-thread rejection occurs before
acquiring a Context ownership reference, keeping destruction on its owner.

A preserved browser-driver failure occurred after all engine/frame/example
checks passed: the target socket closed before the Browser.close CDP response.
The driver now requires actual Chrome process exit (code 0, no signal) rather
than an acknowledgement on a destroyed target transport; timeouts/nonzero
exits still fail. The same complete consumer/frame/example run then passed.

Hosted run 41108525751 (3c564bdf), attempts 1 and 2, passed Windows but could
not observe Chrome's debugging port before the old 10-second launcher window;
Web compilation and the Node API contract passed. These are preserved browser
environment failures, not product passes. Startup now records executable,
arguments, stderr, elapsed time and actual early exit, creates its isolated
profile explicitly and has a bounded 45-second environment startup deadline.
All SUT/frame deadlines/assertions remain unchanged. A manual probe-only workflow
qualifies only Chrome availability and cannot publish or establish product PASS.
The same local full browser/frame/example suite passed through this launcher.

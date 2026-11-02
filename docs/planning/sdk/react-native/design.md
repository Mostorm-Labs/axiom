# Shared Controller and four-platform React Native SDK

User request2026-11-01: publish current main SDK, then design/develop native SDK
for Windows/macOS/Android/iOS sharing the Web TypeScript contract. Attachment is
product direction, not a claim that POC integration or suggested APIs already ship.
Repository github.com/Mostorm-Labs/axiom; implementation anchor061e613d440f57c12cc76d8c6ede7a6660ad79ed
(descendant of main5254b087). Native work is isolated from SDK0.1.0 publication.

Ownership remains RuntimeComposition -> RuntimeFacade -> existing editing owners
-> OperationEngine. SDK Session is private decoding/adaptation. Scene/Skia remain
derived. No public Runtime/Scene/Skia pointers, duplicate History, ID allocation,
JS geometry or WebView-rendered native fallback. Data Runtime owns persistence and
sync. Canonical apply, native present and external durability are distinct fences.

Contract: @mostorm/axiom-api owns DOM/React-free DTOs and an async Controller.
Existing @mostorm/axiom Web factory/API remain compatible; additive createController
wraps that factory with the shared async interface. RN package owns Controller
transport and AxiomCanvasView. A Controller and mounted view share an opaque native
session token plus view/attachment generations. Tokens are strings, never pointers
or JS numbers. IDs remain full128-bit strings; uint64 stamps decimal strings.
Sync Web readback cannot be copied into a cross-thread native ABI: the shared new
Controller makes all owner-dependent reads/commands async. subscribe callbacks are
scheduled on JS outside native owner callbacks; unsubscribe and teardown gate late
callbacks by generation. Buffers are copied before scheduling; bounded queues reject
without mutation. Native lifecycle bindings cannot be issued as arbitrary business
JSON commands.

Lifecycle: create session detached -> attach one native view -> activate independent
canonical/preview providers -> input and frame service -> detach cancels contacts
and releases surface resources -> remount same session preserves document/History.
Required close behavior: finish actual IME and drain required data custody before invalidating tokens. The current RN hosts do not yet implement the native IME lane; this is an implementation blocker, not qualified behavior.
Host resize/lost/rebind are generation stamped; stale commands/results reject.
Each platform Host owns event collection, IME, display clock, physical dimensions,
GPU context/surface and application composition only. Geometry, eraser, viewport
intent and semantic commit remain shared. Platform consumers receive only library,
public headers, resources/licenses and npm TS/JS glue, never dependency archives.

Platform realization: Windows RNW native component/D3D12+Composition; macOS AppKit
NSView/Metal; iOS Fabric UIView/Metal; Android Fabric View/EGL/GLES3. Preserve an
independent transparent preview surface. POC05 uses historical POC03 owners and is
reference only, not a product SDK dependency. Current production SDK has Windows
and Web hosts; macOS/iOS/Android Runtime hosts and RN module/view bindings must be
materialized and built before those platforms can be called supported.

Compatibility discovery2026-11-01: RN latest0.88.1, RNW0.85.0, RNmacOS0.83.0
(peer RN0.83.10), RNW0.83.3 accepts RN^0.83.0. Initial common verification lane:
RN0.83.10 + React19.2.0, RNW0.83.3, RNmacOS0.83.0. Pin exact codegen/headers in
verification and package peer ranges; do not advertise untested0.88 support.

Sources:
- https://reactnative.dev/docs/the-new-architecture/pure-cxx-modules
- https://microsoft.github.io/react-native-windows/docs/native-platform-components/
- https://microsoft.github.io/react-native-windows/docs/native-platform-modules/
- https://microsoft.github.io/react-native-macos/docs/getting-started

EXECUTION_CLOSURE_CONTRACT
Required: common types/async Controller, real Web adapter using root SDK, native
owned-session ABI/buffer/token/lifecycle admission, four native module+CanvasView
hosts, packaging/autolinking/resource identities and build matrix. Reuse published
locked Semantic/Skia. New RN/API packages use aligned next feature version0.2.0;
0.1.0 release stays immutable. Do not publish native artifacts before qualification.
Required evidence: type parity/no DOM leakage, native real Runtime ownership,
wrong/stale/foreign token and thread/reentry/queue bounds, actual controls/history/
IO across real engine, surface detach/remount/lost/resize, input/IME and independent
preview, C/JSI consumers, four platform builds and RN sample execution, Web matching
artifacts and regression. Pure host tests cannot substitute actual RN view evidence.
Hosted Windows builds and local Apple/Android toolchains are required; physical
input is separately reported. No Gate/P34/production PASS. Explicit blockers:
missing accepted target SDK, incompatible RN host/toolchain, failed frozen tests,
publication auth or unavailable platform executor. Continue independent work when
one platform/release is blocked. Never claim cross-platform support from a stub.


## Current implementation checkpoint

The implementation branch now contains the DOM-free API source, a Native C ABI with bounded owned buffers, Android/iOS/macOS/Windows host adapters, independent canonical/preview providers, and a native consumer staging tool. Host tests cover real Runtime ownership, AppKit input/remount, Android JNI compilation, iOS simulator compilation, token/thread/queue rejection, and Web Controller parity. The Android and Apple RN app builds and physical input/lifecycle runs remain required evidence; these artifacts must stay `NOT_QUALIFIED` until those runs exist.


## Verification checkpoint (2026-11-01)

The source is a development implementation, not an RN platform release.
The SDK 0.1.0 line is independently tracked in PR #158; RN/API version 0.2.0
remains private. Source descendants contain the 0.1.0 preview-generation and
GPU-cache lifetime fixes rather than duplicating or bypassing them.

Executed at this checkpoint:

- Shared Controller/transport: 12 Node tests; DOM-free type consumer plus Web and
  RN type consumers. Portable copying does not require Hermes structuredClone.
- Real Session/Runtime transport: controls, brush, pointer, Undo/Redo, snapshot,
  Table, text, Connector/Sticky identity allocated by Runtime when omitted.
- Native C ABI and registry: invalid/stale token, owner thread, contact count,
  buffer lifetime/capacity, single view, detach/remount.
- AppKit host uses real Metal/Runtime input and retains History on remount.
  This is host evidence and does not replace an actual RN macOS app.
- Native macOS, iOS device/simulator and Android arm64 builds; Android JNI compile.
- Matching Web engine/browser: shared Controller cases and existing facade,
  frame-handoff, context loss, Shell/Axis and Demo regressions.
- Runtime boundary checker: 116 contract files. Release/package tests and
  CI-trigger contract checks. No descriptor or dependency locks changed.

Remaining blockers/limits:

- Native IME not implemented in new RN hosts. Do not claim text-entry support
  from programmatic text commands.
- RNW native project and full RN macOS app require corresponding executor runs.
- RN Android app initially blocked by Maven Central HTTP 403, including proxy
  retry. Temporary-consumer mirror diagnostics do not change repository locks.
- RN iOS required proxy + CocoaPods index update and iOS 17; actual app compilation
  is separate from successful pod installation/codegen.
- No physical four-platform input/lifecycle evidence. No Gate/P34/release PASS.
- Local Debug FC00XRuntimeComposition rebind assertion fails independently of
  the cache change; local full FC02Text times out. Hosted SDK 0.1.0 Windows
  required 34-test regression set passed at 59e828d6 (run 41306973001).

Raw local logs remain under ignored out/rn-sdk-deps; durable follow-up evidence
must bind exact clean source revision and artifact hashes. Packaging now creates
XCFramework and Apple resource bundle before hashing and atomically publishes the
staged directory; dirty/foreign inputs reject before consumer output appears.

## Durable delivery update

SDK 0.1.0 was merged via PR #158 and published at main revision
`07126c4c378afdfba8636bd2cd0452c271357136`. Tag workflow
[41309580105](https://github.com/Mostorm-Labs/axiom/actions/runs/41309580105)
passed Windows/Web verification, GitHub publication and npm publication.
`@mostorm/axiom@0.1.0` was published with `latest`; hosted version readback passed.
Local registry reads still reject with 401 (no credential changes were made).
The downloaded GitHub JS ZIP matches SHA-256 and passed its real external browser
52-case, frame, Shell/Axis and Demo regression at that exact clean revision.

RN implementation source `d7ce720aa4f6b139011a9593e2623535e1d3adb1`
is available in draft PR #159. At that clean source, macOS, iOS device/simulator
and Android arm64 native installs were rebuilt using locked SDKs, then staged with
matching resources and exact hashes. Android RN arm64 Debug App and Release AAR build; the AAR contains
only arm64 native payloads. iOS RN arm64 simulator App builds, launches and displays
the Controller state/toolbar with an attached Session. This establishes module/view
startup, not pointer drawing, IME, physical input or all lifecycle acceptance.

Evidence is checked in under `evidence/2026-11-01/` with SHA-256 manifest. It records
platform limitations and the separate published SDK identity. Windows RN build,
macOS RN App, native IME and four-platform physical evidence remain open, so RN
version 0.2.0 is **NOT_QUALIFIED** and is not published to npm.

For a local symlink installation, Metro needs the staged package in `watchFolders`
and the consumer's node_modules in `resolver.nodeModulesPaths`. An installed npm
package does not need this development symlink setup. Apple simulator verification
must request `ARCHS=arm64 ONLY_ACTIVE_ARCH=YES`; the initial XCFramework does not
contain x86_64. Set both Podfile and application deployment target to iOS 17.

## iPhone physical follow-up

On 2026-11-01 (Asia/Shanghai), the signed Release consumer was installed and run
on a connected iPhone 15 Pro with iOS 27.2. The initial consumer trapped in UIKit
before Axiom startup because the RN 0.83 template did not adopt UIScene. The
external consumer was updated to SceneDelegate + UIApplicationSceneManifest;
Axiom SDK bytes/source were not redesigned. The App then launched with bundled
JS and independent Skia/Metal surfaces.

The user drew real finger strokes and confirmed the explicit checklist normal:
partial eraser live effect, object erase at completion, Undo/Redo, two-finger
zoom, detach/reattach retaining strokes/History, and background/foreground.
Runtime logs record 16 successful pointer commits, and the final screenshot
shows 9 objects, generation 23, contacts 0 and zoom 1.21. See
[evidence/2026-11-01-iphone15pro/observation.json](evidence/2026-11-01-iphone15pro/observation.json)
for the exact scope, sources and artifact hashes. Consumer App/SceneDelegate/
Info.plist are retained there for reproduction.

This closes the listed iOS manual smoke observations only. Native IME, Pencil/
real pressure, rotation resize, measured performance, detailed render counters
and the other platforms remain unqualified. Screen recording was unavailable on
this device; screenshots and Runtime logs were retained. RN 0.2.0 remains private
and NOT_QUALIFIED. No Gate or P34 acceptance is issued.

## macOS physical evidence follow-up (2026-11-02)

The macOS standalone Release consumer was physically exercised after the f4d179c3
trackpad-navigation implementation. Evidence is under
`evidence/2026-11-02-macos-physical/`. The tested binary/package source is exactly
f4d179c3284022cca46d87dba348e086bdd8b207; the evidence was organized after pulling
844ffda3c4a265f3a2bbb8b180a6575bb3355f90, which is the Windows evidence descendant.
The macOS app was not rebuilt against 844ffda3, so this record preserves the tested
source identity rather than relabeling it.

The user confirmed the macOS trackpad-navigation follow-up passed. The final
standalone App capture shows retained strokes, document/scene generation 11, no
active contacts and zoom 0.65. JS is bundled. This is a manual navigation smoke
observation, not individual evidence of every editing/lifecycle step; the earlier
explicit multi-item physical checklist belonged to iOS and is not reused here.
Scoped native/runtime tests were 10/10, Controller/Transport Node tests 12/12, the
real Session/Runtime transport check passed, and release/package tests were 27/27.
Native IME, real pressure, measured performance, full physical editing/lifecycle
coverage and exhaustive brush fidelity remain open. RN 0.2.0 stays private and
`NOT_QUALIFIED`; no Gate, P34, or release acceptance is issued.

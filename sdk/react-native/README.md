# @mostorm/axiom-react-native (0.2.0 development package)

This package exposes the shared async Controller and Fabric `AxiomCanvasView`
contract for the React Native development lanes below. The same TypeScript calls are used on
Windows/RNW, macOS/RN macOS, Android and iOS. Native implementations must bind
one Session to one view and route pointer samples directly to Runtime.

The package is not yet a qualified platform release. `AxiomNative` is a required
TurboModule and fails closed when the native implementation is absent. Platform
hosts must provide canonical and independent transparent preview Skia surfaces,
actual input/IME/frame receipts, resize/lost/rebind handling and opaque token
validation. No POC05 state or renderer is used as a product owner.

## Development build

Build a matching native install with `tools/release/build_native_sdk.py`, then stage the RN consumer with `tools/release/native_package.py`. The package includes only the public C header, finished platform library/framework, deterministic assets and licenses; it does not include Runtime or Skia archives.

The Windows development lane uses React Native 0.84.1, React 19.2.3 and RN Windows
0.84.0, selected by the user on 2026-11-02. Existing Apple/Android evidence remains
bound to React Native 0.83.10 / React 19.2.0 (RN macOS 0.83.0). The peer alternatives
allow those separate exact lanes; they do not establish arbitrary cross-version
combinations or requalify the other platforms. All hosts attach one opaque session
token to one `AxiomCanvasView`; the Controller and view share that session.

The Windows RNW/Fabric app has been built and launched on a physical Windows
machine, with Controller state and working toolbar buttons. Canvas input Z order
was corrected after the user reported no ink/preview; drawing after that correction
still requires physical revalidation. See the [Windows report](../../docs/planning/sdk/react-native/evidence/2026-11-02-windows-rnw-physical-003/report.md).

This package is marked `qualification = NOT_QUALIFIED` until each platform has a real RN app build and physical input/lifecycle evidence.

macOS collects trackpad magnification and precise scroll events in the native
Canvas host, then delegates camera changes through generation-checked
`axiom_native_navigation` to the shared Runtime viewport owner. Pinch zoom uses
the event's view-local anchor and Runtime's 0.25–8 gesture limits; two-finger
scroll pans the canvas (including momentum). It does not create document
Operations or History entries. The shared TypeScript Controller is unchanged.
The additive C/POD host record is size/ABI-versioned; values are borrowed only
for the synchronous owner-thread call. Unknown/stale/detached sessions and
malformed/non-finite navigation reject without changing the camera.

For local macOS physical tests, use a Release consumer with bundled JS/assets,
and load `main.jsbundle` from `Bundle.main`. A Debug consumer that loads an
HTTP URL still requires Metro. After changing a local staged-package symlink,
rerun `pod install` **from the consumer root** and inspect the Pods source paths:
they must point at the same staged package as its framework and resources.
Changing only a symlink does not update CocoaPods' resolved source paths.


## Current support boundary

The four native hosts implement pointer collection and separate Skia surfaces.
Programmatic text calls delegate to Runtime. **Native IME integration is not yet
implemented in these new RN hosts** (including the RNW host); focus, preedit,
candidate geometry and composition completion must be connected to the existing
text effect/fact protocol before text input is qualified. `text.finish()` may
reject or remain pending; successful close is never invented.

Initial targets: Android arm64-v8a/API 26, iOS arm64 and arm64 simulator/iOS 17,
macOS arm64/macOS 15, Windows x64. Android x86_64 has a build entry but is not
verified in this checkpoint. Other ABIs are not advertised as supported.

For Apple, staging automatically assembles device+simulator into
`native/apple/AxiomNative.xcframework`; supply both installs. Assets ship as a
complete `AxiomAssets.bundle` to preserve font subdirectories. Set the consumer
Podfile deployment target to at least iOS 17. Staging rejects dirty source,
foreign revision/version/resources and modified native bytes. Each install must
come from `build_native_sdk.py` at the same clean commit.

Example (run at repository root after committing the source):

```sh
python3 tools/release/build_native_sdk.py --target macos-arm64 --build out/rn-sdk-macos --output out/rn-final/macos --offline
python3 tools/release/build_native_sdk.py --target ios-arm64 --build out/rn-sdk-ios-device --output out/rn-final/ios --offline
python3 tools/release/build_native_sdk.py --target ios-simulator-arm64 --build out/rn-sdk-ios-simulator --output out/rn-final/simulator --offline
python3 tools/release/native_package.py --install macos-arm64=out/rn-final/macos --install ios-arm64=out/rn-final/ios --install ios-simulator-arm64=out/rn-final/simulator --output out/rn-final/package --revision "$(git rev-parse HEAD)"
```

Android additionally needs the accepted NDK 27.2.12479018 (`--ndk <path>`).
Install the staged package using `npm install <absolute-package-path>`; source
`sdk/react-native` intentionally contains no native binaries and is not a
self-contained consumer package.

For local `file:` symlink development, add the staged package's real path to
Metro `watchFolders` and the app's `node_modules` to `resolver.nodeModulesPaths`.
On Apple Silicon simulator builds, request `ARCHS=arm64 ONLY_ACTIVE_ARCH=YES`;
the development XCFramework does not contain an x86_64 simulator slice. Set both
Podfile and the application target to iOS 17 or newer.

# iPhone physical smoke verification — 2026-11-01 (Asia/Shanghai)

SDK source: `d7ce720aa4f6b139011a9593e2623535e1d3adb1`, version 0.2.0.
Review ancestor: `937feb840f176b1b78d035d6b7972ce12fe181b7`.
The SDK native library and staged source match the previously recorded native
manifest. The external consumer Shell in this directory adds only public
Controller controls/status and native application lifecycle; it owns no geometry,
canonical store or History. The consumer source and executable/bundle/library
hashes are recorded in artifact.json. Code signing changes framework bytes.

Physical device: iPhone 15 Pro, iOS 27.2 (24B119), UIKit/Fabric + Skia/Metal.
The initial Release App trapped in UIKit's no-UIScene-adoption check before SDK
startup. `startup-red.json` records the failure without device personal identity.
SceneDelegate + UIApplicationSceneManifest fixed the consumer. Both original and
fixed build logs remain local; the final signed Release build, installation,
launch and UI were checked. JS is bundled, with no Metro connection needed.

The user physically drew strokes and confirmed the explicitly listed erase,
Undo/Redo, pinch zoom, detach/reattach and background/foreground checks normal.
The Runtime log independently records 16 successful pointer commits. The final
screenshot reads 9 objects, generation 23, contacts 0, zoom 1.21. This is a manual
smoke observation, not exhaustive pixel/performance, all-brush or platform
qualification. Native IME, real pressure and remaining platforms remain open.

Reproduce using the existing RN 0.83.10 standalone consumer with matching staged
native package, react-native-safe-area-context and the three consumer source files
in this directory. Set Podfile and Xcode deployment target to iOS 17. For a local
file symlink configure Metro watchFolders + consumer nodeModulesPaths when
bundling. Build with the caller's own signing team and a distinct bundle ID:

```sh
xcodebuild -workspace ios/AxiomSdkConsumer.xcworkspace -scheme AxiomSdkConsumer \
  -configuration Release -sdk iphoneos -destination generic/platform=iOS \
  -derivedDataPath <output>/build -allowProvisioningUpdates \
  DEVELOPMENT_TEAM=<team> CODE_SIGN_STYLE=Automatic \
  PRODUCT_BUNDLE_IDENTIFIER=dev.mostorm.axiom.rn.physical \
  IPHONEOS_DEPLOYMENT_TARGET=17.0 -jobs 6 build
```

Use `devicectl device install app` and `device process launch --console` with the
selected connected device. No UDID, certificate, provisioning profile or raw
personal crash report is committed. Screenshot capture works; the device rejected
screen-record capability. The evidence hash manifest covers the files here.

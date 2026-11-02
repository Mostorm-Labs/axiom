# SDK 0.3.0 release

SDK 0.3.0 is the next minor feature iteration after the immutable 0.2.0 tag.
It publishes the React Native package matrix:

- `@mostorm/axiom-react-native`
- `@mostorm/axiom-react-native-windows`
- `@mostorm/axiom-react-native-android`
- `@mostorm/axiom-react-native-ios`
- `@mostorm/axiom-react-native-macos`

The Web package remains `@mostorm/axiom` and is published by the existing SDK
release workflow at the same tag. All packages use the restricted Nearhub
registry and the same exact source revision. The shared React Native package
contains only TypeScript/API code; each platform package contains its adapter
source and matching `sdk/native` Runtime artifact.

Windows intentionally has two binaries: `MostormAxiom.dll` is the RNW adapter
built by the consumer, while `axiom.dll`/`axiom.lib` is the pure Runtime. These
are different ABI layers and must both be present in the application.

The release workflow performs clean locked Semantic/Skia native builds for
macOS, iOS device/simulator, Android arm64/x86_64 and Windows x64, stages all
five RN packages, validates manifests/hashes, and publishes only from the
matching immutable `v0.3.0` tag. Physical qualification remains separate from
package publication.

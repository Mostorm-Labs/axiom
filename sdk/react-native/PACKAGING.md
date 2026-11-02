# React Native package layout

The React Native SDK is published as one shared package and one platform package.
The shared package is `@mostorm/axiom-react-native`; it contains the DOM-free
TypeScript Controller, DTOs, codegen source types and the shared API imports.
It deliberately contains no native library, framework, asset bundle or private
Runtime header.

Each platform package depends on the shared package and contains only its own
adapter plus the matching `sdk/native` Runtime build:

| Package | Adapter | Runtime payload |
| --- | --- | --- |
| `@mostorm/axiom-react-native-windows` | RNW/C++/WinRT `MostormAxiom.dll` | `axiom.dll` + `axiom.lib` |
| `@mostorm/axiom-react-native-android` | JNI/Fabric `libaxiom_react_native_android.so` | `libaxiom.so` per ABI |
| `@mostorm/axiom-react-native-ios` | Fabric/Objective-C++ | `axiom.xcframework` |
| `@mostorm/axiom-react-native-macos` | RN macOS/Objective-C++ | `axiom.framework` |

The Windows pair is intentional. `MostormAxiom.dll` is the RNW activation and
adapter binary; `axiom.dll` implements the shared native C ABI. They are built
from different targets and must not be renamed or substituted for each other.
The same separation exists on Android between the JNI adapter and `libaxiom.so`.

All five packages use the same numeric SDK version and exact source revision.
`manifest.json` records the package format, target, Runtime identity, file hashes
and `qualification = NOT_QUALIFIED` until platform release evidence is complete.

Stage a release from a clean commit:

```sh
python tools/release/native_package.py --common \
  --output out/rn-release/common --revision "$(git rev-parse HEAD)"
python tools/release/native_package.py --platform windows \
  --install windows-x64=out/rn-release/native-windows \
  --output out/rn-release/windows --revision "$(git rev-parse HEAD)"
```

Install the common package and exactly one platform package in the consumer app.
The Web SDK remains the separate `@mostorm/axiom` package and is unchanged by
this matrix.

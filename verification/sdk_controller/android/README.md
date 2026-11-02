# Android RN input regression

`CanvasInputInstrumentation.java` runs inside an external React Native consumer
using the matching staged Axiom package. It finds the actual Fabric canvas and
dispatches Android `MotionEvent`s through its real View/JNI/Runtime path.
Single-finger `MotionEvent.obtain` uses Android pointer ID 0. Runtime reserves
zero as invalid, so omitting the Host's identity mapping must fail the first
contact assertion. It also checks commit, Undo/Redo, cancellation and ID reuse.

This is synthetic on-device regression evidence. It does not establish physical
touch, visible preview/canonical pixels, latency, IME or platform qualification.
Private reflection is confined to the test for reading the existing Session
state; no inspection API or business input shortcut is added to the SDK.

Use an RN Android consumer with bundled Release JS and controls/state startup
equivalent to the physical-test consumer. Copy this Java file into its
`android/app/src/androidTest/java/dev/mostorm/axiom/verification/` directory.
Configure the **consumer** (not the distributed SDK):

```groovy
android {
    testBuildType "release"
    defaultConfig {
        testInstrumentationRunner "dev.mostorm.axiom.verification.CanvasInputInstrumentation"
    }
}
```

The test uses Android's built-in Instrumentation and the consumer's actual RN
and Axiom dependency, without another test framework. Build and run from the
consumer, selecting only an ABI supplied by its matching native package:

```sh
cd android
./gradlew :app:assembleRelease :app:assembleReleaseAndroidTest \
  -PreactNativeArchitectures=arm64-v8a
adb install -r app/build/outputs/apk/release/app-release.apk
adb install -r app/build/outputs/apk/androidTest/release/app-release-androidTest.apk
adb shell am instrument -w \
  <consumer.applicationId>.test/dev.mostorm.axiom.verification.CanvasInputInstrumentation
```

Require `status=SYNTHETIC_VIEW_JNI_RUNTIME_PASSED` and
`INSTRUMENTATION_CODE: -1`. The `adb` process exit code alone is not the test
result. With the broken zero-based mapping, the expected failure is
`Android pointer ID 0 did not enter Runtime`. Installation/instrumentation
restarts the test app and its in-memory document; run this before the separate
manual touch checklist and retain those results independently.

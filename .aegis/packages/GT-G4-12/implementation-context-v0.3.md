# GT-G4-12 Android v0.3

The existing G4 host and Android Arc bridge are present, but there is no
installable Android application shell. This package adds only the Android
Activity/SurfaceView/JNI seam and durable device evidence. Runtime semantic,
scene, render, interaction, and ink ownership remains unchanged.

The first incomplete action is to add a Gradle app under
`apps/ink_playground/platform/android`, then build/install it on the connected
arm64 Pixel 7. Functional smoke accepts touch or a non-pressure input. Pressure
is recorded as unavailable unless the device reports a real stylus with varying
pressure. No physical or pressure PASS may be emitted by P32.

Native implementation targets: RNW/D3D12+Composition, RN macOS/AppKit+Metal,
iOS Fabric/UIView+Metal, Android Fabric/View+EGL/GLES3. All four use the
private Session/Runtime owner path and the shared `axiom` C ABI; platform
adapters are packaged separately from the pure Runtime library.

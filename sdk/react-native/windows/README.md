# React Native Windows 0.83.3 binding

`AxiomModule.h` registers the `AxiomNative` control module on the RN UI dispatcher.
It links the matching `axiom_native.dll` consumer library and public C header.
`AxiomCanvasView` must host a child HWND below the Fabric component and feed
WM_POINTER batches to the C ABI; canonical and preview use D3D12/Skia providers.
Build and UI qualification require Windows/MSVC 19.51 with the locked SDK ABI.

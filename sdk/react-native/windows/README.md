# React Native Windows 0.84.0 development binding

`AxiomModule.h` registers the `AxiomNative` control module on the RN UI dispatcher.
It is the RNW adapter DLL `MostormAxiom.dll`; it links the matching pure
Runtime `axiom.dll` (and `axiom.lib`) through the public C header. Both files
are required at application runtime and have different binary responsibilities.
`AxiomCanvasView` must host a child HWND below the Fabric component and feed
WM_POINTER batches to the C ABI; canonical and preview use D3D12/Skia providers.
Build and UI qualification require Windows/MSVC 19.51 with the locked SDK ABI.

The Windows consumer uses RN 0.84.1 / React 19.2.3 / Node 22. Its unpackaged
Fabric host needs app-local Windows App SDK initialization. The provider DLL is
named `MostormAxiom.dll` to match the C++/WinRT activation namespace; the pure
engine remains `axiom.dll`. The canvas HWND is placed above RNW's sibling
ContentIsland within its layout rectangle so native pointer input can reach it.
These integration fixes do not change the public C ABI or protocol. Drawing and
preview remain pending physical revalidation; SDK 0.3.11 remains NOT_QUALIFIED
until the release evidence contract is complete.

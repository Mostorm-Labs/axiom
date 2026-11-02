# `@mostorm/axiom-react-native-windows`

Windows RNW adapter package. Install it together with
`@mostorm/axiom-react-native`; this package contains the RNW/Fabric adapter and
the pure Runtime pair `MostormAxiom.dll` and `axiom.dll` at application build
time. `MostormAxiom.dll` is the WinRT activation/adapter binary; `axiom.dll`
is the shared C ABI Runtime and is not interchangeable with the adapter.

# SDK and demo preview releases

`sdk-preview-release.yml` builds Windows x64 and Web/WASM using the locked
published Semantic and Skia SDKs. It produces four assets:

- `axiom-sdk-windows-preview.zip`: the C++ Runtime and Windows adapter, locked
  Semantic/Skia libraries, their licenses, and an external consumer example.
- `axiom-demo-windows-preview.zip`: the Minimal Host executable, ICU data,
  application fonts and license notices. Keep the archive contents together.
- `axiom-sdk-web-preview.zip`: the C++ Runtime and Web adapter, locked
  Semantic/Skia libraries, their licenses, and an external consumer example.
- `axiom-demo-web-preview.zip`: the external SDK consumer, WebAssembly,
  resources, license notices and a loopback HTTP server. Run `python serve.py`.

Each archive contains `manifest.json` with the source commit and per-file
hashes. The release also includes `SHA256SUMS` for the four archives. SDK
packages are coupled to the exact compiler and dependency manifests enforced
by their CMake configuration: Windows requires x64 MSVC 19.51, Release and
`/MT`; Web requires the locked Emscripten 6.0.6 toolchain and Release. Follow
the README in the extracted SDK and configure its included example. The Web
Demo is the external SDK consumer, not the internal Minimal Host Debug UI.

Pull requests and manual workflow runs build and test without publishing. A
push of a new `axiom-sdk-preview-*` tag builds both platforms, runs focused
tests and independent consumers, then extracts each SDK archive and builds and
executes its included consumer against the included dependencies. CI rejects
missing, skipped, mismatched or corrupt assets. It creates a draft only after
both platform jobs pass, uploads the exact four verified assets and checksum
file, then publishes an immutable GitHub prerelease. Existing releases are
never overwritten by this workflow. Investigate a failed draft rather than
deleting or replacing assets silently.

This preview is not a stable ABI, production release, or acceptance Gate.
Physical input and IME coverage is incomplete; applications own persistence,
network and durable-close coordination. The known D07 Protobuf/Abseil
allocation-failure hang remains unresolved.

# Initial Windows SDK adapter sample

The sample creates a Win32 window outside the checkout and links only the
installed `Axiom::PlatformWindows` target with its declared locked Semantic and
Skia dependencies. It drives one public Composition, D3D12 canonical and preview
providers, synthetic pointer facts, a Shape selection, actual provider Present
feedback, resize and shutdown. The provider Present outcome does not prove
physical scanout. It does not validate touch, IME, font delivery, Debug UI,
long-running input, or an installable Web SDK.

Run `run_external.py` in the matching MSVC environment using the same `--build`,
`--semantic-root`, `--skia-root`, and `--output` arguments as the other C++ SDK
consumer. The test returns 77 if D3D12 is unavailable; the runner treats a skip
as a blocker rather than a pass. The `RuntimeComposition` must outlive its
Windows adapter. Both are owner-thread objects; destroy/shutdown the adapter
before closing and destroying the Composition.

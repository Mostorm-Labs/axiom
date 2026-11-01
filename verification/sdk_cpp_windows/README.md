# Initial Windows SDK adapter sample

The sample creates a Win32 window outside the checkout and links only the
installed `Axiom::PlatformWindows` target with its declared locked Semantic and
Skia dependencies. It drives one public Composition, D3D12 canonical and preview
providers, the application's supplied font catalog, synthetic pointer facts, a Shape selection, actual provider Present
feedback, resize and shutdown. The provider Present outcome does not prove
physical scanout. It does not validate touch, IME, installed SDK font delivery, Debug UI,
long-running input, or an installable Web SDK.

Run `run_external.py` in the matching MSVC environment using the same `--build`,
`--semantic-root`, `--skia-root`, and `--output` arguments as the other C++ SDK
consumer. The test returns 77 if D3D12 is unavailable; the runner treats a skip
as a blocker rather than a pass. The `RuntimeComposition` must outlive its
Windows adapter. Both are owner-thread objects; destroy/shutdown the adapter
before closing and destroying the Composition.

The runner copies the repository's accepted application font fixtures into the
independent consumer directory. The installed SDK does not currently distribute
those fonts; the application supplies both paths to `loadHostFonts`. This method
loads the currently checked-in catalog rather than arbitrary user font files.
Call it once during startup, before binding other document resources; the
existing loader replaces the Runtime resource manifest.

Each Down is submitted with `beginContact`, which returns the Runtime-assigned
source/pointer/generation key. Keep that key with the native contact and pass
it to `submitContact` for later samples or `cancelContact` for capture loss.
Never look up the current generation by a reused pointer ID when processing a
delayed observation. One continuation batch contains samples for one contact;
its terminal Up or Cancel, if present, must be last. Source loss is delivered
through `unregisterSource`. The host continues to filter stale native OS
observations before associating them with a stored contact key.

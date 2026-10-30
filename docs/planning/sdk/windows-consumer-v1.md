# Windows consumer SDK v1 execution contract

The user authorized JS frame closure, complete Windows DLL consumption and a
matching prerelease in that order. This record binds the implementation choices
for that already authorized work at base
`0074763f2a5d167b24887c37558904e942a502a6`; the historical SDK-01 candidate and
native Canvas C ABI proposal remain unchanged.

The consumer binary is `axiom.dll`. It statically consumes the accepted locked
dependencies internally. Public C++ clients include the SDK header and link
only its import stub; explicit LoadLibrary clients may use the C entry points.
No caller must find/link Semantic, Skia or Runtime component archives. The DLL
must not initialize a Runtime inside DllMain.

The DLL boundary uses ABI-versioned POD config/input records, opaque monotonic
handles and SDK-owned response buffers with size/copy/release functions. No STL
container, C++ exception, CRT allocation or internal Semantic type crosses it.
The C++ facade is compiled in the application and owns its strings/vectors and
JSON readback values. Its small bundled JSON header is an implementation helper,
not a link dependency. Commands and readback use the same bounded, explicitly
decoded control vocabulary as the JS SDK; no arbitrary semantic JSON Patch,
operation construction or new History owner is introduced.

One handle owns one existing RuntimeComposition, public Facade and Windows
adapter on its creating thread. Multiple handles are independent. A handle is
never reinterpreted as a pointer; unknown, destroyed, wrong-thread or foreign
contact identities reject. Caller callbacks are borrowed, owner-thread,
non-reentrant and cannot throw across the boundary. Operation bytes are real
encoded local operations, not notifications of persistence.

Window input/IMM32 adapters retain real native facts, source/pointer/generation,
history, cancel, focus, candidate geometry and confirmed composition semantics.
They reuse the current validated Minimal Host collectors. The application still
owns its HWND/message loop, storage and networking; the SDK supplies attach,
native-message and frame/pump facilities, not a second platform application.
Controls, selection, Group/Section, rich text, Image/Crop, Connector/Sticky,
snapshots, operations and resources reuse their existing Runtime owners.

Control close must finish native composition through the existing effect/fact
lane. A pending close is not success and must not destroy a borrowed HWND or
silently discard composition. Force/abandon teardown is distinct and makes no
asynchronous IME or persistence guarantee. No callback or borrowed HWND access
may continue after successful shutdown.

Assets have an explicit module-relative or configured root, independent of CWD.
Required ICU data and the provided font catalog remain shipped runtime assets
unless embedded through a separately verified loader. Licenses remain included.
The default release contains DLL, public headers, import stub, assets, CMake
consumer config and an actual out-of-tree application. Component archives and
full dependency SDKs are not consumer payload.

Required proof includes the real pending-frame browser RED/GREEN, ZIP and npm
external JS/TS consumers, strict native DLL build, import/export inspection,
outside-checkout C++/LoadLibrary consumers, multiple Runtime/CRT/lifecycle and
stale-identity rejection, supported facade command coverage, real native HWND
input/IME routing, existing affected regressions and same-revision artifacts.
Synthetic tests remain distinct from physical IME/touch validation. The known
D07 OOM and separately recorded legacy failures are not silently closed.

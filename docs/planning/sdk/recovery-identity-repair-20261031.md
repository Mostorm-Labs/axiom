# SDK recovery identity and regression repair — 2026-10-31

This bounded repair follows the user's explicit request on branch
`codex/sdk-shell-axis-bridge`, starting at
`1a8bc6a037d6943682ffcc304829530b646cf424`. Authorization is materialized in
`.aegis/packages/AXIOM-SDK-RECOVERY-REPAIR-20261031-v0.1/package.json`.

## Runtime identity

The unchanged external recovery probe fails against the published DLL: two
different edits on either side of checkpoint restoration share OperationId
`00000000000000800000000000000000`. This is a product defect: an Axis journal or
outbox using OperationId for deduplication could discard a genuine new edit.

Composition now obtains a fresh 128-bit origin from `std::random_device` before
opening its Runtime, including the candidate Runtime used for checkpoint restore.
The supported MSVC toolchain uses OS randomness; the locked Emscripten toolchain
routes entropy to Web Crypto. An unavailable entropy source or zero origin fails
open rather than falling back to a clock or process counter. Entropy failure was
not artificially injected in this run.

The Interaction owner maps its disjoint local drawing and History ordinals through
that origin before preparing canonical operations. The mapping is injective within
an owner; cross-owner identity is collision-resistant rather than an absolute
distributed uniqueness guarantee. Brush, Shape/Connector creation, eraser, command
and Undo/Redo paths use these identities. Numeric drawing object IDs, order keys,
resource identities and caller-supplied legacy IDs retain their existing contract.
Remote and replay operations retain their original IDs. The SDK callback does not
rewrite IDs, and the snapshot does not invent a pre-checkpoint operation ledger.
No wire schema, descriptor lock, SDK dependency lock or exported DLL ABI changes.

`SDKPublicOperationIdentity` exercises three restored owners, new edits, actual
Undo/Redo, and three independent owners using the Brush, Shape and object-eraser
input paths. The same published-DLL probe now passes against the rebuilt installed
DLL in two separate processes; all four observed operation IDs differ. Both real
external consumer suites cover recovery followed by edit and Undo/Redo as well.

## Two previously failing regressions

`FC02WindowsTouchShapeLabelEntry` is retained. Its synthetic Win32 fixture mixed
requested event spacing with synchronous host rendering time. The diagnostic
failure records Up timestamps 335795031 and 335795593 ms: the supposed rapid pair
is actually 562 ms apart, beyond the production 500 ms double-tap threshold. A
lighter diagnostic run passed, confirming timing dependence.

Only this fixture's injected native clock is now deterministic. The real host
window procedure, Router, Facade and Runtime still execute. A deliberate 600 ms
processing delay leaves the synthetic source timestamps unchanged. Existing slow
pair, movement and multiple-contact rejection assertions remain. Production tap
thresholds and native timestamp handling are unchanged. The full matrix passes;
three additional consecutive runs also pass. This is synthetic-input validation,
not physical touch or IME acceptance.

`G4InkPlaygroundSelection` was a stale application fixture: it submitted input
without a surface after the old application had already been removed. Following
the user's cleanup instruction, its CTest registration, mode and function are
deleted. Its useful invariant moves to `SDKPublicSelectionInput`, using the actual
public Runtime owner: unbound input rejects, bound selection input succeeds,
canonical commit fence/object count stay unchanged, and no Undo becomes available.
Other qualification-only tests have distinct coverage and are retained. Historical
failure records and old package bindings are not rewritten.

## Actual verification

See [the result and raw evidence](evidence/20261031-recovery-identity/result.json)
and [exact commands](evidence/20261031-recovery-identity/commands.json).

- Windows Release MSVC 19.51 `/MT /WX`: build and SDK install pass.
- 92 retained regressions plus the two new public-owner tests: **94/94 pass,
  zero skipped**. The original 93-test failure report remains immutable.
- Touch entry: **three consecutive additional passes**.
- Python descriptor, semantic, host and font contracts: **18 pass, zero skipped**,
  with the resolved `AXIOM_PROTOC` environment actually imported.
- Installed Windows SDK: fresh application outside the checkout links and runs
  against the DLL, verifies 16 exports and absence of internal dependency archives.
- Actual Chrome/WASM: **52 general assertions, 20 Shell/Axis assertions**, frame
  regressions and shipped-example mouse/pixel checks pass. Native debug output on
  the browser console is retained; no uncaught browser exceptions were observed.
- Published recovery probe: RED before repair; unchanged probe GREEN in two new
  processes against the installed repaired DLL.

The local test artifacts embed base revision `1a8bc6a0` with `dirty=true`; the exact
tested source bytes and artifact hashes are recorded in the evidence manifest.
This report's containing Git commit materializes that source/evidence pair. These
artifacts do not replace the published preview release.

D07 Protobuf/Abseil injected-OOM work remains deferred. Physical acceptance, full
SDK acceptance, DEV/P34/PHYS and release readiness are not claimed.

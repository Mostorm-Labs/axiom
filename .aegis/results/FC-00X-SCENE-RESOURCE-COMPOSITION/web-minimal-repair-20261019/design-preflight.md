# Authorized Web Minimal consumer narrow repair

Authorization: user explicitly requested repair of the correctly identified Web consumer's
input, presentation and RuntimeFacade wiring. Start revision is
621388791a809ec19d721b5a67b14f3d6afa72aa; upstream baseline remains
f1d72f22ad20209c8ccdbdd7474d86c651434914. Reuse the isolated worktree and branch.

Owner graph stays WebSurfaceHost -> RuntimeComposition -> existing owners. Optional Web
DebugController borrows composition.controlFacade(), just as the Windows Minimal controller
borrows its facade. The executable alone links Debug UI when enabled. No Playground linking,
private editor access, new stable ABI, gesture recognizer or canonical mutation path.

REDs: prior correct-entry audit proves three Number-to-BigInt failures and zero received
pointer samples. The new browser regression fails with missing Minimal frame/control entry.
Both run actual generated WASM. Synthetic DOM tests stub capture only and do not prove physical input.

Target input: PointerEvent -> one logical coordinate mapping -> source homogeneous platform
samples. Input ingress owns generations. Echo full keys on move/up/cancel; reject stale keys.
Cancellation precedes resize/rebind/context loss/close and tool changes. UI owns its pointer
sequences; only pointer events reach the canvas (no promoted mouse listener).

Presentation: remove duplicate legacy backend wrapping of the same WebGL context; stage one
canonical/preview provider pair. Demand-driven rAF delegates work selection to composition.
Keep exact work tokens until a later browser yield and corresponding GL fences signal. Submission
alone never completes work. Fence completion is Web backend execution feedback, not a claim
of physical monitor observation or a measured display timestamp. Pending/stale/lost/failure
paths retain or invalidate tokens; they never fabricate a successful completion.

Validation: real browser pixel readback, repeated Ink, Shape/Line/Arrow, Undo/Redo, cancellation,
generation reuse, frozen 250 ms AutoIntent, Debug capture, fractional DPR resize and idle;
Web Debug ON/OFF dependency and artifact checks; existing native Web surface/composition/facade
and frame retry regressions. Physical checks remain pending. No Authority/Golden/history/PHYS
or Android changes. Existing two baseline Clang diagnostics remain recorded separately.

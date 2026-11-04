# macOS native viewport input (RN 0.3.0)

Execution anchor: `Mostorm-Labs/axiom@39028aca9692de5b9c41c2c409700f05ee4cc48d`
on `codex/react-native-sdk`. The user authorized AppKit pinch/scroll adaptation
and local verification. This extends the existing RN development contract; it
does not qualify the SDK or change semantic authority or published SDK 0.1.0.

Ownership: AppKit collects `magnifyWithEvent:` / `scrollWheel:` facts in view
logical coordinates. The Native Session validates its opaque token and exact
attachment generation, then Composition delegates to the existing
`CanvasEditingRuntime::applyViewportNavigation` / `ViewportInteractionController`.
Runtime owns camera, scale limits (0.25–8), anchor math, and invalidation of both
canonical and preview. No TS/native host camera, synthetic finger positions,
geometry, History or canonical document owner is introduced.

The additive host-only `axiom_native_navigation` C/POD entry point accepts one
borrowed record synchronously: size, ABI version 1, kind (pan or relative zoom),
logical delta/anchor and scale factor. It retains no pointer/buffer. Calls are
UI-owner-thread only, non-reentrant, and require a live matching attachment.
Unknown kinds, versions, non-finite values, nonpositive scale, foreign/closed
tokens, stale generation and detached views reject without camera mutation.
OK means Runtime applied navigation, not that a frame, document Operation or
durability acknowledgement was produced. Presentation uses the existing frame
service. This adds no JS API or changes to existing ABI record layouts.

AppKit magnification is incremental (`factor = 1 + event.magnification`) at
the event's view-local anchor. Scroll deltas reverse into Runtime's subtractive
pan convention; precise deltas are logical points, wheel notches use ten points.
Ended/cancelled events apply any final nonzero delta but retain the last camera;
zero terminal deltas do nothing. Momentum scroll continues through the same lane.
No gesture state survives detach; resumed events target only a current binding.

EXECUTION_CLOSURE_CONTRACT:
- Required: C navigation admission, Composition delegation, AppKit event mapping,
  matching header/export/package docs; rebuild the standalone Release consumer.
- Forbidden: dependency/descriptor locks, semantic schema, new viewport owner,
  JS geometry/input shortcut, Metro runtime dependency, other-platform claims.
- Required oracles: AppKit pinch/scroll anchor math and direction; repeated
  gestures and terminal/cancel; no document/History mutation; active preview
  camera alignment; invalid/stale/thread/lifecycle rejection through actual C
  ABI; SDK registry/C/package tests; runtime boundary check; `git diff --check`.
- Local-only host tests are supplemental; real touchpad observation is recorded
  separately. No hosted verification is required for this bounded Mac iteration.
- Terminal success: tested native adaptation plus exact clean-revision native
  package and standalone App launched with Metro absent, ready for user testing.
  Missing physical input evidence remains explicit, not a simulated PASS.
- Blockers: authority conflict, unavailable locked dependencies/toolchain,
  failed required oracle, or inability to launch actual RN Mac consumer.
- Continue until terminal state; preserve evidence and never issue Gate/P34 PASS.

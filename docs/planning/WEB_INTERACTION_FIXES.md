# Web interaction correction slice

Repository github.com/Mostorm-Labs/axiom, ancestor ff36e1d4719d6cd5cdc4e95439ce2de1cb147aa8.
User authorizes a new branch, diagnosis/correction of rectangle flashing, zoom-out
jumps, natural-scroll pan direction, rectangle/lasso selection and Group rejection,
with a matching external Web demo for acceptance. No automatic merge/publication.
Current authority is located through docs/notion/manifest.yaml. Public JS contracts
are sdk/web/axiom.mjs + axiom.d.mts and docs/planning/sdk/js-v1-contract.md; existing
Runtime acquisition/hierarchy/navigation/presentation owners remain authoritative.

Owners: DOM extracts native wheel/pointer facts; SDK facade adapts transport;
CanvasEditingRuntime owns selection/creation/viewport and semantic OperationEngine
commits; SkiaRenderer owns derived feedback and provider presentation. Shell owns
layout/UI. No duplicated editing, brush or document owner or private SDK imports.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: reproduce each report independently using real exported SDK + matching
  WASM, including toolbar layout and ink-heavy selection; fix owning layer and
  retain generation, stale command, canonical-visible/preview retirement invariants.
- required: native/Node fail-first regressions for changed behavior; real browser
  tests for natural wheel sign, complete rectangle/lasso acquisition, group/ungroup,
  selection cancellation, stable drawing-buffer/lifecycle and presentation through
  rectangle creation; repeated small zoom-out display updates with no canonical
  document mutation or approximate CanonicalVisible.
- blocking verification: affected native/render/acquisition/Group/navigation tests,
  Node facade/contracts, matching external SDK browser + handoff + navigation,
  public packaging/path checks, boundaries/ABI/workspace, runnable separate demo.
  Root CTest is diagnostic with exact failed names retained, not silent authority repair.
- evidence: before/after outputs, source/artifact hashes and explicit limitations.
  Local repository evidence + external demo is the result boundary. Hosted/physical
  tests are corroborative; no physical-platform/Gate/P34 PASS from synthetic input.
- forbidden: dependency/descriptor locks, brush geometry/materials/schema changes,
  new consumer debug APIs, incomplete coverage canonical receipt, rewriting earlier
  evidence or modifying other chats' worktrees/processes.
- terminal: all four report groups corrected and verified, or exact environment,
  authority, input or newly unresolved regression blocker; hand user a test URL.

2026-11-02 resize follow-up: the user's latest correction replaces center-anchored
window resizing with a fixed Runtime camera and top-left logical pixel origin.
The full-viewport Shell has floating tools/board, properties and status. The SDK
coalesces live mount resize (120 ms), flushes before new pointer collection and
retains display pixels until qualified new-surface completion. This changes only
Shell layout and platform display scheduling; paired provider generation, semantic
document ownership and canonical completion qualification remain unchanged.
Regression evidence: `verify_live_resize.mjs` runs against an external SDK copy
with real Chrome/WebGL2. It checks pixels, camera, surface buffer resets, final
metrics, immediate drawing, close cancellation and chrome/layout independence.

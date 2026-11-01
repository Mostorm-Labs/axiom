# Exact GPU coverage follow-up

Repository github.com/Mostorm-Labs/axiom, anchor a1f724cd (ancestor), working
branch codex/g5-settle-timing with prior dirty exact-coverage slice preserved.
User authorizes continued optimization and coverage pixel alignment.

Owner/scope: existing shared Skia Render Adapter and private derived work.
Pinned compactCoverage/coverage shaders, CPU Direct byte oracle, semantics,
SDK contracts and dependency locks remain unchanged. No platform shader renderer.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- diagnose coverage, quantization, sampling/material boundaries independently;
  no arbitrary half-pixel shift or tolerance weakening.
- correct exact execution under compatible GPU targets when byte oracle permits;
  a conservative hybrid GPU interior / pinned CPU edge correction is allowed
  as a Render culling optimization, without new brush geometry semantics.
- gate unsupported material/version/transform cases to existing CPU fallback;
  budget/target loss and no partial canonical receipt remain enforced.
- required: real WebGL differential for tuned round/marker/taper/caps/affine/DPR,
  existing cache/native regressions and matched external SDK browser/handoff;
  16/64 revision-3 timings, bounded temporary work accounting, source/artifact hashes.
- terminal success: qualified applicable improvement and matching independent demo;
  otherwise preserve CPU default and report exact blocker and measured results.
- physical platform evidence remains BLOCKED_EVIDENCE; no G5/P34 PASS.

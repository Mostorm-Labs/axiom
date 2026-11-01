# Shared exact coverage execution slice

Repository github.com/Mostorm-Labs/axiom. Trusted anchors: brush tuning
 de274161 and timing 8484eba2, reconciled at a1f724cd. User authorizes GPU
coverage → bounded reuse/work → cooperative exact settlement. G5 v0.3 WP-11..19
and existing shared Skia/material authority are consumed, not redefined.

Owners: immutable confirmed brush geometry stays in Runtime/Scene. Skia-private
coverage targets and cache jobs stay in Render; providers own contexts/present.
No public JS/DLL API, canonical schema, brush algorithm, SDK lock or private pool.

EXECUTION_CLOSURE_CONTRACT:
- continue_until_terminal_state: true
- required: compatible GPU coverage targets under the existing Skia adapter;
  CPU Direct remains unchanged as oracle/fallback. No hot-path readback.
- required: bounded reusable temporary targets and work accounting, local
  content/resource/metrics/surface validity and loss/resize cancellation.
- required: exact preparation checkpoints within long strokes, demand supersession,
  bounded work admission instead of one atomic stroke per opportunity; no partial
  canonical receipt/present. Latest camera demand wins.
- required tests: fail-first GPU coverage execution, CPU/GPU differential,
  material/version/cap/taper/mask/affine/DPR/clip/seams, cache parity, reuse bounds,
  cooperative work and generation rejection, Runtime/ARC handoff unchanged.
- required verification: native G5 suite, real WebGL probe, matching root SDK
  browser/resize/context-loss tests and same 16/64-stroke timing scenarios;
  boundary/workspace checks. Keep known full-suite failures explicit.
- evidence: source/artifact identity, raw comparisons, observed timing/work and
  platform applicability; physical devices remain BLOCKED_EVIDENCE when absent.
- terminal: three applicable slices and required checks; if GPU oracle cannot be
  met, retain safe CPU fallback and explicitly report that GPU qualification is
  blocked rather than silently weakening pixel/semantic authority.

# G5 idle input feedback diagnostic — 2026-11-13

This is the finite diagnostic result for package
`.aegis/packages/G5-idle-input-feedback-diagnostic-r1/package.json`
(SHA256 `6720ee60d3fe8892c853d2d8a015894e0fd62f7f929441013ccad0fac76d35ca`).
It does not fix production behavior, qualify overall G5 performance or close the
user defect. The existing RR/MQ/CL changes, ordinary 4333 SDK and user pages are
preserved. No commit or push is part of this package.

## Outcome

The stable controls and the one real 1,200,003 ms visible idle did not reproduce
missing strokes at the sampled points. Strict idle qualification is blocked:
29 natural SDK RAF callbacks occurred during the interval, although GPU write
counts stayed unchanged, all 29 frames were `ok:true`, `pending:false`,
`retry:false`, and there were no observed lifecycle/contact/Worker events.
The observer did not retain hover/resize trigger facts, so their source remains
unknown. The existing no-button mouse hover and pointerleave handlers can schedule
frames; this is a source-supported possibility, not a causal finding.
`code:FRAME_REJECTED` and `code:INPUT_REJECTED` are present even in successful
transport envelopes; rejection requires `ok:false`.

All windows used isolated headed Chrome, DPR1, CSS 822×900, Fixed40 restored via
root SDK and camera scale .34 / translation (120,260). The brush was explicitly
selected as default `round-handdrawn` revision4 with unchanged configuration.
Initial root state has no selected package; the first driver attempt incorrectly
assumed Demo initialization and failed before drawing. That 5-second attempt is
preserved, followed by the single permitted corrective ordinary window, a private
short diagnostic and the single real idle window. Starts 4/4, charged time
1269/2220 seconds, diagnostic builds 1/1, ordinary builds 0.

## Actual witnesses

- Before idle: 42 objects, generation/presented 3/3, zero contacts and pending
  intents. After idle those identities and the camera remained equal.
- Both sequences delivered two 66-call mouse strokes and one 34-call object drag.
  Root errors, actual input/frame rejections and observation ring loss were zero.
- Each stroke had amber preview pixels at move16 and move32; settled canonical
  pixels and successful presentation were observed. Four AddStroke operations
  contain four distinct persisted objects with 64 knots each. The pre/postidle
  position and effective-pressure byte digests match for each corresponding path.
- Four new strokes and two transforms produced final 44 objects / generation6.
  Offline field comparisons preserve the original40 bodies, properties, placement
  and masks. Only the selected original object's transform changes.
- Both drags selected `80202ea91a27b2511dd5fd34cf6c40a5`. Requested view displacement
  was (+80,+30); actual was approximately (+81.9143,+30) before idle and
  (+80,+24.7001) afterward, with existing snapping unchanged. While Down,
  pending selection bounds and real canonical ROI pixels moved. These are not
  exact raw-pointer transforms or identical-pose performance pairs.
- Up-to-next-Down was about34ms and includes host/page boundary reads, without a
  settled wait. It is not same-DOM-task or zero-gap input.

First feedback is only an upper bound at move16, not a measured first photon.
The observer adds CPU and synchronous ROI readback overhead; diagnostic latency
is not ordinary performance. The postidle opaque strokes cover the same positions
as the preidle strokes. Settled black-pixel differences alone cannot prove new
strokes; amber while Down, distinct operations, counts and persisted geometry are
used together. There is no new full-scene CPU pixel oracle or AA qualification.

## Important missing precondition

The user subsequently confirmed repeated partial erasing before the symptom.
This Fixed40 snapshot already has 80 historical swept-circle masks: two per
object, each with32 segments, total2560. It is not an unmasked fixture. However,
the diagnostic restores those masks, performs no new erasures, and requires full
settlement before idle. It does not cover live consecutive erasure, outstanding
pending work, display handoff or resource history at the transition into idle.
It is also a different document from the user's22-object document.

Visible and focus events qualify only browser-visible state; physical OS
frontmost position was not continuously observed. The no-RAF rule is not waived.
No repeat idle or additional browser start was made after this qualification limit.

## Changes and verification

The only production-file additions are private timing-guard observation macros
and calls at existing display admission/seal/cover/retire, preview submit/ack and
Web fence/rejection seams. They do not change a decision, ownership or receipt.
Normal OFF macros evaluate no arguments, verified by a native compile/run probe.
The ordinary SDK's42 files are byte-identical to the starting manifest. One
separate diagnostic install uses locked prebuilt Semantic/Skia with timing ON;
no dependency was rebuilt. Source files' original mixed line endings were restored
before idle, with byte-normalized lexical equality to the diagnostic build input.

Verification: four Node observer tests, three Python offline field/clock tests,
and seven existing BrushBatch/transient-display tests pass. No full-project suite,
performance PASS, Brush/circle change, budget change or public API is claimed.
Exact records live under `.aegis/results/G5-idle-input-feedback-diagnostic-r1/`;
raw pixels, snapshots and operations under the matching `out/` directory.

## Minimal successor proposal

Continue diagnosis through the existing Runtime pendingLocal/CPU erase,
OperationEngine, display records, Composition and canonical/preview providers:
perform consecutive partial erasures in the same live document, then the next
draw/drag while work is pending and after settlement. Preserve mask revision,
pending serial/contact generation, display cover/retire and frame/fence identities.
Prefer the actual user document if available. Passive hover/resize trigger facts
would explain natural RAF without adding polling or prewarming. A further20-minute
window requires its own finite authorization. No behavior patch is justified yet.

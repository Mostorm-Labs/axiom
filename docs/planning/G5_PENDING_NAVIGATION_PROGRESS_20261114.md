# Pending erase navigation and preparation progress

The two production fixes are implemented and have separate actual RED/GREEN
proof. Browser qualification is still incomplete. Neither the parent package nor
the evidence IO supplement closes the whole G5 or establishes a performance PASS.

## Production ownership and behavior

`CanvasEditingRuntime::invalidateViewportChange()` shares the existing camera
invalidation for pan, anchored zoom, fit, and the actual two-contact input paths,
including selection mode. It updates the retained preview transform, display
revision/roster, navigation demand and pending erase transient dirty state. The
controller's generation semantics remain unchanged. Pan input avoids invalidating
the same camera twice. No shell, SDK, Composition, Render or document owner was
added or changed.

The actual old Composition left pending erase pixels at the old camera after
zoom, fit and two-contact moves. Whole1431×1010 pixel comparisons failed until an
equivalent zero-pan redraw. The repaired paths match that redraw exactly while
the real Worker request is held and canonical generation remains unchanged.
Selection-touch and pan-tool guards are included.

The pending owner now compares constant-size progress facts owned by the real
Semantic target/finalization preparation, current Scene stamp cursor and
History/publication stages. Useful work continues under the original turn's
8000µs allowance and4096 iteration ceiling. Semantic advance receives remaining
time rather than a new8000µs per target. Actual Worker waits still yield;
unavailable surfaces and expired allowance do not publish or manufacture
progress. The allowance is cooperative, not OS preemption of an indivisible
target or atomic stage.

Before this change, the target, stamp, History and publication regressions used
about246µs,70µs,20µs and1µs respectively, then yielded to another owner opportunity
despite actual internal progress. The small three-target case can now finish
multiple preparation stages and atomic publication within one owner turn.
OperationEngine still commits atomically and in pending intent order.

## Validation and observed tradeoff

31 focused native tests passed, including failures, source cancellation, context
loss, timeline, mixed frame, History barrier and stamp abort/value changes. The
post-publication allocation failure was moved from a frame-count assumption to
the actual publication-success stage and proved to occur after atomic mutation.
The added Semantic test compares staged encoding/store results with the ordinary
sequential engine. Seven Worker/ordered-promise Node tests and120 runtime
boundaries passed. Existing locked Semantic/Skia SDKs were used.

Two counterbalanced ordinary Chrome runs and one IAB ordinary lane set restored
the same three-stroke fixture. Navigation and next-Down actually occurred while
erase was pending. Chrome Up→first erase operation callback decreased from about
205→39ms for zoom and212→51ms for pan. Preparation observations spanned about
175ms before and8ms after, with similar CPU work. Passive observations are not
individual RAFs: the observed preparation owner frames dropped from about11 to1.

However, first default-target GL content calls became later for zoom/pan, about
25→55ms and28→51ms. Those are submission proxies, not first correct camera pixels
or photons. This tradeoff remains an open finding, and no Render/budget tuning was
added to this task.

## Evidence IO failure and supplement

Parent package `G5-pending-navigation-owner-progress-r1` remains sealed as
`BLOCKED_EVIDENCE`. Its IAB page showed COMPLETE but only8 of12 rows were durable.
The server tried to stringify the entire base64 pixel aggregate. An actual
offline reproduction exceeded Node's536870888-character string limit and threw
`RangeError: Invalid string length`. The server returned404; the old page ignored
`response.ok`. A separate actual HTTP500 negative reproduced the false completion.
The original2Chrome/1IAB/167seconds and all saved buffers remain unchanged.

`G5-pending-navigation-evidence-io-repair-r1` changed only new verification tools.
Each row now saves independent gzip binary pixels, snapshots and operations,
then bounded metadata and a compact index. The client validates successful HTTP
and exact saved-key/count/hash ACKs. Missing, duplicate, unknown and persistence
failure cases are checked offline.

The sole supplemental IAB page saved only the four missing pixel cases, with62
binary buffers verified by byte count and SHA256. It charges118/180seconds;
the parent's167seconds are retained separately. One ancillary unknown-route
error is preserved without assuming its URL; all four row ACKs and consumer
statuses succeeded. Production files, both42-asset SDKs, parent drivers and
sealed results stayed unchanged. No SDK rebuild or new ordinary timing run
occurred.

All three actions' final full RGBA matches the corresponding baseline exactly.
For zoom/pan, non-edge pixels also match the reused independent Snapshot→Scene→
Direct CPU reference exactly. Original bodies/config/pressure/placement and added
mask geometry are preserved. Newly allocated identities vary between browser
Runtimes; exact same-input wire equivalence is established by the native test.

Both nextdraw lanes show visible amber pixels on the actual preview target while
erase is pending. The right edge advances along the input trajectory at move8
and16, and preview is hidden after canonical completion. Final object count alone
is not used as that proof.

The finite zoom/pan readbacks did **not** capture a completed current-camera
image while erase was still pending. Earlier captures still contained the old
frame; later captures followed commit. Dual full readbacks add roughly600ms and
perturb scheduling. This Web branch gap is preserved separately from the native
held-Worker whole-frame proof. Control has proposed a bounded deterministic
qualification that delays delivery of a real computed Worker terminal, with no
production change or ordinary performance claim. No4336 demo is promoted until
the required combined proof is resolved.

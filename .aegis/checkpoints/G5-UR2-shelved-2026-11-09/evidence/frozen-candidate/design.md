# Resolved implementation design

Owner: existing CanvasEditingRuntime and RuntimeCompositionCore. Entry remains
real PlatformPointerBatch/control -> ingress/interaction -> eraser/camera owner.
OperationEngine remains the only canonical mutation path. No consumer API,
dependency, GPU policy, budget, or snapshot UI changes. Existing UR1a work stays.

The current Begin clears the preview and synchronously presents canonical before
creating the next trace; Up applies before fallible History recording. Current
navigation accepts the camera but requires an exact frame with matching Scene.

Target transaction: independently recover current Scene before exact dependent
queries; prepare inverse, History, publication and bounded sealed metadata before
apply. Failure before apply cancels only that active gesture and changes neither
document nor History. After apply, move the real commit record, finalize reserved
History, seal the gesture and publish once without new mandatory allocation;
derived Scene/presentation failure leaves the committed erase accepted and dirty.
The existing noexcept observer port keeps its contract. No asynchronous Up.

Extend the existing erase trace owner with gesture serial/key and runtime-local
display lifetime; seal only identity/basis/commit metadata, never another geometry
or mask owner. Bounded pending records reject precommit resource exhaustion; no
record is silently evicted as if it was presented. No-op uses its existing state
basis without creating an Operation. Cancel/source loss cancel display demand
separately; context/metrics change invalidates proofs rather than issuing receipts.

Deferred partial Begin performs admission/trace setup and hides obsolete separate
overlay visibility only; no acquire, raster, clear or present. Frame opportunities
draw current Scene plus the active disposable mask into a complete replacement.
Committed masks appear only in Scene, so retained sealed masks are never reapplied.
Sync and object eraser behavior stays on its existing path.

Before an exact complete canonical submit, freeze eligible sealed serials and
their identity/basis against the synchronized Scene, commit fence and full target.
On successful renderer/coordinator/tracker submit retain this immutable roster.
Core.captureCanonicalWork associates only that roster with the actual work tuple
(runtime/lifetime/attachment/work/frame). Strict completion remains unchanged;
after it passes, retire only its associated frozen serials. A newer exact frame
may replace older feedback or state superseded by Undo/Delete without emitting an
old receipt. Active/sealed C created after preparation is never cleared by B.
Do not globally clear the shared transient while active or uncovered sealed erase
feedback remains. Failed submit/approximate navigation never retires sealed state.

Scene readiness and input admission are separate from pixels. Transient/exact
frames recover through the existing derived owner; Up independently recovers
before partialEraseTargets even if two Up events have no frame opportunity.

Navigation: keep the accepted old exact identity immutable. Build a separate
disposable current-Scene FrameState/query from synchronized semantic generation,
Scene revision, current camera and surface metrics. Feed the existing query
overload of SkiaRenderer::renderNavigation; existing retained-pixel proof decides
reuse. A Scene mismatch cannot be bypassed or restamped. Busy target keeps camera
demand/dirty progress and retries at existing opportunities; quiet-time exact
convergence remains bounded/fair. Navigation never occupies canonical completion
or qualifies sealed retirement.

Proof: native real v4 nonempty BusyProvider RED first, then current Scene/fault,
History preparation/publication/apply and handoff/lifecycle negative tests with
native Skia pixels at DPR1/2/4. Native actual input/control tests distinguish camera
acceptance, Scene readiness, feedback work and exact receipts. Reuse existing
busy/selection/History/erase/render checks and boundary checker. Build new normal
candidate from accepted locked SDKs, then only UR2H-C1/C2 (180s/3GiB serial), each
restoring exact S0/S2 in independent branches for continuous erase and immediate
real wheel/ctrl-wheel. Preselect before drag; cold oracle/UndoRedo outside timing.
Failures remain failures; no retry/old A/B rerun. Preserve4280/4281/4282 and serve
candidate at a new port. The roughly500ms frame leaf remains separate work.

Implementation details: sealed metadata is capped at128 pending records, reserved
before Up apply; exhaustion rejects before mutation rather than evicting proof.
History finalizes before the observer, and the complete engine record is moved
into lastAppliedCommit without copying. Source-level integration uses small
interaction-owned Scene/work methods rather than a reverse dependency/friendship
on concrete Composition. These are not installed/exported SDK capabilities.

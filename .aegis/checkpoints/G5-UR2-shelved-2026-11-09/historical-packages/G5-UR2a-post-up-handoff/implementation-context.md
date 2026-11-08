# G5-UR2a: unified post-Up edit and navigation handoff

This is the next independently testable production slice, not completion of all UR1 or UR2. The user explicitly stopped snapshot UI work and asks to progress product fixes. Control owns scope/review; the existing implementation thread is the sole source writer.

The last source anchor is14ce6221; retain the accepted dirty UR1a full transient/no-encoding builder. UR1a browser relative performance is unresolved. r2 is closed: four actual immediate-reerase failures in both A/B, semantic generation0->1 committed but presented generation0 and contacts0 on next Down. Eight drag rows lack initial point selection and are not performance evidence. The105-file r2 manifest was independently rehashed with no mismatch.

Current path: RuntimeAppBinding/InputRouter -> Coordinator -> CanvasEditingRuntime eraserBegin/Sample/Finish -> OperationEngine -> Scene -> existing SkiaRenderer/provider. eraserBegin currently clears/hides transient feedback then forces presentCanonicalFrame before admitting the new trace. eraserFinish clears the trace, applies Operation, then calls fallible history.record. Scene apply failure is deferred to recoverHistoryScene, currently often reached through rendering. Canonical receives strict identity in RuntimeCompositionCore and CanvasEditingRuntime; keep that proof unchanged.

Implement the bounded Runtime-owned active/sealed replacement lifecycle needed to remove the next-Down presentation gate safely. No new canonical owner, renderer, public token or thread. Scene latest-state readiness must be independent of pixel rendering. Use existing History prepareRecord/commitPrepared only for the erase transaction. A newer full exact frame may replace old sealed feedback with a frozen eligible set; it does not issue the old receipt and cannot clear a newer gesture.

Start with a real public/native BusyProvider RED regression on nonempty v4 stroke. Reuse existing tests and r2 S0/S2 fixtures/caller-owned restore envelopes. Then implement, run safety tests, build normal candidate, run two fixed functional browser workers. Fix the cold point-selection precondition in this task's driver only; leave old failed observations intact. Do not spend another window attributing UR1a. The existing500ms frame callback finding remains separate work.

Output: .aegis/results/G5-UR2a-post-up-handoff/result.json, exact artifacts and new unused-port Demo. Preserve4280/4281/4282 and unsaved user documents. No commit/push. Notify control01a1b6f3-e69a-7fe2-8d9b-0222e8741f03 at READY_FOR_CONTROL_REVIEW or an explicit blocker.

## User scope amendment: integrate navigation now

After dispatch of the narrower erase package, the user correctly asked to handle Up->new erase and Up->two-finger pan/zoom coherently. This package replaces the narrower package before candidate browser execution; preserve already completed valid design/RED/tests.

Static finding: applyViewportNavigation/setViewportZoomAt update camera and mark canonical dirty on deferred hosts; they do not normally reject merely because old canonical is pending. But serviceNavigationPresentation requires last submitted/presented exact Scene generation+token equal current Semantic/Scene and cancels navigation otherwise. serviceFrameOpportunity then chooses presentCanonicalFrame. Therefore two different symptoms share a post-commit display dependency: erase Down can reject, navigation camera can be accepted but lack current feedback.

Implement one existing-owner contract, with distinct input/Scene/feedback/completion states. Current navigation query must carry fresh current Scene generation/revision and mask basis even before exact submission. Do not restamp the old exact state or weaken cached-pixel proof. Existing renderNavigation query overload already supports current Scene commands, resource-bounded tile work and optional valid retained pixels; use it through Runtime. Navigation display is disposable, not an exact completion or a sealed-preview retirement proof.

Extend native RED/regression and the same two functional browser workers to immediate pan and pinch branches from the saved fixture. No new A/B measurements, no snapshot UI. Keep inputs/planned4ms vs actual arrival/submit timings separate. This is a concrete shared lifecycle fix; coverage/GPU strategy and500ms leaf costs remain separate follow-up.

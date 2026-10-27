# Independent whole-branch review — executor-transcribed summary

This is a faithful summary, not a verbatim transcript. The original reviewer response remains in the conversation.

Reviewer: fresh read-only gpt-6-astra subagent. Base 2fee75f66a7f96f6aac834228ede372a9abda59a through head c1300ce658016ad2026257c67a0cc0d024b47e05. No files, index, branches or build outputs changed by reviewer. Reviewer did not rerun tests while final verification was running.

Assessment: request changes. Three Important findings; no Critical or Minor findings.

## Strengths

Formal selection has one owner with revision fences and explicit Add semantics. Complete samples and generation-bearing keys reach Runtime; Down latches Shift and predicted samples cannot decide membership. Shape narrow-phase handles concavity, rotations, ellipse corners and even-odd self-intersections. Partial providers preserve selection. Multi-selection guards primary-only editing. Active hosts are wired; Web cancel retires ingress generations.

## Important findings

1. Obsolete retained roots: selection_session.cpp:64, canvas_region_selection.cpp:139. Select standalone A; begin Shift region for B; external reparent A beneath Group G before Up. onChangeSet revisions deletions only, so Add copies A while new query resolves B. Selection becomes [A,B] instead of [G,B]; subsequent delete can remove a Group child independently, and later Add may include ancestor and descendant. Resolve/normalize retained plus acquired roots on the same current view, or cancel on relevant topology change. Test actual ingress plus external reparent; ordinary geometry-change requery should remain supported.
2. Visible stale region surface: canvas_region_selection.cpp:132, canvas_editing_runtime.cpp:1979 (also invalid update line147). Successful Move presents a complete replacement scene and sets transient dirty=false. Context/render invalidation cancels the leaf and dirties canonical, but does not hide/clear the transient surface or set a handoff flag. Canonical repaint and late Up leave old lasso/camera/deleted content visible above canonical. Retire every region cancellation path safely; test provider visibility or screenshot, not state alone.
3. Locked non-primary deletion: canvas_editing_runtime.cpp:1943, canvas_region_selection.cpp:173. Select unlocked A then Shift-click locked B using the intentional includeLocked point policy. Primary A stays unlocked; UI and deleteSelectedObjects only inspect it; DeleteObjectsOp includes A and B and semantic deletion has no lock check. Validate all roots and expose matching UI availability; test both orders and unchanged canonical/History on rejection.

## Declined to judge

- Full region providers for Section, standalone RichText, Connector, vector/erased strokes and Group containers: explicitly deferred; Partial is implemented.
- Complete Image-resource/Sticky-child E2E acceptance: geometry fixtures alone do not establish those profiles.
- Physical mouse/pen/touch, IME, other platforms, G5/fault qualification and full FC08 302 runner: outside bounded review without evidence here.
- Separate second receivePlatformPresented handoff-clear branch: no active-region guard, but reviewer did not establish the necessary retire condition/live-frame sequence. Concrete cancellation leak above is traced independently.
- Product Gate PASS and independent test totals: source review does not establish them; existing tests did not cover the three sequences above.

No re-review was requested. Executor dispositions and new RED/GREEN evidence are recorded separately.

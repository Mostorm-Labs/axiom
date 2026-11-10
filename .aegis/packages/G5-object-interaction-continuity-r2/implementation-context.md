# G5 Object Interaction Continuity r2

This is a successor to r1, which ended `BLOCKED_EVIDENCE` and remains immutable. The controlled worktree currently contains the uncommitted r1 Render/cache checkpoint; preserve that delta and reconcile it before changing code.

## Confirmed evidence

1. A native probe showed Shape, Sticky and Table layout snapshots with identical stable `digest` values but different `shared_ptr` addresses are treated by `RenderGroupManager` as `change.content=true`, incrementing content revision and producing a contribution. This causes canonical/transient preparation to reset even though the painted layout is unchanged.
2. The Web reconstruction OI-CD5 (0b3b436e normal SDK, DPR2, 1431x1010 CSS, builtin fonts, 19 long-line objects after two erases) showed selected state already updated while `presentedGeneration` lagged during continuous hover/drag. Logs contained 272 `canonical working set pending` messages. Drag RAFs continued but content publication was sparse.
3. r1 static reuse had an independent oracle RED because the first-build path omitted prefix composition and the initial oracle accidentally used the new cache. r1 later produced a corrected bounded fixture, but did not materialize the real dense long-line + historical mask + four-object fixture.

## r2 target

Fix the derived invalidation/publication chain while preserving canonical semantics. First make layout identity stable for equal painted snapshots while still invalidating real text/font/content/layout changes. Then ensure transient object transforms and text presentation do not repeatedly reset a compatible prepared working set before it can publish. Keep the corrected static prefix/active/suffix reuse behind strict eligibility and preserve independent Direct oracle behavior when RenderCacheOptions.enabled=false.

# G5 post-range convergence: execution design

Package: `f29e30b771d197614968ea6c4c203d435b48a540e07b611fba9b5c3dd436f5ce`.
Anchor/current source before edits: `7d3ea0aaf75c8cf6d26e9eaa691b5d99ee82038d`.
The accepted range traversal implementation is retained without replaying its RED.
Previous r3 source, results and user demos are preserved in their original worktrees.

## Scene owner and intended change

`EditingSceneCompiler` owns derived Scene content stamps. `OperationEngine` remains
the canonical mutation owner; `PendingSceneStampPreparation` remains an intent-owned
resumable cursor. The compiler already retains the published body stamp, immutable
mask storage and cumulative FNV hash after each mask in `stampMetadata_`.

The pending prepare path currently ignores that metadata and reserializes every
historical mask. Actual-owner RED `stamp-red3` restores a real compact-stroke
snapshot, publishes two 4096-segment masks through OperationEngine, then appends
64 swept segments, prepares with
one existing 32-primitive step per call and applies through OperationEngine.
It takes 258 steps instead of 2. Complete published Scene records still equal
the independent `compileFull` path, including after front insertion and
remove/reapply with changed values. The initial `stamp-red` fixture omitted
surface binding and was rejected at admission; it is not the mechanism RED.
`stamp-red2` used restored historical masks; the first narrow implementation did
not accelerate it because restore's independent full compiler has no cumulative
mask memo. That cold path is deliberately retained. `stamp-red3` tests repeated
preparation after a real incremental publication, which has the existing memo.

On pending job initialization only, reuse the already published cumulative hash
prefix when body bytes match, mask storage owner AND pointee match the retained
before view, and the metadata has one hash per retained mask. Sorted mask pointers
must match that immutable prefix. Any newly inserted mask ends reuse; process the
entire subsequent suffix normally. Preserve existing operation/cursor identity,
on-apply full mask comparison, cancellation and per-step/time allowance. No added
persistent cache, new public API, Scene/History owner or semantic behavior change.

## Verification and measurement

Run GREEN and the existing abort, filled-path, history, publication and progress
guards. Compare complete incremental records with the independent full compiler;
do not replace the oracle with the optimized path. Existing Web primary input
provides confirmed A/B/C operations, Undo/Redo snapshots/pixels and qualified
canonical callbacks. Build matching normal baseline/candidate SDKs using locked
prebuilt Semantic/Skia and run bounded serial measurements without overlapping
builds/native CPU work. Keep source/build/driver/fixture identities with outputs.

Analyze retained frame/owner/worker intervals first. Owner CPU, outside-owner
opportunity gaps, worker envelopes, submission and physical feedback are distinct;
unattributed time is not assumed to be hardware GPU wait. Isolate DPR4 crayon
input on current normal builds and document any actionable regression or finding.
3s/5s classify progress, not hard rejection. Do not import old r3 sampling windows
or acceptance thresholds. Do not issue a Gate or overall performance PASS.

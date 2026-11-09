# UR2a P36: own-source mask isolation

Control independently re-read all80 saved cold buffers and the unchanged CPU AA
maps.14 stages fail: S0/S2 E after/Redo each4432 pixels; S2 Z after/Undo/Redo
each1069, in both SDKs. Every failing interior witness is transparent zero where
the original reference requires opaque white; preview is empty and hidden.
Diagnostic white compositing removes the interior difference, but is expressly
not a permitted fix or change to acceptance.

Input liveness and currentScene work are useful existing progress.10 new UR2a
native tests independently pass. Candidate20-file source delta, matching artifacts,
918 readonly payloadfiles and898 archive members resolve. This is an implementation
failure of already-frozen pixel requirements, not a new Verification finish line.

The Render path to investigate is precise: shouldIsolateInkEntry excludes
nonidentity transforms; canRetainCompact excludes permanent masks; admission can
choose kComposite, followed by mask-clear on that ordered composite.16 recorded
v4 copies have transforms. Confirm this in a native causal RED before changing
product code. Do not treat comments or old CPU branches as semantic authority;
preserve unaffected legacy semantics and report a real conflict rather than
silently expanding scope.

Published HEAD is bed9e779; current20-file UR2a delta is uncommitted and must be
preserved. Control writes package/review artifacts only. Implementation owns the
product and index. No commit/push. Continue until READY or a precise BLOCKED,
then proactively notify the control thread with exact evidence identities.


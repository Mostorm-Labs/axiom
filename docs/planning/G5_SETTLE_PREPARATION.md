# Cooperative exact navigation settle

This user-authorized follow-on slice starts from GitHub Mostorm-Labs/axiom
`6aa330a8d27b8925f8c8be5f93625b07542c3292` (ancestor relation). It extends the
camera/navigation work under G5 v0.3, not a new semantic or SDK contract.
Runtime remains camera/demand owner; Render owns disposable prepared pixels;
Skia/provider remains the sole draw/present path. No SDK declarations/exports,
semantic schema, Brush geometry or dependency locks change.

Execution closure: preserve Direct exact pixel oracle; prepare only cacheable
independent compact groups offscreen; cooperatively yield by group/time policy;
retain existing navigation pixels and canonical dirty demand while pending;
never submit/present/qualify a partial frame. New camera/content/clip/resource/
surface identities invalidate prepared sources through existing cache validity.
Uncacheable sources, insufficient residency, evictions or allocation failure
fall back to existing synchronous rendering rather than retry forever.
Tests must cover bounded slices, no visible target/present/submission changes,
Direct parity, supersession, capacity fallback, invalid geometry and Runtime
receipt isolation. Native G5 suite, actual WebGL oracle, SDK browser measurements,
boundary/workspace checks and source/artifact hashes are required. Physical
platform qualification and formal G5/P34 PASS are outside this slice. Continue
through these checks or return an explicit oracle/environment/regression blocker.

Implementation policy: at most two missing groups and a 4ms cooperative time
check between groups per render invocation. A group is atomic, so this is not a
hard frame deadline. The Web bridge must not spend another preparation slice
in the same RAF on an ordinary retry; only a completed outstanding GPU fence
allows same-opportunity preparation. Approximate navigation remains visible
until an exact complete frame is submitted. The initial implementation repeats
visibility/reference planning while preparing; reducing that copying and splitting
single expensive coverage groups remain subsequent work.

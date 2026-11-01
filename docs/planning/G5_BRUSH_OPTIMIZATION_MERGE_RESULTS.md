# G5 / brush integration result

Integration parents: caaae6e1563e7707646b616a4aefd2e945f49aba and
f2e391cd13180cd7627561c911120dcec058e5ee. The older de274161 merge remains
historical; this integration includes the later round/marker V4 and pressure
crayon V3 plus its target-compatible GPU preview optimization.

Resolved three shared renderer/navigation conflicts by preserving G5's V3 hybrid
coverage, cached transient editing, complete feedback, admission/backpressure and
prewarming, while adding V4's retained contour compiler and V5's captured seed
and compatible surface policy. Direct and cooperative raster jobs both pass seed;
cooperative V4 jobs retain compiler state across slices instead of rescanning the
prefix. No SDK API, lock, semantic codec or package identity was redefined here.

Validation: 34/34 native G5/brush tests; matching Web SDK acceptance, pending-fence
handoff, resize/loss, Shell/Axis and demo; navigation cadence; 103 DPR2 brush
checks including crayon material/preview/persistence; actual WebGL cached/resumed
Direct pixel parity for V4 round/marker and V5 crayon, plus original hybrid corpus.
The expanded GPU corpus exceeded the old 120s harness deadline on the shared
machine. The identical compiled probe completed with code0 under a diagnostic
240s harness deadline, with unchanged pixel assertions and no non-timing errors.
This is a functional verification deadline, not a latency budget.

64-stroke rectangle drag remains responsive (synthetic Chrome DPR2 observation:
max pointer2.4ms, live RAF3.8ms, first GL submission19.8ms) and its final pixel
SHA256 remains 7f343f1364e87041ad12946cd08c40125bb9591873fbc0aaac9962df20b93213.
No canonical submission occurs during live transform; terminal increments once.

Evidence and exact source/artifact identities are under
verification/evidence/performance/g5-brush-merge. Separate demo4194 contains the
matched merged artifacts;4193 is preserved. Only source commit f2e391cd is merged;
new uncommitted chalk/watercolor work subsequently started in the sibling worktree
is excluded. No full-root rerun, physical-platform qualification or Gate/P34 PASS.

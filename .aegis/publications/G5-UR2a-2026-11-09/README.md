# UR2a source and evidence checkpoint

This checkpoint publishes the independently accepted23-file UR2a/P36 source
delta, its frozen execution packages, lightweight failed/repaired result and
control-review indexes, and these six publication metadata files.

Scope acceptance covers UR2a U2R-01..12 and the permanent-mask own-source repair.
It is not whole G5/P34, performance, other-platform or release qualification.
Prior failed results and raw evidence remain unchanged. No new verification was
run for publication; already accepted source hashes are checked exactly.

Large raw pixels/snapshots, native and Web SDK binaries, source inventories and
tar payloads remain LOCAL_ONLY in the controlled shared worktree (approximately
3.35GiB per control's publication boundary). Git contains hashes and indexes,
not those bytes; a remote checkout cannot recover the local payload by itself.
See evidence-index.json and local-only-boundary.json for exact bound references.

This directory cannot embed its own resulting commit hash. The final staged-blob,
commit/tree/parent, ordinary push, authenticated server and clean-index proof is
retained under .aegis/results/G5-UR2a-publication-2026-11-09/ as LOCAL_ONLY final
publication evidence and reported to the control thread. No UR2b is activated by
this checkpoint. Existing4291/4290 and user documents remain untouched.

Publication amendment r1 preserves the original BLOCKED subject and all frozen
file bytes. The exact five SHA-bound noncode artifacts retain their complete
checker diagnostics; every other94of99paths must pass the strict checker. This
does not admit any source/test/driver whitespace exception. The final99-path
stage/server result is LOCAL_ONLY under
.aegis/results/G5-UR2a-publication-whitespace-r1/result.json.

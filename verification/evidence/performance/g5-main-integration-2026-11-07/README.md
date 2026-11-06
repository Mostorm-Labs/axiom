# G5 / main integration verification, 2026-11-07

The merge joins controlled branch `17688e57` with fetched main `e32dee03`
(SDK 0.4.3). It was automatic, without conflict resolution or additional
production changes. Relative to main, only the prior mixed reentry test,
registration, runner documentation and test evidence are added. The assessment
and this record are a documentation-only descendant of the merge commit.

`result.json` binds the merge revision and tested tree, full case-name comparison,
installed package hashes and browser checks. `test-inventory.json` lists all 1076
configured cases. `focused.log.gz` covers four Render cases, including main's
capacity/masked-stroke corpus and this branch's pause/reentry cases. The disjoint
remaining cases are in `native-regression.log.gz`. Names are checked against the
inventory; numeric CTest IDs are local to filtered runs. Shared CTest Temporary
files are not used to combine results.

Results: 1006 passed; 70 failed/not-run. The previous 66 names remain; four added
failures are old creation fixtures expecting the creation preset to remain active
after success. Main's `uiux-default-interaction-v1.json` and `selectCreatedObject`
now finish one-shot creation by selecting the object. The fixture comment and
second-gesture setup contradict that behavior. Their future repair must preserve
same-pointer reuse/other-source cancellation coverage, explicitly reselect the
creation tool and check the new post-create policy. No fixtures were weakened,
skipped or modified by this merge. All failed names and the four existing Semantic
test compilation failures are preserved in the result and compressed logs.

Also passed: 39 G5 Node tests, nine controller transport tests, 119 Runtime boundary
contracts, Web/WASM build and a matching 36-file root-only SDK install with no
private header/archive payload. Actual Chrome public-root-SDK checks cover the
user-like mixed scene's 1000ms pause/resumed zoom and DPR2 16-source erase capacity,
preview/commit pixels, Undo/Redo, cancel, repeated sweeps and subsequent two-touch
pan/zoom. Each browser worker was isolated, bounded to 180s/3GiB, closed all owned
processes, and ran after native tests/builds finished.

The SDK was compiled while the merge was pending. Its embedded revision/dirty
values alone do not identify that staged tree; the real source observer reports
dirty and the exact tested merge tree/package hashes are retained. These are
local integration observations, not a new clean-source paired performance
qualification. Historical 0.3.9 measurements, previous BLOCKED G5 evidence and
Gate records are not rewritten. No physical/all-platform/G5/P34 PASS is claimed.
The user's 4242 Demo remains the previous confirmed build.

Next work is proposed, not newly authorized implementation, in
`docs/planning/G5_POST_MAIN_REASSESSMENT_2026_11_07.md`.

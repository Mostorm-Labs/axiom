# G5-DPR4-render-sync-r1

`READY_FOR_CONTROL_REVIEW` — scoped Render work bounds and correctness verified; overall recovery target remains unmet, with a measured total-latency regression. No Gate/P34/performance PASS is issued. No commit/push performed.

Current working tree is `/Users/qing/.codex/worktrees/g5-post-range-convergence`, branch `codex/g5-post-range-convergence`, HEAD `7d3ea0aaf75c8cf6d26e9eaa691b5d99ee82038d` plus the accepted Scene stamp overlay and this private Render delta. Index is empty. The earlier overlay, locks, public API and old Demos 4351/4352/4353/4354 are preserved. No third-party rebuild or resource-profile increase.

## Outcome and remaining performance finding

Ordinary matching SDK runs use the same fixed wax16-default DPR4 inputs and 512/1024/128 MiB profile, with timing/qualification disabled. Values below are pan / pinch in milliseconds.

| Observation | Baseline B | Final candidate C |
| --- | ---: | ---: |
| Maximum callback | 7854 / 8827 | 22 / 93 |
| p95 callback | 617 / 752 | 2.6 / 3.4 |
| First erase color-write activity proxy | 7818 / 8778 | 31 / 8 |
| First motion color-write activity proxy | 8512 / 9604 | 12999 / 16434 |
| Up to qualified committed camera frame | 8837 / 10114 | 13030 / 16463 |
| Terminal camera convergence | 8679 / 9956 | 12868 / 16301 |

Smaller callbacks and earlier erase activity do **not** close recovery: the 3s/5s bands remain unmet, and navigation still waits longer. First correct physical pixel and hardware GPU duration were not measured. Flush API CPU scopes and color writes are reported only as such. See [ordinary comparison](ordinary-review-comparison.json).

The final matched pair has identical 18 Operation buffers, Snapshot and 37,440,000-byte RGBA. Receipt generation18 matches the qualified camera callback. Snapshot SHA256 `fb3ca5d2d83dc12cbf6e12e7e500e1a29d79c1f69413145e3dc41c89e50e7c76`; RGBA SHA256 `8e5efae17233816ddb3eebb35fc2290e868d536f487d494578ac6f7dbc10d996`. See [artifacts](review-pixels-operations.json).

## Implementation and proof map

- DS0: [actual Web attribution](DS0-Web-attribution.json), [remaining mask replay attribution](DS0-candidate-remainder.json), [r3 selective disposition](r3-disposition.json). The initial Down-to-first-Move stall was transient feedback flush after 26,805/71,346 pressure commands; canonical Up flush is a separate path.
- DS1: [design](design.md), [owner RED](owner-red.json) / [GREEN](owner-green.json), [Down RED](down-red.json) / [GREEN](down-green.json), [mixed prepared RED](mixed-prepared-red.json) / [GREEN](prepared-green3.json), [mask RED](mask-red.json) / [GREEN](mask-budget-final.json). Existing owners advance 32 knots, aggregate 128 pressure commands, and 32 mask draws per step (128 per opportunity); pending progress persists before provider publication.
- DS2: [independent native pixels](final-independent-pixels.json), [lifecycle](lifecycle-green3.json), [quota/leases](quota-green3.json), [three optional-cache limits](oversize-final.json), [navigation-release pause](suspended-green2.json), [new-prefix interleaving](interleaved-final.json). Capture tiles own their detached pixel lease immediately. Optional pressure residency cannot become a document limit; existing staging remains cooperative. Native pixels: 15/16 rows byte-exact; one v5 ink-only DPR4 channel byte differs by1 within the established <=2 AA tolerance. Final Web RGBA is byte-exact.
- DS3: [final SDK](review-SDK.json), [frozen source](final-sample-source-freeze.json), [final comparison](ordinary-review-comparison.json), [browser ledger](browser-ledger.json). Eight owned starts total 1372 actual ceil seconds, all cleaned; no new browser runs during handoff.
- DS4: [verification summary](verification-summary.json), [exact-old-source baseline failures](baseline-regression-relink.json), [fresh final 11/11 CTest](handoff-native-checks.json), [bindings/demo validation](handoff-validation.json).

Production changes are confined to Render. Canonical/transient preflight and streamed ink-only material use shared prepared pressure geometry and the original raster job. Material body and historical mask replay retain independent progress. SDK/Input/Interaction/Scene/History/Operation/ARC and Brush/AA rules are unchanged by this package. The earlier accepted Scene stamp files remain byte-identical.

## Verification findings and retained invalid attempts

Final focused CTest is 11/11. Expanded related native tests are44/46: `G5CanonicalPreparedCompatibility` and `G5TransientDisplayContinuity` fail identically against the exact-old-header/source baseline Render archive. Python1/1; Node25/26 with the immutable historical SDK fixture missing (ENOENT);122 boundary contracts; P36 material and21 Runtime lifecycle rows. Full suite is corroborative; prior unrelated semantic switch `-Werror` blockers remain. See command logs and [verification summary](verification-summary.json).

The first ordinary wall-clock-timestamp pair produced a different second erase under the blocked baseline and is retained as **not input matched**, not correctness PASS. The local driver then fixed explicit sample timestamps while preserving coordinates, event scheduling rhythm and immediate post-Up navigation. The final fixed-input comparison is the review pair. Initial wrong Python/tool switches, invalid old-archive links using new private layouts, and corrected test setup errors are retained and classified; invalid segfault/link attempts are not baseline regression evidence.

## Exact review material

Start with [result.json](result.json), [source manifest](source-manifest.json), [Render-only patch](source-delta.patch), [artifact manifest](artifact-manifest.json) and [command ledger](command-ledger.json). The artifact manifest binds the shared local evidence; no Git publication is implied. Large browser buffers/diagnostic logs have deterministic individual gzip reviewer copies in `raw/`, with original and compressed identities in [raw manifest](raw-artifact-manifest.json). All originals, including cold buffers and invalid initial observations, remain at their recorded paths.

Normal user demo: **http://127.0.0.1:4355/example.html**, serving `out/G5-DPR4-render-sync-r1/install-review/js` with diagnostics and qualification off. Wrapper and HTML HTTP bytes match that SDK. This candidate is for comparing bounded callbacks and the still-unresolved recovery wait.

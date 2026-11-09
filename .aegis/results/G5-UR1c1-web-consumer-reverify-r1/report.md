# UR1c1 Web consumer functional reverification r1

Status: **READY_FOR_CONTROL_REVIEW** for this verifier-only amendment. Not an overall UR1/G5/P34 or performance verdict.

The original inline scheduling reproduced the600ms/1200ms deadline-overrun RED before the helper changed. A separate functional pacing seam now preserves original Down/32Move/Up geometry, order and logical timestamps while giving bounded display opportunities after Down and each Move. The legacy drag/performance pacing defaults remain intact. Abort/timeout cancels the contact and remains a failure.

Both qualification S0/S2 cases actually executed33SDK frames while erase was active, not merely33sampler callbacks. In each, navigation was followed by16retained-ink get calls on the same renderer service,16identity hits,0new body builds,0fingerprint bytes and0complete-content comparisons on that reused call. These facts come from actual Render leaves. Normal cases also executed33SDK frames and content writes, with no prepared/execution diagnostic records.

All4new public SDK cases completed:12freshRGBA/snapshot stage pairs independently qualify and4Undo/Redo roundtrips qualify. All3stages per case were transferred and hash-checked before any oracle/predicate. The original independent CPU geometry AA rule is unchanged.8new-input oracle invocations completed under the original10000ms bound;4full snapshot/request/oracle/resource exact matches reused saved independent references and still compared newly captured candidate pixels. The exact map is in verification-facts.json and reference-catalog.json. Raw strict hash predicates are retained even when their byte hashes differ within independently qualified AA edges.

Node29/29, Python14/14,120runtime boundaries and diff checks pass. The14protected product/native-test files, every historical Runtime/SDK/schema/lock file, originalSDK installs, oracle and resources remain byte-exact. Native43/43and prior canonical/drag coverage are inherited from the bound independent review rather than rerun. There was no product edit, SDK/native rebuild, newDemo port, commit/push or UR2 activation.

Only UR1C1-R1-Q1 and UR1C1-R1-N1 ran, serially, once each. Q1 charged56seconds; N1 charged55seconds. Both completed with owned cleanup and RSS below3GiB. The separate ledger preserves historical48starts/1770seconds, adding2starts/111seconds for cumulative50/1881; remaining NONE. Original windows and evidence are unchanged.

This repairs functional reachability, not the long first frame. The old600.6/553.1ms observations are preserved; these new functional runs observed first SDK frames around807.4/849.4ms in qualification and843/927.9ms in normal. These are disclosed raw observations, not an A/B benchmark or evidence that cold-frame latency was fixed.

New source identity is recorded in source-identity.json, including oldUR1c1materials and the new frozen package; only this continuation's own result/performance outputs were added to existing self-exclusions. It is not relabeled as historical0a5d8113. The8-file verifier delta/preimages and byte-exact patch roundtrip are materialized separately from unchanged product bytes and matching SDKs.

Existing normalDemo4285continues serving the same artifact; this verifier change needs no newinteractivebuild. UR1c2, UR1d and UR2 remain outside authorization and await control review.

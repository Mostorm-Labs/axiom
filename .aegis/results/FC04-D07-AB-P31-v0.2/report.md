# FC04-D07-AB P31 Scene correction v0.2

Result: **four package preflights PASS; product implementation NOT_RUN**. This materialization corrects a P31 scope defect; it does not close D07-A, D07-B or D06-B, and is not a Gate or physical test pass.

- Repository: `Mostorm-Labs/axiom`; branch: `codex/fc04-d07-selection-audit`.
- Verified current P32 result/task anchor: `15e76458f3872be4d1b51d4aac05eb8402719014`.
- Feature anchor: `c7f874395d55952e9d5288e2a336a21747f37f11`; previous task anchor `832866a2d001a247c4eb374c1fb1899258a6f6cf`. Both are ancestors of the task anchor.
- Input archive SHA-256: `2b1912f37efdff35f8dd073f49ea7103fef5675829a47f5d395e72dd2b9d5ff6`.
- New package: `.aegis/packages/FC04-D07-AB-P31-v0.2/package.json`; SHA-256 `b653d8847493f724c60c6921d2e807321174c89b8e310f272ccda5d789143da8`.
- Source lock SHA-256: `9b3eb816b64a63bb80528dc4cdda34273e538a263fe89676cf3703fdc562c322`; package integrity SHA-256: `dee7b32bc034f300b17964c8185cdf26fe62b1c3431ccdfc39e3e2e80ea873d2`. `input-lock.json` binds historical report, commands, authority inputs and exact regression lists; package `authority/` captures the current reviewed pages.
- Parent package v0.1 and all earlier results remain unchanged. Product and test source are unchanged by this P31.

## Findings and limits

The old coordinator applies to the live Scene while synchronizing; it does not furnish a detached prepare-only seam. The new primary Scene scope covers its actual owners. Two conditional participants are included on concrete source evidence: `SceneRecordStore::commit` inserts into live containers and copies records despite `noexcept`; `DirectRenderScene::commit` likewise inserts/pushes and copies. `Scene::publishStagedSnapshot` also copies invalidation state in a `noexcept` path. P32 must stage actual allocations and prove failure containment, not treat the `noexcept` signature as proof.

07-14 still requires ordered per-Operation identities, generations, stamps, ChangeSets and eligible DataBridge notification. The design binds separate ordered notifications and coherent local Scene publication. **Actual callback timing and consumer compatibility are not established by this P31**; the real P32 case must prove them, and an actual conflict must stop as `AUTHORITY_CONFLICT`. Further participants remain read-only until specifically justified under a corrected scope; an unlisted owner requires return to P31.

Historical real native results at the task anchor: D07 16/17 passed, with `fc04.d07.D07.ExplicitIntentGroupProducesOneUndoUnit` failing; Selection 160/160 passed; single-command receipt-growth fault went from exit 86 to exit 0. The earlier Web runs were actual Chrome/WASM with synthetic CDP input. These are historical observations, not new executions. SCENE-PREP-01–06 and the new grouped product obligations remain `PLANNED_NOT_RUN`.

## Four preflights

Exact command arguments, Python executable, working directory, exit codes and SHA-256 of each raw output are in `commands.json`; the final four `raw/final/*.log` files are preserved alongside it. Every command uses `--repo .` in the real worktree:

```
python .aegis/packages/FC04-D07-AB-P31-v0.2/preflight.py --repo . --check PackageBindingPreflight
python .aegis/packages/FC04-D07-AB-P31-v0.2/preflight.py --repo . --check EvidenceContractPreflight
python .aegis/packages/FC04-D07-AB-P31-v0.2/preflight.py --repo . --check DesignPreflight
python .aegis/packages/FC04-D07-AB-P31-v0.2/preflight.py --repo . --check D07PackagePreflight
```

Each exit was `0`. These are package/structural checks; they did not build or run the new product tests. Preflight verification checked the prior result manifest and exact prior Selection and D07 IDs. No claim is made that the unimplemented producer or Scene seam works.

The exact materialized revision is the Git commit containing this report. The next authorized P32 must start from it, retain the valid work recorded at `15e76458`, and follow the frozen package scope and terminal blockers.

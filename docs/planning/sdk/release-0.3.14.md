# SDK 0.3.14 release

The user authorized integrating codex/geometry-navigation-integration through
`d98ac4caae0bf8c06d2898feb9d622006bd7cacf` and publishing a new SDK. Merge
`7dccf70de155eeee9568d81e7fa3a1358a8dde72` retains main's published 0.3.13 history
and MSVC fixes. The feature branch and its active worktree remain available;
uncommitted work in that worktree is outside this release boundary.

This is a patch-level rendering correctness/performance improvement. Shared
Render's existing retained-source, navigation-tile and resource-budget owners
coordinate derived pixels and GPU lifetime. Runtime/OperationEngine retain
canonical document, camera, history and receipt ownership. Public JS/C APIs,
ABI, transport and Operation/Snapshot formats are unchanged (API/ABI/transport 1).

- Retain at most one additional admitted wide canonical image so a narrow
  zoom-in completion does not discard all coverage needed after a pause.
  Partition qualified images and tiles by density, keep sharper overlap, reject
  stale scene/surface identities and unknown regions, and retire the optional
  older image first under mandatory pressure. No additional pixel copy or pool.
- Share completed canonical pixels with navigation, keep bounded canonical replay
  under the existing hard limit, account for actual tile staging requirements,
  and discard obsolete exact preparation when navigation demand changes.
- Distinguish already-retiring GPU quota from live allocations when planning
  reclamation; wait for real completion before actual allocation rather than
  needlessly evicting useful feedback. Admission remains in ResourceBudgetManager.
- Preserve the stable material path after unsuccessful experiments. Timing
  observers and source-bound evidence keep their diagnostic limitations explicit.

Default release capacities remain 32/96MiB. No new consumer budget knob,
Semantic/Skia dependency rebuild, lock change or private install API is included.
All platform package versions and RN dependencies are 0.3.14.

Fresh local release checks pass: 35 selected native cases (including retained
zoom reentry, cache/Direct parity, shared resource lifetime, selection, image
export and Table); 39 G5 Node tests; 37 packaging and 15 server/workflow tests;
16 G5 Python cases with five unavailable private-fixture cases skipped; strict
consumer TypeScript; 118 Runtime boundary contracts; offline Semantic Store
resolution; strict shadow compilation of all four changed production C++ TUs.
The matching rebuilt JS/WASM/resources pass real Chrome external-consumer,
selected-navigation and feedback checks. Dense DPR2/DPR4 workers finish pan,
idle/freeze/recovery with stable source/artifacts/state and owned-process cleanup
inside 180s/3GiB bounds. Three additional bounded normal diagnostics-OFF root-SDK
pause/resume workers pass: stress zoom, user-like zoom and user-like pan (1000ms
pause, 144 inputs; user-like image 2,334,857 bytes). Document, operation and
selection remain unchanged; no page errors. Content-call timing is only a local
software observation; accepted-submit diagnostics are unavailable in OFF builds.
The checked-in repair evidence verifier independently verifies the historical
14-run matrix and retained failure/control records without granting a Gate pass.

Before reporting publication complete, hosted gates must bind this exact release
source, verify Web/Windows SDK and Demo archives and all six RN native targets,
and confirm all six npm packages. Published versions/tags/assets are immutable.

Historical evidence is preserved, including the retirement-arbitration BLOCKED
result and the pause/resume evidence's 66 existing full-suite failures/not-run.
The release checks do not establish a full native-suite, physical-input latency,
100K/1M, all-device performance, formal G5/P34 or Gate pass. Local Web checks do
not establish Windows ABI correctness; hosted Windows external-consumer checks
and all native RN target packaging remain separate release obligations.

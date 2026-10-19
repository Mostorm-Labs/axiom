# FC-01 收尾核对（首轮）

当前优先级：先完整收尾 FC-01，再评估 FC-02/03 及后续计划。
用户提出 FC-01A 在 FC-02/03 之后并入 FC-04 实施；本轮仅记录方向，不提前开展套索设计/实现，
不改路线 Authority 或既有冻结合同。

核对 HEAD：`aef78929b79d4c479b5ba9ac37114dc6c601aebe`。候选产品源码：`e42c907f827e33864d80a47ba7f9d5938ba14e3a`；两者之间仅有证据提交。
FC-01 原任务、控制、P31 包和旧结果的 ancestry 均已验证；五个包内文件哈希均匹配。

Windows/Web 当前构建、自动回归、用户实测反馈已经存档。它们是 FC-01 收尾可复用的材料，
不能自动代替原合同 E01–E10 的完整独立场景。旧 E06/E07 的 NOT_IMPLEMENTED 等文字也不能
直接判定当前代码缺失：这些记录绑定旧源码，之后 Runtime/Host 已有显著改动。

| 项目 | 当前收尾重点 |
|---|---|
| E01 | Complete frozen SH2-G01..25 mapping and independent wire/typed admission comparison; preserve V1 negatives. |
| E02 | Qualify typed/wire/restore/replay/remote, idempotency and Snapshot2 reader/writer capability cases. |
| E03 | Map the frozen Create terminal/source-loss schedules to current common Runtime; preserve historical RED chronology honestly. |
| E04 | Requalify independent far-axis Snap, DPR/camera math and preview-to-Up consistency on current source. |
| E05 | Requalify analytic Triangle/Line/Arrow geometry, Full/Incremental Scene and pinned headless raster cases; locked Skia is now available. |
| E06 | Qualify selected commands and complete conditional History/preimage/delete-closure conflict matrix; do not infer missing implementation from obsolete reports. |
| E07 | Map current RuntimeFacade owner queue, target fences, coherent full-ID readback and UI routing to the frozen matrix. |
| E08 | Bind current real Minimal Windows/Web builds and adapter observations; keep Android user deferral explicit and reconcile frozen applicability before full closure. |
| E09 | Qualify Canonical Applied, backend-qualified Presented and Axis durability/NoEcho as distinct facts on current owners. |
| E10 | Unify exact package/source/provider identities, inherited authorized deltas, protected scope and regression mapping; independent review decides final Gate. |

完整映射见 obligation-reconciliation.json。当前主源码下尚无 verification/corpus/fc01/v1；
run_dev.py 负责构建/测试，仍需确认并补齐冻结独立场景的执行与映射。已有机制充分覆盖的项目
可复用，避免把置信度补强变成新的阻塞条件。发现真实实现失败后，先复现 RED，再做窄修复。

平台范围：Windows/Web 使用当前真实 Minimal consumers；原 FC-01 平台义务与后续 consumer
迁移之间应保留明确映射。Android 保留用户延期状态，不能静默改为 PASS 或从冻结义务中删除。
历史首轮 RED 执行顺序也不补造，由独立审查明确处置。

本轮只完成现状核对，没有重新执行 FC-01 全套场景，也没有宣布 FC-01-DEV 或 PHYS PASS。
下一步按原 FC-01 接受标准收敛当前版本的证据与实现缺口，准备可独立审查的最终交付。

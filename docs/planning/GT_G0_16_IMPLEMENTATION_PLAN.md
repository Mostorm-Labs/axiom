# GT-G0-16 实施计划

## 目标

建立 G0 Gate Report 的严格 Schema、纯函数 aggregator、CLI 文件完整性边界和 commit-bound Evidence。
本任务完成后仍停留在 G0，GT-G0-17 review 不在本轮启动。

## 工作包

1. **Schema 与数据模型**：冻结 `axiom-gate-report-v1`、E1～E4、task lineage、platform records、
   artifact metadata 和 promotion 保护。
2. **Aggregator**：实现 `createG0GateReport`，要求 GT-G0-00～15 全部出现，确定性排序并保留第一处分歧。
3. **CLI 完整性校验**：读取 lineage/platform/artifact 目标，拒绝缺失、越界、符号链接逃逸、哈希或字节数漂移。
4. **测试与语料**：覆盖报告阻塞、缺任务、非法 commit、缺失文件、路径穿越、hash drift 和 bytes drift。
5. **Evidence**：从最新 main 生成 `g0-gate-report.json`；保留 E4 `BLOCKED` 的事实，不把 hosted 结果升级为 physical PASS。

## 交付物

- `verification/packages/platform-harness-runner/src/ci/GateReport.ts`
- `verification/packages/platform-conformance-cli/src/commands/gate_report.ts`
- `verification/schemas/platform/gate-report.schema.json` 及合法 fixture
- runner/CLI 测试
- `verification/evidence/g0/gt-g0-16/` 输入和报告
- 本设计、Evidence 和 Tracker/R milestone 状态更新

## 退出条件

- [x] Schema inventory、runner build、CLI build 通过；
- [x] runner 和 CLI 的报告/完整性测试通过；
- [x] E1、E2、E3 可由当前 GT-G0-00～15 Evidence 重建；
- [x] hosted-only Release 明确输出 E4 `BLOCKED`，promotion=false；
- [x] 实现提交 `d335c7a4aa5bb3e934434f8c551b88f3ab3c6a62` 后重新生成 commit-bound report，并记录最终文件 hash；
- [ ] GT-G0-17 独立 review 通过（不属于本任务）。

当前 Gate 状态：`Validating`。在未形成最终 implementation commit 前，不宣称 GT-G0-16 Pass。

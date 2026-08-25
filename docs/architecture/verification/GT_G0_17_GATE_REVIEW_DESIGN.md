# GT-G0-17：G0 Gate Review 设计

## 术语

- **Gate Review**：独立检查 Gate Report 是否满足该 Gate 的全部退出条件，并决定 Gate 是 `PASS`、`FAIL` 还是 `BLOCKED`。
- **Promotion authority**：允许从 G0 晋级 G1 的权威结论；不能由单个任务 Pass 或 hosted runner 自行产生。
- **Evidence gap**：退出条件要求的验证事实没有独立、可复现、commit-bound 证据。Evidence gap 不是实现者可以临时放宽的阈值。

## 目标与边界

GT-G0-17 只审核 G0，不实现产品代码、不重写历史 Evidence、不进入 G1。审核输入是 GT-G0-00～16
的任务谱系、GT-G0-16 `G0_GATE_REPORT`、G0 退出条件和当前仓库状态。

Review 必须分别判断：

1. 任务是否全部 `Pass`；
2. Gate Report 是否 schema-valid、commit-bound 且 artifact hash 可复算；
3. E1～E4 是否达到 G0 退出条件；
4. 是否存在 hosted/physical authority 混淆、缺失 Evidence 或未关闭的确定性门禁。

## 决策规则

- `PASS`：所有适用退出条件均有可复现 Evidence，且 Gate Report 的 promotion.allowed 为 `true`；
- `FAIL`：已有冻结契约或已执行测试明确失败；
- `BLOCKED`：Evidence 缺失、物理 authority 未满足或上游事实无法证明。`BLOCKED` 不得改写成条件 PASS。

本次审核明确区分任务状态和 Gate 状态：GT-G0-16 可以是任务 `Pass`，而其生成的 G0 Gate Report
仍可因 E4 `BLOCKED` 使 G0 晋级被阻断。

## G0 退出条件逐项审计

| 退出条件 | 当前结论 | 审核理由 |
| --- | --- | --- |
| C++ runner、seed corpus、invalid corpus 全部 PASS | BLOCKED | 现有 Evidence 证明 C++ native seams 和 TypeScript/reference runner，但没有独立的完整 C++ runner Gate 证据 |
| 同一语料连续 10 次结果与 digest 一致 | BLOCKED | G0-03/G0-04 有局部十次 replay；GT-G0-15 hosted full matrix 为两次 repeat，未覆盖全量 G0 corpus 十次 |
| first-divergence 可机器读取 | PASS | Runner/CLI 测试和报告包含确定性 first divergence 字段 |
| Gate Report schema-valid、artifact hash 可复算 | PASS | 23 schemas/fixtures 通过；GT-G0-16 manifest 文件 hash 已复算 |
| clean checkout 可重复构建运行，blocking CI 不自动 bless | BLOCKED | 有 workspace/CI 局部 clean-checkout 证据，但缺少完整 G0 Gate Report 重建的独立 clean-checkout Evidence |
| 总状态 PASS | BLOCKED | E4 `PHYSICAL_RELEASE_AUTHORITY_BLOCKED`，且上述 Evidence gaps 未关闭 |

## 非阻塞事实

- GT-G0-00～GT-G0-16 的任务 lineage 当前均为 `Pass`；
- Nightly aggregate/reproducibility 为 `PASS`，Release workflow 正确输出 `BLOCKED_AUTHORITY`；
- iPhone、iPadOS、Android、Windows 和 Web 的平台 Evidence 保持独立，不因本审核合并；
- Android visual-smoke 未跟踪目录不属于本审核输入，不能成为本任务的隐式 Evidence。

# GT-G0-16：G0 Gate Report 设计

## 术语

- **Gate Report**：把一个 Gate 的任务谱系、验证层级、平台事实和产物完整性聚合成机器可读报告。
- **G0_GATE_REPORT**：本报告的唯一 authority。它可以决定 G0 是否具备晋级资格，但不替代产品 Gate 或发布审批。
- **commit-bound**：报告的源码提交、语料/Schema 版本和 Evidence 文件哈希可以被重新计算并核对。
- **BLOCKED**：证据不足或外部 authority 尚未满足。它不是条件 PASS，也不能被报告生成器自动降级成 PASS。

## 目标与边界

GT-G0-16 消费 GT-G0-00～GT-G0-15 的已接受 Evidence，生成唯一的 `axiom-gate-report-v1`。
报告聚合 E1 Contract/Unit、E2 Reference/Mock、E3 Integration/Golden 和 E4 Physical/Demo 四层，
并保留每个任务的状态、证据路径和 SHA-256。它不实现产品 Semantic Kernel，不改变 POC/RF ABI，
也不执行 GT-G0-17 的人工 Gate review。

GT-G0-15 的 **Platform Release Decision** 只描述 Nightly/Release wiring 的 provider-neutral 结果，
authority 是 `G0_WIRING_ONLY`。GT-G0-16 将其作为输入：hosted Nightly PASS 可以使 E3 通过，
但 hosted Release 不能替代物理设备 authority；缺少物理 Evidence 时 E4 必须为 `BLOCKED`，
总报告也必须为 `BLOCKED`。

## 输入与输出

CLI 命令：

```text
axiom-platform-conformance gate-report \
  --source-commit SHA --branch NAME \
  --lineage PATH --hosted PATH --artifacts PATH \
  --repository-root PATH --output PATH
```

输入包括：

1. `lineage`：GT-G0-00～15 的任务状态、证据路径/哈希、语料身份和平台来源事实；
2. `hosted`：GT-G0-15 的 Nightly、Release 和 reproducibility 结论；
3. `artifacts`：所有进入报告的文件路径、字节数和 SHA-256。

CLI 在调用纯函数 aggregator 前重新读取所有 lineage Evidence 和 artifact：路径必须是仓库内的
相对路径，拒绝绝对路径、`..`、符号链接越界、缺失文件、字节数漂移和 SHA-256 漂移。Runner
只负责 schema/invariant 和状态聚合，不访问文件系统。

输出固定包含：Gate/authority/commit、schema/generator、corpus、平台来源事实、E1～E4 检查、
GT-G0-00～15 lineage、artifact metadata、issues 和 promotion。平台的详细设备、系统、编译器、
构建配置和 Skia backend 仍以来源 Evidence 为准；报告通过 `platforms[].evidencePath` 保留可追溯入口，
不得伪造或合并 iPhone/iPadOS 等独立 profile。

## 状态与晋级纪律

- 任一已冻结契约或 Evidence 完整性错误为 `FAIL`；不能通过修改阈值或删除 Evidence 绕过。
- 物理 authority 缺失为 `BLOCKED`；`promotion.allowed` 必须为 `false`。
- `G0_GATE_REPORT=BLOCKED` 不代表 GT-G0-17 已 review，也不允许进入 G1。
- 只有报告本身 commit-bound、schema-valid，且 GT-G0-17 独立 review 通过后，G0 才能标记 Pass。

## 验证矩阵

| 验证 | 断言 |
| --- | --- |
| E1/unit | 16 个 G0 task 齐全、状态和路径合法、报告 schema-valid |
| E2/reference | 五个平台 profile 分开记录，平台来源可定位 |
| E3/integration | Nightly PASS、reproducibility PASS，hosted 运行结果可重放 |
| E4/physical | Release authority 为 PASS 才能通过；hosted-only 必须 BLOCKED |
| Integrity | 缺失、路径穿越、符号链接越界、字节数或 SHA-256 漂移全部拒绝 |

# AR-0 架构对账文档验证记录 — 2026-08-23

> 状态：Validating / 待绑定 Git commit
>
> 范围：AR-0 文档重构，不包含产品代码、G0 实现或运行时门禁

## 1. 验证对象

本记录验证以下 AR-0 交付物是否形成一致、可执行的文档基线：

- 产品平台、Operation-only、一 Page 一 Document、Shared Data Runtime 和 Arc fallback 决定；
- `AR-0 → G0 → … → G9 → R5-B` 唯一晋级路线；
- G0～G9 Gate Task Tracker、R1～R5 里程碑状态表与 POC/RF 历史 Evidence 映射；
- 2026-08-23 动态 Notion capture 的读取边界、来源限制与用户明确裁决；
- 现行架构、需求、验证策略、历史 ADR/POC lineage 之间的一致性。

## 2. 验证身份与限制

| 字段 | 值 |
| --- | --- |
| 工作分支 | `codex/axiom-architecture-workflow` |
| 验证基点 HEAD | `3b14af2`（本轮 AR-0 文档仍在其后未提交） |
| 目标 Git commit | `pending`；用户尚未授权 commit |
| 验证日期 | 2026-08-23 |
| 内容范围 | 仓库跟踪/待跟踪的 README、`docs/` 与本轮历史 POC 说明；排除未跟踪 `.deps/` |

当前验证证明工作树中的 AR-0 文档集合自洽，但尚未形成 commit-bound Evidence。因而本记录不能
使 `AR0-01～06` 或 AR-0 变成 `Pass`，G0 继续保持 `Not Started`。

## 3. 已执行验证

### 3.1 Diff 与 Markdown/链接

```text
$ git diff --check
exit 0

$ python3 tools/check_docs.py
validated 90 Markdown files
exit 0
```

### 3.2 Task Tracker 结构与状态

对 [Gate Task Tracker](../../../planning/GATE_TASK_TRACKER.md) 执行只读结构审计，规则为：

- Task ID 必须唯一；
- 每行必须有 14 列；Design、Implementation、Validation、Final 只能使用批准的八种状态；
- Dependencies 只能使用完整 Task ID、完整闭区间或上游 Gate Pass，禁止自然语言前置；
- 各 Gate 的任务数必须等于本次来源 capture 与 repository reconciliation 的登记数。

结果：

```text
task_count=128; unique=128
gate_counts=AR-0:6, G0:18, G1:8, G2:7, G3:10, G4:9, G5:12, G6:11, G7:12, G8:12, G9:15, R5B:8
tracker_schema=PASS
```

G7/G8 的执行尚未开始，但因其已知 Open 决定会阻塞正确实现，Gate 聚合状态统一为
`Blocked`；未被该决定直接阻塞的子任务仍保持 `Not Started`。

### 3.3 Mermaid、路线与来源隐私

对仓库跟踪的 Markdown 与本轮待跟踪的 `docs/quality/evidence/ar0/*.md` 执行 fenced-block、
隐藏字符、私有来源 URL、本机工作区路径和 promotion route 扫描；排除 `.deps/`、
`node_modules/` 和生成目录：

```text
mermaid_fence_check=PASS
zero_width_check=PASS
private_source_url_check=PASS
local_workspace_path_check=PASS
second_promotion_route_check=PASS
```

仓库唯一 promotion 图位于
[Axiom Evidence-Gated 实现与验证总路线](../../../planning/AXIOM_GATES_AND_STAGES.md)，并严格表达
`AR-0 → G0 → … → G9 → R5-B`。POC/RF/R 只保留表格化 Evidence/milestone 映射。

### 3.4 现行语义残留与历史 Evidence

- 旧 `Transaction → operations[]`、Tauri Windows、Apple Tier B 等命中仅允许存在于历史 POC、
  被替代决定或冲突说明中；现行路径均指向 Operation-only、RNW 和 Tier-A iOS/iPadOS。
- Arc 图只保留 `Arc::Protocol → Arc::Core → Arc Platform Preview Backend`，Canonical Stroke 不
  再存在绕过 `Operation → Document → RuntimeScene → Canonical Renderer` 的直接渲染边。
- G2 固定 profile 已绑定 algorithm `1`、seed `0x43414e5641533033`、100K nodes、1K columns、
  cell size 32、固定 600-frame viewport trace、1920×1080 和 DPR 1。
- POC-03 Windows D3D12 p95/p99 历史 Fail 仍在 G5 blocker/lineage 中，没有被文档改写为 Pass。

## 4. 结果与晋级结论

工作树级文档验证结果为 **Pass**，但 AR-0 Gate 状态仍为 **Validating**：

- `AR0-01～06`：Design/Implementation 已完成；Validation/Final 仍为 `Validating`；
- R1、R3：`Not Started`；
- R2、R4、R5：因后续已知决定/任务 authority 缺失而为 `Blocked`；
- G0：`Not Started`；
- promotion：**不允许**。

剩余条件只有两步：在用户授权后提交当前 AR-0 文档集合，把本记录的 `pending` 替换为实际
commit，并复跑相同验证；随后由用户/架构评审明确批准 AR-0 Pass。任何一步未完成都不得进入
`GT-G0-00`。

# GT-G0-14：PR CI 依赖图设计

状态：`Accepted for implementation`
适用任务：`GT-G0-14 / IH-14`
上游：`GT-G0-07`、`GT-G0-10`～`GT-G0-13`

## 术语

- **Trusted root（可信根）**：后续结果赖以成立的、已通过结构和完整性校验的输入。本任务中的 schema、protocol corpus 和 semantic bootstrap 都是可信根的一部分。
- **Platform observation（平台观测）**：适配器实际观察到的事件、状态和诊断；它不自行判断通过或失败。
- **Shared Platform Comparator（共享平台比较器）**：唯一负责将 scenario oracle 与平台观测比较并生成结果的组件。
- **First divergence（首个分歧）**：按稳定检查顺序发现的第一个不一致点，用于避免不同实现输出不同的“首要失败”。
- **Run-set manifest（运行集合清单）**：一次 PR 验证应运行哪些层、哪些平台，以及为什么选择它们的机器可读记录。
- **PR decision（PR 决策）**：本次 PR 验证的聚合结论；它不是 G0 Gate Report，后者由 `GT-G0-16` 负责。

## 目标与非目标

本任务建立唯一的 PR 依赖链：

```text
schema
  ├── protocol
  └── semantic bootstrap
          ↓
selected platform contracts
          ↓
PR aggregate decision
```

目标是让错误停在正确层级，避免上游不可信时仍产生看似有效的平台 PASS；并使所有适配器只提交事实，由同一个比较器拥有判定权。

本任务不实现产品 Semantic Kernel，不把 POC-01 接口升级为产品 ABI，不建立 nightly/release 流程，也不生成最终 G0 Gate Report。后两项分别属于 `GT-G0-15` 和 `GT-G0-16`。

## 设计决定

### 1. 共享比较器

比较器输入为一个版本化 scenario 与一个或多个 observation。检查顺序固定为：

1. 输入身份和结构；
2. target applicability 与 capability；
3. `requiredEvents`，按声明顺序；
4. `forbiddenEvents`，按声明顺序；
5. `partialOrder`，按声明顺序；
6. `stateAssertions`，按声明顺序；
7. 多平台共享 checkpoint 的一致性。

首个失败写入结构化 `divergence`。`SPEC_REQUIREMENT` 和 `FREEZE_CANDIDATE` 的阻塞检查失败为 FAIL；`OPEN`、实验或基准观察可以产生 observation，但不能伪装成 correctness PASS。

适配器不得再生成 `COMPARATOR_DEFERRED` 结果。CLI 的 `run` 只写 observation；CLI 的 `compare` 调用共享比较器生成 result。

### 2. Semantic trusted-root bootstrap

G1 产品 Semantic Kernel 尚不存在。本任务建立一个明确标记为 `bootstrap` 的可信根：只运行 POC-01 已有的确定性 operation、replay、digest 和 scene projection host-core 测试，并输出固定格式的机器摘要。

这份摘要证明 PR DAG 能阻止语义前置条件失败继续污染平台 Evidence；它不满足 G1 的 60-case semantic corpus，也不能作为 G1 晋级证据。

### 3. 变更分类与运行集合

分类器只依赖仓库相对路径，不依赖 GitHub。规则必须保守：未知路径选择完整 PR 基线，不允许 Green-by-Exclusion。输出 run-set manifest，至少记录：schema、protocol、semantic 是否选择，选择的平台、命中规则和输入路径。

平台选择规则遵循最新基线：schema、corpus、common runtime/hooks 触发所有已完成平台；单个平台 adapter 只增量选择对应平台，但仍保留 protocol 前置；无法归类时选择全部平台。

### 4. PR decision contract

聚合器读取各层的版本化 job record，不解析供应商特有日志。结论只有：

- `PASS`
- `PASS_WITH_OBSERVATIONS`
- `FAIL`
- `INVALID_EVIDENCE`
- `BLOCKED_AUTHORITY`

结构、缺失、hash 或上游依赖无效映射为 `INVALID_EVIDENCE`；真实 semantic/platform correctness 失败映射为 `FAIL`；仅存在非阻塞 OPEN 观察时为 `PASS_WITH_OBSERVATIONS`。所有 attempt 都保留，retry 不得擦除先前记录。

### 5. GitHub Actions DAG

PR workflow 的稳定 job 名称为：

- `verification/schema-validate`
- `platform/protocol-seed`
- `conformance/pr-seed`
- `platform/pr-contract`
- `platform/pr-gate`

`platform/pr-contract` 仅在 schema、protocol、semantic 均成功后运行；aggregate 使用 `always()` 读取每一层机器记录，因此失败时仍能给出正确归层的 PR decision。每层始终上传自身 Evidence，失败时保留首个分歧和日志。

## 可量化退出条件

1. schema、protocol、semantic、platform 四种故意失败分别被归入正确层；
2. 上游无效时不得生成受信平台 PASS；
3. 同一输入重复比较得到逐字节一致的 result 与 first divergence；
4. 未知变更路径选择完整基线；
5. 所有适配器的 PR 路径只输出 observation，判定由共享比较器完成；
6. 全部选中阻塞 job 通过时，PR decision 为 `PASS` 或 `PASS_WITH_OBSERVATIONS`；
7. Evidence 记录 commit、corpus hash、run-set、attempt、结果和 artifact hash；
8. `GT-G0-16` 的 Gate Report schema/aggregator 保持未实现。

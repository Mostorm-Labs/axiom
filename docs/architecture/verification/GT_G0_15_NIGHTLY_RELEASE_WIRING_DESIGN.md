# GT-G0-15：Nightly / Release Wiring 设计

状态：`Approved in conversation; pending repository review`
适用任务：`GT-G0-15 / IH-15`
上游：`GT-G0-10`～`GT-G0-14`

## 术语

- **Cadence（验证节奏）**：触发完整验证的时机。本任务包含定时或人工触发的 Nightly，以及固定源码版本后人工触发的 Release Conformance。
- **Active matrix（活动矩阵）**：本次验证实际选择的平台 family、profile 和 case 集合。矩阵必须由版本化清单明确记录，不能依赖 workflow 中不可审计的隐式条件。
- **Platform family（平台家族）**：Web、Windows、Android、Apple 四类产品平台。Apple family 下的 iOS 与 iPadOS 是彼此独立的 profile，不得相互替代 Evidence。
- **Release profile（发布验证配置）**：为某个平台明确 runner、runtime、capability 和 Evidence 要求的配置。它不等于产品 Release 构建变体。
- **Run-set manifest（运行集合清单）**：绑定 source revision、corpus、profile、runner 和本次 case 选择的机器可读文件。
- **Evidence index（证据索引）**：列出本次运行全部输入、输出、hash、来源和完整性状态的机器可读文件。
- **Platform Release Decision（平台发布验证结论）**：Nightly 或 Release Conformance 的 provider-neutral 聚合结论。本任务生成的结论只具有 `G0_WIRING_ONLY` authority，不是 G0 Gate Report，也不是正式 G3 GateDecision。
- **Reproducibility comparison（可复现性比较）**：比较相同 source revision、corpus 和 profile 的两次运行是否得到相同阻塞结论；环境诊断字段不参与 correctness oracle。
- **PG-01～PG-06**：Platform Gate 的六组汇总视图，依次为 Semantic Parity、Host/Lifecycle、Surface/Generation、Device/Recovery、Bridge/Input 和 Release Authority Readiness。

## 目标与非目标

本任务把 `GT-G0-14` 已建立的可信根、共享比较器和四端适配器接入两种完整验证节奏：

```text
checked-in schema + corpus
            ↓
protocol + semantic trusted roots
            ↓
Web + Windows + Android + Apple profiles
            ↓
provider-neutral evidence index
            ↓
PG-01..PG-06 + PlatformReleaseDecision
            ↓
same-input reproducibility comparison
```

目标是证明完整矩阵可以被确定选择、执行、聚合和复现，并且在缺少物理设备证据或存在未关闭 authority 时正确阻断。Nightly 用于持续发现回归；Release Conformance 用于冻结某个 `main` commit 的输入和证据。

本任务不发布产品或 GitHub Release，不定义新的性能 SLO，不选择 device-farm 供应商，不解决 OPEN 物理实现决定，不把 POC/RF 接口升级为产品 ABI，也不生成 G0 Gate Report。`GT-G0-16` 负责消费本任务 Evidence 并聚合 G0；实际产品级 G3 Platform Gate 仍在 G3 执行。

## 设计决定

### 1. 一份可复用执行图，两个薄触发入口

Nightly 与 Release Conformance 共享同一份 reusable workflow，避免两套矩阵、artifact 和聚合规则逐渐漂移。两个入口只负责触发语义：

- Nightly 支持 `schedule` 和 `workflow_dispatch`，默认验证触发时最新的 `main` commit；
- Release Conformance 只支持 `workflow_dispatch`，必须传入完整 40 位 source commit，且该 commit 必须属于 `origin/main`；
- reusable workflow 接收 cadence、source commit 和重复轮次等显式输入；
- 所有 workflow 仅授予 `contents: read`，不创建 tag、Release 或产品发布资产。

Nightly 对相同输入执行两轮完整逻辑验证，用于生成可复现性比较。Release Conformance 至少执行一轮完整验证，并可消费同一 source commit 的第二轮或受信物理 Evidence 完成比较。重复执行的随机 seed 必须进入 run-set；本任务不以随机数据替代 checked-in correctness corpus。

### 2. 四个平台家族、五个独立 profile

完整矩阵至少包含：

| Platform family | Required profile | 执行边界 |
| --- | --- | --- |
| Web | `web` | React/TypeScript reference adapter 与 WASM/WebGL contract |
| Windows | `windows` | Windows native adapter；hosted runner 可做逻辑/原生正确性，不能冒充物理设备 |
| Android | `android` | Instrumentation adapter；emulator 与 physical Evidence 必须标明 reality |
| Apple | `ios` | iPhone profile；与 iPadOS 分开判定 |
| Apple | `ipados` | iPadOS profile；与 iPhone 分开判定 |

每个平台 adapter 只记录 observation；共享比较器继续拥有 correctness 判定权。缺少 family、required profile、capability 声明或完整 result 时，聚合器不得静默缩小矩阵。

### 3. 独立于 PR CI 的机器契约

Nightly/Release 不扩充 `GT-G0-14` 的 PR Decision schema，而是增加以下版本化契约：

1. **Full Run-set Manifest**：记录 cadence、authority、source commit、corpus hash、profile ID、runner/runtime version、seed、repeat 和 case 选择；
2. **Platform Evidence Index**：记录每个 profile 的 observation、result、环境 reality、artifact hash 和完整性状态；
3. **Platform Release Decision**：记录总状态、PG-01～PG-06、required/missing profiles、blocking reasons 和输入 identity；
4. **Reproducibility Comparison**：记录两次运行的 identity 是否可比、阻塞结论是否一致，以及不参与 correctness 的环境差异。

契约保持 provider-neutral：聚合器不解析 GitHub 日志或 job 名推断语义。GitHub Actions 只是生成和传递这些文件的一个 provider。

### 4. Authority 与状态优先级

本任务输出的 decision 固定包含：

```text
authority = G0_WIRING_ONLY
```

它证明验证基础设施如何决定，不宣称 G0 或 G3 已通过。状态优先级固定为：

1. schema、hash、identity、required record 或依赖关系无效：`INVALID_EVIDENCE`；
2. 正确性 authority 尚未决定，或 release 所需物理 Evidence 不存在：`BLOCKED_AUTHORITY`；
3. 受信比较器发现 correctness divergence：`FAIL`；
4. 只有非阻塞 OPEN/诊断观察：`PASS_WITH_OBSERVATIONS`；
5. 所有必需检查通过且不存在观察：`PASS`。

`REQUIRED_WHEN_CAPABLE` 根据版本化 capability 声明判断。capability 本身缺失或自相矛盾属于无效 Evidence，不能被解释为“不适用”。

### 5. 物理设备 Evidence 不得被模拟器替代

Release Conformance 可以在 GitHub hosted runner 上验证逻辑、契约和平台构建，但 hosted runner、模拟器和 emulator 不得被标为 physical。若 release profile 要求物理 Evidence 而相同 source commit 下没有受信报告，decision 必须为 `BLOCKED_AUTHORITY` 或 `INVALID_EVIDENCE`。

`GT-G0-15` 的任务 Pass 条件是 wiring 能正确运行、聚合、复现并在证据不足时正确阻断，不是现在伪造一个产品级 Release PASS。后续 G3 只有取得与目标 source commit、corpus 和 profile 一致的物理证据后，才可以使用同一 provider-neutral contract 形成真正的 G3 结论。

### 6. PG-01～PG-06 汇总

聚合器按版本化 case metadata 将结果投影到六组：

- `PG-01 Semantic Parity`
- `PG-02 Host/Lifecycle`
- `PG-03 Surface/Generation`
- `PG-04 Device/Recovery`
- `PG-05 Bridge/Input`
- `PG-06 Release Authority Readiness`

PG 分组只是 release evidence 的可审计视图，不重新运行 comparator，也不修改 case result。无法映射、重复映射或缺少必需分组的 case 使 Evidence 无效。

### 7. 可复现性与环境诊断

只有 source commit、schema/corpus hash、profile identity、runner/runtime version 和 case selection 完全相同的两次运行才可比较。比较输出至少包含：

- blocking status 与 blocking reason 是否一致；
- 各 PG 状态是否一致；
- profile/case result identity 是否一致；
- 不参与 correctness oracle 的环境诊断差异。

时间戳、runner 实例名、临时路径、下载耗时和硬件诊断等环境字段不得改变可复现性结论。同一输入产生不同 blocking conclusion 时，本轮 Evidence 为 `INVALID_EVIDENCE`，并保留双方 artifact 供调查。

## 数据流与失败边界

```text
Trigger
  ↓ validates cadence/source commit
Full Run-set
  ↓ schema + protocol + semantic prerequisites
Platform Matrix
  ↓ observations only
Shared Comparator
  ↓ per-case results
Evidence Index
  ↓ validates identity/hash/completeness
PG Aggregation
  ↓
PlatformReleaseDecision
  ↓ paired run with identical identity
Reproducibility Comparison
```

失败必须停在首次不可信的层级：上游 trusted root 无效时，不运行或不信任下游平台结果；某个平台失败不能被另一个平台 PASS 抵消；retry 必须保留全部 attempt；artifact 上传失败不能被 workflow 的绿色 job 状态掩盖。

## 实现边界

预计新增或调整的仓库边界如下：

- `verification/schemas/platform/`：完整 run-set、evidence index、release decision 与 reproducibility schema；
- `verification/packages/platform-conformance-runner/`：provider-neutral 聚合与可复现性比较；
- `verification/packages/platform-conformance-cli/`：生成 run-set、聚合完整 decision、比较重复运行的命令；
- `verification/fixtures/`：PASS、观察、correctness FAIL、缺失 profile、authority blocked、hash/schema 损坏和不可复现语料；
- `.github/workflows/`：reusable full-conformance workflow、Nightly 入口和 Release Conformance 入口；
- `verification/tools/` 与 `verification/evidence/g0/gt-g0-15/`：静态 workflow 检查、Evidence 生成和任务验证摘要；
- `docs/quality/evidence/g0/`、Gate Task Tracker 与 R1 milestone：只记录本任务事实，不提前生成 G0 Gate Report。

## 验证策略

实现遵循测试先行，至少覆盖：

1. 四个平台家族、五个 profile 齐全时生成确定 run-set；
2. 缺少 iOS 或 iPadOS 时不以另一个 Apple profile 替代；
3. 缺少 required profile、损坏 hash、错 source commit 和未知状态分别归入正确层；
4. 缺少物理 Evidence 时生成 `BLOCKED_AUTHORITY`，而不是 PASS；
5. correctness divergence 生成 FAIL，OPEN 观察只生成 `PASS_WITH_OBSERVATIONS`；
6. 相同 identity 的两次运行逐字节得到相同阻塞结论；
7. identity 不同的两次运行拒绝比较；
8. 环境诊断变化不影响 correctness；
9. workflow 静态测试确认 Nightly/Release 都调用同一 reusable workflow，release 固定 main commit 且权限只读；
10. hosted dry-run 能产出完整 artifact chain，并在没有物理证据时给出预期阻断，而非虚假 release PASS。

## Evidence 要求

本任务 Evidence 至少绑定：

- 实现 Git commit；
- workflow 文件 hash 与 hosted run URL/ID；
- source commit；
- schema、corpus、profile、runner/runtime 版本及 hash；
- 四个平台家族、五个 profile 的 observation/result 索引；
- PG-01～PG-06 汇总；
- Platform Release Decision；
- 两次相同输入的 reproducibility comparison；
- deliberate failure 的分层结果；
- artifact 清单及 SHA-256。

工作区阶段 Evidence 可标记 `WORKTREE`，但不能据此把任务标为 Pass。最终 Pass Evidence 必须在实现提交后重新生成，并由 hosted workflow 验证。

## 可量化退出条件

1. Nightly 和 Release Conformance 由同一 reusable execution graph 驱动；
2. Nightly 可重复运行完整 active matrix，并生成可复现性比较；
3. Release 绑定 `main` 上完整 source commit、checked-in corpus 和版本化 profile；
4. Web、Windows、Android、iOS、iPadOS 的 required record 均被枚举，任何缺失均有确定的非 PASS 结论；
5. blocking regression 能阻断 release Evidence，OPEN 差异不产生错误 correctness FAIL；
6. provider-neutral manifest 可以重建 source、corpus、runner、profile 和每个 case 的 Evidence 链；
7. schema、protocol、semantic、platform、authority、reproducibility 六类故意失败均落在正确层；
8. hosted dry-run 与静态 workflow 验证通过；物理 Evidence 缺失时明确阻断；
9. 输出 authority 为 `G0_WIRING_ONLY`，不生成 G0 Gate Report 或正式 G3 GateDecision；
10. `GT-G0-16` 与 G3 的实现范围保持未被本任务提前消费。

## 对路线的贡献

- Gate：为 G0 提供 Nightly/Release 验证基础设施，但单个任务 Pass 不代表 G0 Pass；
- R1：推进 Verification Foundation 的完整 cadence、证据冻结和复现能力；
- 后续：`GT-G0-16` 使用本任务产物形成 G0 Gate Report；G3 在真实产品集成和物理 Evidence 到位后复用 release contract，而不是复用本任务的 authority 结论。

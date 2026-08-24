# GT-G0-09 Platform Scenario Corpus Evidence

> 任务：`GT-G0-09`（Notion locator：`WP-G0-09 / IH-09`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 状态：`Validating`；28 个场景已落盘，等待 implementation commit 后生成 commit-bound Evidence

## 1. 目标、边界与术语

- **Platform scenario corpus**：Web、Windows、Android、Apple 共用的 logical platform
  correctness 语料。每个场景只有一份 `scenario.json`。
- **Platform-private expected truth**：某个平台自行维护的 expected 结果。G0-09 明确禁止它，
  平台差异只能通过 target policy、capability 和 observation 表达。
- **Fixture integrity**：场景中引用的 canonical/input fixture 必须真实存在、身份一致且有内容哈希。
- **Semantic validator**：在 JSON Schema 之外验证 28 个 stable ID、发布顺序、requirement/status、
  capability、fixture/step/checkpoint/generation 引用和关键 action recipe。

场景中的 `authorityRefs` 使用仓库内的脱敏来源标识（`SRC-NOTION-G0-IH09-CAPTURE-20260824`、
`SRC-NOTION-PLATFORM-SCENARIO-FORMAT-V01-CAPTURE-20260824`、
`SRC-NOTION-PLATFORM-SEED-V01-CAPTURE-20260824`），不写入受限 Notion URL。

本任务只 author corpus、validator、meta-tests 和结构化 Evidence；不实现任何平台 adapter，不运行
真实 Surface/GPU，不引入 pixel golden、性能阈值或平台私有测试逻辑，也不把验证 schema 升级为
产品 ABI。

## 2. 权威输入与交付物

本轮重新读取最新的 IH-09、Platform Scenario File Format、Platform Scenario Seed Set 和 G0
Implementation Plan。仓库不保存私有 page ID 或私有链接，只记录由其约束得到的实现结果。

交付内容：

- `platform-seed-v0.1` suite：固定 scenario/runner protocol version 和 28 个有序引用；
- 28 个共享 `scenario.json`：Foundation、Metrics、Lifecycle、Surface/Device、Bridge/Input、Arc、
  Destroy/Recovery families；
- 3 个共享 fixture：mixed operation replay、SetTransform operation、coalesced pen batch；
- schema + semantic validator 和正向/负向 meta-tests；
- 四份机器可读 Evidence：
  - [`scenario-validation-report.json`](../../../../verification/evidence/g0/gt-g0-09/scenario-validation-report.json)
  - [`suite-manifest.json`](../../../../verification/evidence/g0/gt-g0-09/suite-manifest.json)
  - [`case-requirement-capability-matrix.json`](../../../../verification/evidence/g0/gt-g0-09/case-requirement-capability-matrix.json)
  - [`fixture-integrity-report.json`](../../../../verification/evidence/g0/gt-g0-09/fixture-integrity-report.json)

## 3. 测试先行与关键约束

最初的正向测试因 suite/scenario 目录不存在而失败。物化初稿后，semantic validator 又正确发现
两个权威 fixture ID 尚未落盘：`OP-SETTRANSFORMS-VALID-001` 和
`POINTER-PEN-MOVE-COALESCED-001`。补齐 fixture 后，28/28 正向验证通过。

随后先加入 mutation tests，再扩展 validator。负向语料覆盖：

- 重复 suite member 与缺失 scenario；
- SPEC 场景空 oracle；
- 未知 capability；
- 损坏 fixture、checkpoint 和 generation-source 引用；
- hidden `RUNNING` precondition；
- platform-private expected truth；
- correctness sleep；
- Arc 场景错误声明 Web target。

关键场景还固定了可执行 recipe：01～03 的 attach 隔离；Surface loss 后使用
`PROVIDE_SURFACE_REBIND`；device loss 的 activate/clear；input hot path 的 deterministic
present hold；Arc fallback 与 canonical-visible/preview-clear 交接；Destroy stale-work 的
present hold、预置 LateEventFence、positive stale-source attempt 和 drain；重复 surface/device
恢复。所有 correctness 同步都基于 logical completion/event，不使用 sleep。

## 4. 当前验证结果

| 检查 | 当前结果 |
| --- | --- |
| suite membership | Pass；恰好 28 个 stable ID，唯一且顺序固定 |
| schema + semantic validation | Pass；28/28 |
| meta-tests | Pass；22/22 platform scenario tests，5/5 schema meta-tests，另含 Evidence generator test |
| shared authority | Pass；每个 ID 只有一个 `scenario.json`，无 per-platform scenario copy |
| fixture integrity | Pass；3/3 fixture 可解析并记录 SHA-256 |
| Arc/Web contract | Pass；3 个 Native Arc 场景无 Web target，显式 `NOT_APPLICABLE_BY_CONTRACT` |
| deterministic stale-work recipe | Pass；present hold + pre-armed fence + stale attempt + drain |
| adapter / Surface / GPU execution | 本任务不适用；由 `GT-G0-10..13` 承接 |

当前 worktree Evidence identity：

| 对象 | SHA-256 |
| --- | --- |
| platform seed canonical digest | `20d358607f04c7eed5843129ab5b9e33f612b216740c7de4d8d434aa57718741` |
| scenario validation report | `67c25eab160d6f19c3feaaac943d68fe8be49c9ffb73b27d9c6758e2724ca079` |
| suite manifest | `c12cbbfdd98ea3150fb5681d3360f5d12ee48bc4eecadb5c6c4b86b3b0a1a183` |
| case→requirement/capability matrix | `4a326e984dbab41014227a81da5ec390897f75d46d80695c50f0a303234f877b` |
| fixture integrity report | `4208aa7f4110d99c3937273942957f04194facc31af9fa266939d0b0e4286ad1` |

这些值尚未绑定 implementation commit；提交时必须从已提交实现重新生成四份 Evidence，记录
implementation commit/tree/archive hash，并再次运行完整 workspace 与文档校验。

## 5. Evidence 等级与任务状态

| 等级 | 适用性 | 当前结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | schema、semantic validator 与 mutation guards Pass |
| E2 Reference / Mock | applicable | canonical/input fixture 引用和结构化 Evidence generator Pass |
| E3 Integration / Golden | pending downstream | 真实 adapter 执行由 `GT-G0-10..13` 负责 |
| E4 Physical / Demo | not applicable to IH-09 | 本任务明确不运行 Surface/GPU/设备 |

因此 `GT-G0-09` 当前为 `Validating`，不是 `Pass`：implementation 已完整物化，但还没有在
implementation commit 上重新生成最终 Evidence。G0 与 R1 保持 `Validating`；本轮不进入
`GT-G0-10`。

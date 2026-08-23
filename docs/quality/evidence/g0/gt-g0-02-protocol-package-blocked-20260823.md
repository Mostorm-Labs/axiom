# GT-G0-02 Protocol Package 与 Envelope Codec 状态记录

> 任务：`GT-G0-02`（Notion locator：`WP-G0-02 / IH-02`）
>
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation
>
> 审计时间：2026-08-23（Asia/Shanghai）
>
> 结果：**历史 BLOCKED（上游已关闭）**

## 1. 阻塞结论

本记录保留一次真实的 preflight 阻塞。最新来源中的 `IH-02 — Protocol Package + Envelope Codec`
明确依赖 `IH-01`，其完成信号是：

> `typed protocol + strict parse/version rejection`

本任务页面同时要求 protocol package、typed IDs/enums/message parse、UTF-8 JSON HarnessEnvelope
codec、strict protocol/version/messageType/unknown-field rejection、tagged-u64 exact round-trip、
safe artifact path helpers，以及 package unit tests 和 dependency graph。

在本记录创建时，最新 `IH-01 — Schema + Workspace Skeleton` 的 Definition of Done 尚未在仓库满足：

- `verification/package.json`、`verification/tsconfig.base.json` 和既有 package-manager convention
  尚未建立；
- 需要的 Platform/Protocol JSON Schema 尚未完整 materialize；
- 需要每个 schema 至少一个合法 fixture、统一 schema validation command 和 negative meta-tests；
- `verification/` TypeScript workspace 尚未完成 build/typecheck。

上一轮提交 `996f178` 建立的是 Python workspace manifest 骨架，不是最新 IH-01 所要求的完整
Platform/Protocol schema workspace。因此当时 `GT-G0-01` 的 Pass 结论被发现过早，不能作为
`GT-G0-02` 的有效前置条件。没有执行 protocol package、没有生成 envelope codec、没有创建
`GT-G0-02` 的伪造 Pass Evidence。

## 2. 来源对账与冲突

| 来源 | 可确认内容 | 对仓库的影响 |
| --- | --- | --- |
| `IH-01 — Schema + Workspace Skeleton` | 12 个 Platform/Protocol schema、TS workspace、单命令 validate、build/typecheck、合法 fixture 和 negative meta-tests | 当前骨架缺失；`GT-G0-01` 需回到 `Fail` 或 `Implementing`，不能继续晋级 |
| `IH-02 — Protocol Package + Envelope Codec` | 依赖 IH-01；typed protocol、严格解析/version rejection、tagged-u64、safe path；不含 runner mutable state/transport | `GT-G0-02` 必须 `Blocked` |
| 仓库 `verification/`（`996f178`） | Python manifest v1、7 个 corpus 预留目录、无依赖 validator 和 9 个测试 | 只能 `Reuse` 为目录/治理输入，不能宣称 IH-01 DoD 完成 |
| 仓库 `GATE_TASK_TRACKER.md` | `GT-G0-01` 已记录为 Pass（历史误判） | 本轮纠正为 `Fail`；`GT-G0-02` 记录阻塞项 |

来源页面的文本还存在一个需在后续任务开工前澄清的计数不一致：IH-01 标题称“12 个 schema”，
但 Target files 清单目前列出 13 个 schema 文件（包含 `platform-protocol-meta-result.schema.json`）。
本记录不擅自删除任何一个 schema，也不把 12/13 选择写成仓库决定；该问题需要来源 owner 确认。

## 3. 恢复条件

上述恢复条件现已满足：

1. 固化 IH-01 的 Platform/Protocol schema 清单及计数解释（12 或 13，含明确 schema ID）；
2. 建立 TypeScript workspace/package、锁定现有包管理 convention，完成 clean build/typecheck；
3. 所有 schema 通过同一命令加载；每个 schema 至少有一个合法 fixture；
4. 负例覆盖未知 top-level field、`formatVersion != 1`、unsafe path 和 invalid generation encoding；
5. 更新 GT-G0-01 Evidence、状态和 commit-bound artifact hashes；
6. 只有 GT-G0-01 恢复为 Pass 后，才可实现 IH-02 的无状态 protocol package。

`GT-G0-03` 及后续任务不因本次阻塞而启动。本记录不代表 GT-G0-02 的最终结果。

## 4. 状态

本次 preflight 未修改产品代码，未实现 protocol package/codec，未执行 transport、runner state
machine 或 56 protocol vectors。当前 GT-G0-02 工作包已解除上游阻塞，正式实现和验证结果另见
后续 `GT-G0-02` Evidence；GT-G0-03 仍为 `Not Started`。

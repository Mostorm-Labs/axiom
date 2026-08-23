# GT-G0-02 Protocol Package 与 Envelope Codec Evidence

> 任务：`GT-G0-02`（Notion locator：`WP-G0-02 / IH-02`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-23（Asia/Shanghai）  
> 基线提交：`c466232`（GT-G0-01 revalidation；本任务修改仍按用户要求未提交）

## 1. 范围

本任务实现一个无状态、可独立构建的 TypeScript protocol package：

`verification/packages/platform-harness-protocol/`

它只负责 v1 protocol IDs、enums、typed payload parsing、UTF-8 JSON envelope codec、稳定拒绝
分类和 safe relative POSIX artifact path。它不实现 SessionRegistry、CompletionRegistry、eventSeq、
fault/fence state machine、transport、runner、平台 adapter 或 PASS/FAIL 判定。

Envelope v1 的 canonical 结构为：

```json
{"protocol":"axiom-platform-harness-exec-v1","protocolVersion":1,"messageId":"msg:00042","messageType":"HELLO","sessionId":"session:001","sessionEpoch":"u64:0000000000000007","payload":{}}
```

`messageType` 固定为 IH-02 的 17 个 v1 类型；exact u64 只接受 `u64:[0-9a-f]{16}` 字符串，绝不
通过 JavaScript number 表示。

## 2. 实现交付物

- `ids.ts`：brand 类型和 message/session/action/source/tagged-u64 校验器；
- `enums.ts`：17 个 v1 message type 和 completion outcome；
- `envelope.ts`：严格 envelope parse、UTF-8 fatal decode、JSON decode、encode 和稳定
  `ProtocolRejection` 分类；
- `session.ts`、`action.ts`、`completion.ts`、`event.ts`、`source.ts`、`fault.ts`、`fence.ts`、
  `scenario.ts`、`observation.ts`、`result.ts`：无状态 typed payload parsers；
- `profile.ts`：safe relative POSIX artifact path；
- `schema_loader.ts`：strict key/type helpers；
- `test/envelope.test.mjs`：round-trip、所有 message type、拒绝和边界语料；
- `test/dependencies.test.mjs`：无 runtime/platform 外部依赖检查。

协议 schema 与 typed semantic parser 分层：schema 负责结构/版本/字段形状，package 负责 branded
ID、message-specific payload 和错误分类。二者均拒绝未知字段，但没有引入 runner mutable state。

## 3. 验证命令与结果

```text
cd verification
npm run validate
npm run build
npm run typecheck
npm test
```

结果：

```text
schema validation: 13 schemas and 13 fixtures valid
4 schema meta-tests passed
workspace build/typecheck scaffold: valid
protocol package: 18 tests passed
TypeScript 7.0.2 build/typecheck: passed
```

覆盖项：

1. HELLO、OPEN_SESSION、ACTION_REQUEST、ACTION_RECEIPT、ACTION_COMPLETION、EVENT_DRAFT round-trip；
2. 其余 11 个 v1 message type 的 typed parser；
3. unknown envelope/payload field、protocol/version、unknown messageType、invalid UTF-8、invalid
   JSON、invalid tagged-u64 的稳定拒绝分类；
4. `u64:ffffffffffffffff` exact text round-trip；
5. `/tmp`、`..`、Windows drive、重复分隔符、反斜杠和空格路径拒绝；
6. package 无 dependencies、无 Node/React Native/native/runtime import。

首次测试曾因依赖扫描把合法枚举名 `ADAPTER_ERROR` 误判为平台 adapter 而失败；扫描规则已收敛
为检查实际外部 import 和运行时模块，随后完整测试 18/18 通过。该失败轨迹保留为验证过程，
不计入最终失败。

## 4. 状态与限制

实现、类型检查和测试均通过，`GT-G0-02` 当前任务结果为 `Pass`。本轮按用户要求未创建提交；
因此该结果是基于 `c466232` 加当前工作树的可复现本地 Evidence，不能单独宣称 G0 Gate Pass 或
R1 Accepted。提交协议包后应重新生成 commit-bound hashes，并再次运行同一命令。

`GT-G0-03` 及后续任务保持 `Not Started`。

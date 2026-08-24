# GT-G0-05 Scripted Adapter 与 Transport Evidence

> 任务：`GT-G0-05`（Notion locator：`WP-G0-05 / IH-05`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 基线提交：`2bb1ae9`；本任务按用户约束尚未提交

## 1. 范围与术语

- **Scripted Adapter**：不链接 Axiom 产品代码的 verification-only 可编程恶意 adapter，按有序
  script 发送协议消息、带 transport context 的 EventDraft、非法字节、断线和重连。
- **IN_PROCESS**：保留逻辑 envelope 语义、在同一进程内交付 UTF-8 JSON bytes 的测试 boundary。
- **Serialized loopback**：执行 UTF-8 JSON line encode/decode 和 portability validation 的逻辑
  out-of-process boundary；本任务不 fork 真实进程。
- **Transport metadata**：source/generation 等 transport-only context，不写入 canonical envelope，
  也不参与两种 boundary 的 logical state equality。

本任务不实现真实 stdio/socket/browser/device transport，不实现 Web/Windows/Android/Apple adapter，
不加入 sleep-based race，也不物化 `GT-G0-06` 的 56 vectors。

## 2. 来源对账与交付物

本轮重新读取 Notion 最新 IH-05，按其 Target files 和 Definition of Done 新增：

- `@axiom/platform-harness-transport`
  - `ITransport`、`InProcessTransport`、`SerializedLoopbackTransport`；
  - `JsonLineCodec` 与稳定 transport diagnostics；
- `@axiom/platform-harness-scripted-adapter`
  - `ScriptedAdapter`、`ScriptProgram`、`ScriptCursor`、`InboundInjector`；
  - `MalformedScriptCatalog`：stale epoch、duplicate completion、lease leak、fault pulse mismatch、
    malformed payload；
- Runner 的 transport-lost seam：断线关闭旧 session 并重置 HELLO negotiation；
- dependency proof：自动扫描两个新包的 dependencies 和 source，禁止产品 Runtime、React Native、
  native library 与 process transport 依赖。

## 3. 验证结果

```text
13 schemas / 13 fixtures: passed
protocol tests: 18 passed
Runner Core A/B tests: 20 passed
Scripted Adapter tests: 6 passed
Transport tests: 3 passed
Python workspace tests: 9 passed
TypeScript build/typecheck: passed
workspace manifest validation: passed
Markdown validation: 111 files passed
```

验收语料覆盖：

1. 同一 action + source lease + fence + event + finalization 流在两个 boundary 得到等价 checkpoint；
2. stale epoch、duplicate completion、lease leak、fault pulse mismatch 和 malformed payload 均由
   ordered script 稳定生成；
3. serialized boundary 拒绝 BigInt/非 portable envelope 和缺换行或损坏的 JSON line；
4. disconnect 后未重新 HELLO 不得 open session；重新 HELLO 和更高 epoch 后，旧 epoch 消息仍拒绝；
5. package dependency proof 确认不链接 Axiom/native platform libraries。

首次 workspace 测试暴露 scripted-adapter 在 transport `dist` 生成前编译的 clean-order 问题，以及
HELLO helper 未使用 branded ID；补充显式 prebuild dependency 和 branded constructors 后通过。随后
reconnect 测试因在同一 Runner 上先制造 HELLO failure 而触发既定 first-divergence 保留语义，拆为
两个独立重放场景后同时证明 HELLO 必需和旧 epoch fence。两次失败均保留在本轮执行轨迹中。

## 4. Evidence 等级

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | transport、script cursor、malformed catalog 与 reconnect invariants 通过 |
| E2 Reference / Mock | applicable | 双 boundary Reference Runner checkpoint 等价 |
| E3 Integration / Golden | not applicable | 56 vectors 和 CLI 属后续任务 |
| E4 Physical / Demo | not applicable | verification-only adapter 无设备依赖 |

## 5. 状态

`GT-G0-05` 的设计、实现和工作树验证为 `Pass`，commit-bound Evidence 为 `Pending`。G0 与 R1
Verification Foundation 继续为 `Validating`；`GT-G0-06` 保持 `Not Started`。提交实现后必须重新
执行验证并记录 implementation commit、tree、`git archive` 与关键文件 SHA-256。

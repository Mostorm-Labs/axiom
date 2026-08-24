# GT-G0-05 Scripted Adapter 与 Transport Evidence

> 任务：`GT-G0-05`（Notion locator：`WP-G0-05 / IH-05`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 实现提交：`512cc7b155e0706ccd09d4bda8df300cf02678b1`

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

提交后的完整验证结果已绑定到以下 commit identity：

| 对象 | SHA-256 / Git identity |
| --- | --- |
| implementation commit | `512cc7b155e0706ccd09d4bda8df300cf02678b1` |
| implementation tree | `97c608086874b1423d94f153bf4f12709155588b` |
| `git archive`（implementation commit） | `a5899024d40456d0638c211ff115c724cd16451835c656bf36abd8b4f610d4e2` |
| `verification/package-lock.json` | `0bdd46cfac18ef0f87f2510713eb6b237d8c6a81357005128f3ef745595e2c72` |
| `platform-harness-transport/package.json` | `2c1db2884079582c8a0f3a25d159aec01225a75869963a29cafd810e4013c072` |
| `InProcessTransport.ts` | `22ebfdd8b5051642f460b297accfa04c1b979e282f63d8af20da5d7eb4f15530` |
| `SerializedLoopbackTransport.ts` | `4089af8342319f06abbb578a156c93f449440dd853d8d972f3efbf4d6b9d9d5c` |
| `JsonLineCodec.ts` | `95f1511cffb8a0da27aed5020a5a46e86b7ad8fb55ee750148b6cec045e3e3c1` |
| `transport.test.mjs` | `a7998dc2d5573907eae537e5b0219b4a886b305a82af7afde58dc3f27f0e0c8e` |
| `platform-harness-scripted-adapter/package.json` | `d98e2b7e2edb0a07759efd443c006029354e81fd3c0fb959b95cb182ac187e7d` |
| `ScriptedAdapter.ts` | `faaa1c3a46c7d0e06da64ef957877b64f0022b8203e0c9544c79e0092380c174` |
| `ScriptProgram.ts` | `925cd9cce38ba00c0a6bd39f42e88d51422ddd27da72bafbaeea459eadaa6d13` |
| `MalformedScriptCatalog.ts` | `22967f6441b33004fa7baba7f5d2dcccd9d941cf040a2ff3fa81900e0f10609b` |
| `scripted-adapter.test.mjs` | `7644b0bb2e1ea2b573cd016636de0f011fbd809894488b0b10aea1ffd125a8b5` |
| `dependencies.test.mjs` | `8b38dde2202ae1574c1c82b3f341d2931dc2a17054d6be0d729d02333ac71b54` |
| Runner `ReferencePlatformRunner.ts` | `bd8fd9cc595faa7db4503a9413c0b85928b42f772f7e7733a941bfd4e7f30ac1` |
| Runner `SessionRegistry.ts` | `cc14898853da60cc71532ce6abb859b22bb02a28b34fc05e6b88868b0bca538a` |

`GT-G0-05` 的设计、实现和 commit-bound 验证均为 `Pass`。G0 与 R1 Verification Foundation
继续为 `Validating`；`GT-G0-06` 保持 `Not Started`。

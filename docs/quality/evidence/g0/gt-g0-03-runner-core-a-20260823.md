# GT-G0-03 Runner Core A Evidence

> 任务：`GT-G0-03`（Notion locator：`WP-G0-03 / IH-03`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-23（Asia/Shanghai）  
> 实现提交：`78e4d082c25c7376cfa9e820f2b63aca75ac3a1c`

## 1. 术语与范围

- **Runner Core A**：Shared Runner 的第一半可信状态机，只负责 handshake、session、command、
  action、completion 和 deterministic first divergence。
- **First divergence**：同一输入中最早出现的协议分歧；后续 diagnostics 可以保留，但不得覆盖
  首个稳定 category/location。
- **Completion token**：session 内一次性 terminal identity，不是 Product RequestId、GPU fence 或
  Canonical token。
- **Command sequence**：Runner 独占、连续分配的命令序列；Adapter 只能原样返回，不能跳号或改写。

本任务明确不实现 EventDraft/eventSeq、SourceLease、FaultHandle、LateEventFence（`GT-G0-04`），也
不实现 transport malformed injection（`GT-G0-05`）、CLI、56 vectors 或 PlatformObservation。

## 2. 来源对账与处置

最新 IH-03、Platform Harness Execution Protocol 和 Reference Runner 契约均要求：HELLO 后才能
OPEN_SESSION、adapter-instance-local epoch 严格递增、Runner-owned commandSeq、ActionReceipt 与
Completion 分离、token exact-once，以及 deterministic first divergence。

对仓库现状的处置：

| 范围 | 处置 | 结果 |
| --- | --- | --- |
| GT-G0-02 envelope codec 与 branded IDs | Reuse / Modify | 扩充 HELLO、ActionReceipt、CompletionToken 字段，但不改变 wire envelope 外层 |
| Shared Runner package | Missing | 新增 `@axiom/platform-harness-runner` |
| Handshake/Session/Command/Action/Completion registries | Missing | 本任务实现 |
| first-divergence collector 与 transition trace | Missing | 本任务实现 |
| Event/Source/Fault/Fence/transport | 后续任务 | 未提前实现 |

## 3. 实现交付物

`verification/packages/platform-harness-runner/` 包含：

- `ReferencePlatformRunner.ts`：Core A orchestration、ingress ownership、checkpoint/finalize；
- `handshake/AdapterHandshake.ts`：protocol version、adapter instance 与 boundary mode negotiation；
- `codec/HarnessEnvelopeCodec.ts`、`validation/ProtocolSchemaValidator.ts`：schema-before-mutation 和
  稳定 error-category 映射；
- `registry/SessionRegistry.ts`：`NEW → OPEN → CLOSING → CLOSED` 与 epoch monotonicity；
- `sequencing/CommandSequencer.ts`：Runner-owned tagged-u64 command sequence；
- `registry/ActionRegistry.ts`：request/receipt/sync-terminal/completion-pending/terminal；
- `registry/CompletionRegistry.ts`：one-shot token `REGISTERED → COMPLETED`；
- `diagnostics/ProtocolViolationCollector.ts`：deterministic first divergence 与追加 diagnostics；
- `test/core-a.test.mjs`：happy path、所有 IH-03 negative paths、replay 和 transition trace。

协议包同步补充 HELLO negotiation、boundary/completion/receipt enums、CompletionTokenId，以及
ActionRequest/ActionReceipt/ActionCompletion 的 typed fields。这些仍是 Verification-only 协议，
不升级为产品 ABI。

## 4. 验证结果

执行命令：

```text
cd verification
npm run validate
npm run build
npm run typecheck
python3 ../verification/tools/validate_workspace.py --print-digest
python3 -m unittest discover -s ../verification/tests -p 'test_*.py' -v
```

当前结果：

```text
13 schemas / 13 valid fixtures: passed
4 schema negative meta-tests: passed
GT-G0-02 protocol tests: 18 passed
GT-G0-03 Runner Core A tests: 13 passed
TypeScript build/typecheck: passed
```

Runner Core A 的 13 项测试覆盖：

1. handshake/session/action async completion happy path；
2. HELLO 必需与协议版本不兼容；
3. zero/reused session epoch；
4. stale epoch inbound 且 registry 不发生部分修改；
5. duplicate actionId 与 commandSeq mismatch；
6. missing completion token 与 invalid sync terminal；
7. unknown/duplicate/action-mismatched completion；
8. DISPATCH_ONLY 与 COMPLETED_SYNC terminal；
9. unresolved registry finalization；
10. 相同输入重复 10 次 first divergence category/location 一致；
11. 首个 divergence 不被后续 diagnostic 覆盖；
12. malformed wire 分类为 `INVALID_ENVELOPE` 且 schema-before-mutation；
13. Session/Action/Completion transition trace 顺序稳定。

首次 workspace build 曾失败，因为 `npm install --package-lock-only` 只更新 lock、没有在忽略的
`node_modules` 中物化新 workspace link；执行普通 `npm install --ignore-scripts` 后 build/typecheck
通过。该失败是本地 workspace link 缺失，不是用修改 expected 掩盖实现错误。

## 5. Evidence 等级与摘要

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | Core A 状态机、拒绝分类和 replay 全部通过 |
| E2 Reference / Mock | applicable | ReferencePlatformRunner deterministic trace 通过 |
| E3 Integration / Golden | not applicable | Scripted Adapter、transport 和 56 vectors 属后续任务 |
| E4 Physical / Demo | not applicable | Runner Core A 无平台设备依赖 |

实现、类型检查和测试结果已绑定到实现提交
`78e4d082c25c7376cfa9e820f2b63aca75ac3a1c`。本 Evidence 的收口修改随后独立提交，不改变
以下实现对象摘要。

关键 commit-bound identity 与文件 SHA-256：

| 文件 | SHA-256 |
| --- | --- |
| implementation commit | `78e4d082c25c7376cfa9e820f2b63aca75ac3a1c` |
| implementation tree | `886f235fda54246a147e77eeb4d91f1f60bd6fa0` |
| `git archive`（implementation commit） | `34eeb7d2386e50603556c75b43ec55089745cbd3042656aa3f915f8d3f40f845` |
| `verification/package-lock.json` | `e0ecec160d7f307a22fdd85050d0813d76e492fbd6d2550a6ea4afda539aeb9e` |
| `platform-harness-runner/package.json` | `7c7883eeaebd8efce5193be831751d2a34e8e8a7fbfe71ad245c3f039e03c546` |
| `platform-harness-runner/src/ReferencePlatformRunner.ts` | `ec624f29c7240ae2648bcb07d60cf17acbb3a28367f1538095f7367c188d0edd` |
| `codec/HarnessEnvelopeCodec.ts` | `174b5cfb3d7dc4da15f0cdb95a0e5759014afb7a9b3291c100c47d6d723ccf62` |
| `validation/ProtocolSchemaValidator.ts` | `4938a71178ba972cedfb1265a97fe41aef7494d1b298d82efa3dce51ab37a33d` |
| `diagnostics/ProtocolViolationCollector.ts` | `ae291b3a93711293ecc8d8477f8b180b0695be570a86972f232e861c06b88840` |
| `handshake/AdapterHandshake.ts` | `688c6956d0e032b3a4a471739fbfc27d48e6b940a36744ed06c363fd67f407aa` |
| `registry/SessionRegistry.ts` | `a45f3b457e17547ae280090b3a286bfc5b8c42c10a7f7df428e5526af2e7bca5` |
| `registry/ActionRegistry.ts` | `27324b188b85a7c06c8fa62da9da5a95f62bce056fb9cdd42ab65480518f1053` |
| `registry/CompletionRegistry.ts` | `263c1a48d08840b2eb2414216a43dd26a2da03dca32791b56c142b65023094e6` |
| `sequencing/CommandSequencer.ts` | `c5816ffc35f51fbbfab245f637c240b55ade65831b435369c465eba11b81fe54` |
| `test/core-a.test.mjs` | `dfbb85d15266b8746857ac6256f86538fdf743eb2a65cf889624adea022e6d8e` |

协议包同步修改的关键摘要：`action.ts` =
`8c543135f54e1d5466d1378b4dc8096b5a3fff1c98cf15e88fc1d5af4cfa6bfd`，`completion.ts` =
`f9eb98960c8b598ef948d33ea908dde34f5c0880810aebcec2dccb63013d1bfd`，`session.ts` =
`09c58eee9cce8f538b88cac4e05876008d89deb08075bf1ada5a195c9501d856`。

## 6. 状态

`GT-G0-03` 的设计、实现和 commit-bound 验证均为 `Pass`。G0 与 R1 Verification Foundation 继续为
`Validating`；单任务 Pass 不代表 G0 Pass 或 R1 Accepted。`GT-G0-04` 保持 `Not Started`。

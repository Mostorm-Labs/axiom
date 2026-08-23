# GT-G0-04 Runner Core B Evidence

> 任务：`GT-G0-04`（Notion locator：`WP-G0-04 / IH-04`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 实现提交：`eeb02b1654d06caa6a3a07b09325d1e29b836d0e`

## 1. 范围与来源对账

Runner Core B 补齐 Shared Runner 的 EventDraft/eventSeq、SourceLease、FaultHandle、
LateEventFence 与 finalization 状态机。`eventSeq` 由 Runner 连续分配；事件只有在 source lease
处于 OPEN、session 仍有效且 generation 等于当前 fence 时才可进入事件序列。活动 source 或 fault
均会阻止 session finalization。

本任务不实现 transport、Scripted Adapter、CLI 和 56 protocol vectors；这些分别属于
`GT-G0-05` 与后续任务。`EVENT_DRAFT` wire envelope 当前缺少 source/generation 上下文，因此通用
wire ingress 会明确拒绝；带上下文的 adapter transport 组合留给 `GT-G0-05`，不在本任务私造协议。

## 2. 实现交付物

- `sequencing/EventSequencer.ts`：Runner-owned、连续 tagged-u64 event sequence；
- `registry/SourceLeaseRegistry.ts`：OPEN/CLOSED lease、重复与未知 lease 拒绝；
- `registry/FaultRegistry.ts`：fault activate/clear handle 生命周期；
- `fence/LateEventFence.ts`：generation 单调推进与 late-event rejection；
- `ReferencePlatformRunner.ts`：Core B orchestration、checkpoint 和 finalization；
- `test/core-b.test.mjs`：7 项状态机、拒绝路径和十次 deterministic replay 测试。

## 3. 验证结果

```text
13 schemas / 13 fixtures: passed
protocol tests: 18 passed
Runner Core A tests: 13 passed
Runner Core B tests: 7 passed
TypeScript build/typecheck: passed
Python workspace tests: 9 passed
workspace manifest validation: passed
```

Core B 测试覆盖 source gating 与 eventSeq、重复/未知 source transition、late-event rejection 且不消耗
eventSeq、generation monotonicity、fault activate/clear、finalization unresolved registry，以及相同流程
重复十次 checkpoint/transition trace 完全一致。

## 4. Evidence 等级

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | Core B 状态机、拒绝分类与 finalization 通过 |
| E2 Reference / Mock | applicable | deterministic checkpoint/trace 十次一致 |
| E3 Integration / Golden | not applicable | transport、adapter 与 vectors 尚未进入本任务 |
| E4 Physical / Demo | not applicable | Runner Core B 无平台设备依赖 |

实现、类型检查和测试结果已在实现提交上重新执行并绑定到以下 identity：

| 对象 | SHA-256 / Git identity |
| --- | --- |
| implementation commit | `eeb02b1654d06caa6a3a07b09325d1e29b836d0e` |
| implementation tree | `4966640e3c0b691943f3ae827f525be00a1df752` |
| `git archive`（implementation commit） | `cbda5629fbc98c98031504a15931d0f5340383dd805de467a3113a221096c5b8` |
| `verification/package-lock.json` | `e0ecec160d7f307a22fdd85050d0813d76e492fbd6d2550a6ea4afda539aeb9e` |
| `ReferencePlatformRunner.ts` | `7d2d6e9a3b76428730293f583d631bae8ba462239f61fdf7beb5841bd135ac47` |
| `ProtocolViolationCollector.ts` | `61a2923ebf66a1620e549895767562126fc8df4d46fe803aac328c72d05d0c80` |
| `LateEventFence.ts` | `c3835861871f2709801e4c262a323d76ce55795c1d760075f1636a6025a08488` |
| `FaultRegistry.ts` | `e2e038f944e28250e2ba4fc62957eea94bf60efead4129bfb66e7583136ffe8f` |
| `SourceLeaseRegistry.ts` | `1a552a6cab13bdadab89bfae0af371f5c4f4e56ea8d86422864affe94a90213c` |
| `EventSequencer.ts` | `992ac208d1b31d16736c5e00ef46671d515feced4eac2eac66442e6cefa366f1` |
| `test/core-b.test.mjs` | `570eacb888a7975ee5e6755676131cf58993074ec7ce5f114fda8cee2dd456fb` |

## 5. 状态

`GT-G0-04` 的设计、实现和 commit-bound 验证均为 `Pass`。G0 与 R1 Verification Foundation
继续为 `Validating`；Evidence 收口时 `GT-G0-05` 尚未开始。

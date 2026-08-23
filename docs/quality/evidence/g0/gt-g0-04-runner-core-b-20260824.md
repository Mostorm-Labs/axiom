# GT-G0-04 Runner Core B Evidence

> 任务：`GT-G0-04`（Notion locator：`WP-G0-04 / IH-04`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 基线提交：`9cb0191`；本任务按用户约束尚未提交

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

当前结果基于 `9cb0191` 加本任务工作树，尚不是 commit-bound Evidence。提交实现后必须重新执行验证，
并记录 implementation commit、tree、`git archive` SHA-256 和关键文件 SHA-256；在此之前不进入
`GT-G0-05`。

## 5. 状态

`GT-G0-04` 的设计、实现和工作树验证为 `Pass`，commit-bound Evidence 为 `Pending`。G0 与 R1
Verification Foundation 继续为 `Validating`；`GT-G0-05` 保持 `Not Started`。

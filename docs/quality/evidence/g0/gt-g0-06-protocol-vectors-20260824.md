# GT-G0-06 Protocol Seed v0.1（56 vectors）Evidence

> 任务：`GT-G0-06`（Notion locator：`WP-G0-06 / IH-06`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 基线提交：`f6b90bc`；本任务按用户约束尚未提交

## 1. 已完成范围

- 物化 `verification/platform/protocol/v1/suites/protocol-seed-v0.1.json`；
- 物化七个 family、共 56 个稳定 `HPR-*` vector 目录和 `vector.json`；
- 每条 vector 均包含 `requirementStatus=FREEZE_CANDIDATE`、authorityRefs、focusAreas、
  `boundaryModes=[IN_PROCESS, OUT_OF_PROCESS]`、setup、ordered steps、expected oracle/terminal、
  deterministic first-divergence 与 no-partial-mutation assertions；
- expected 明确由 contract authoring 写入，测试禁止出现 `observed`/`actual` 反生成字段；
- `verification/tests/protocol_vectors.test.mjs` 提供 56 条 blocking structural/meta checks，验证 suite
  membership、ID 唯一稳定、七 family 各 8 条、重点负向场景覆盖和 expected policy；
- `npm run validate` 现包含 schema meta-tests 与 protocol vector meta-tests。

七个 family：Envelope/Session、ActionReceipt、CompletionToken、Source/Event/Attempt、FaultHandle、
LateEventFence、Boundary/Finalization。

## 2. 验证结果

```text
protocol vector/meta/mutation tests: 71 passed
suite membership: 56/56
IN_PROCESS + serialized-loopback execution: 112/112 passed
mutation guards: 7/7 detected
corpus integrity SHA-256: ee2f85dd532ee59abb5bb8e1079fd92f95f0541797ebdd5bc3e187a3b89decfb
schema validation: 13 schemas / 13 fixtures passed
protocol package: 18 passed
runner core A/B: 20 passed
scripted adapter: 6 passed
transport: 3 passed
Python workspace tests: 9 passed
TypeScript build/typecheck: passed
```

## 3. Runner 执行与 Evidence

`verification/tools/run_protocol_vectors.mjs` 逐条读取 contract-authored expected，在两个 boundary
上驱动 Reference Runner 并生成 112 个 `ProtocolMetaResult`。结果位于：

- `verification/evidence/g0/gt-g0-06/protocol-meta-results/{IN_PROCESS,OUT_OF_PROCESS}/`；
- `verification/evidence/g0/gt-g0-06/corpus-integrity.json`；
- `verification/evidence/g0/gt-g0-06/mutation-guard-summary.json`。

mutation harness 分别屏蔽 duplicate completion、stale epoch、lease leak、fault transition、late fence、
transport drop 和 boundary equivalence guard，确认对应 vector 必然 FAIL。普通 executor 不含自动 bless，
也不把 observed 复制为 expected。

## 4. Evidence 等级

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | schema、ID、family、expected policy meta-tests 通过 |
| E2 Reference / Mock | applicable | 112/112 Reference Runner executions 通过 |
| E3 Integration / Golden | applicable | 112 个 ProtocolMetaResult、integrity manifest 与 7 个 mutation guards 通过 |
| E4 Physical / Demo | not applicable | protocol corpus 无设备依赖 |

`GT-G0-06` 的设计、实现和工作树验证为 `Pass`，commit-bound Evidence 为 `Pending`。G0/R1 继续
`Validating`；`GT-G0-07` 尚未开始。

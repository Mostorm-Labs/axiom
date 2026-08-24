# GT-G0-06 Protocol Seed v0.1（56 vectors）Evidence

> 任务：`GT-G0-06`（Notion locator：`WP-G0-06 / IH-06`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 实现提交：`156cbea52d7131ed5da4d4af7c2eed87363221d9`

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

提交后的结果已重新生成并完整验证。commit-bound identity：

| 对象 | SHA-256 / Git identity |
| --- | --- |
| implementation commit | `156cbea52d7131ed5da4d4af7c2eed87363221d9` |
| implementation tree | `340987f04c362189bd44f4450beec0758b583849` |
| `git archive` | `41a6ff03e13e50fad904a527ffc2b2af419a09cf265007b9457900685c4fa2b6` |
| suite | `5a0103a7602f51aaf49c9a1b4ff64070e848839cae8d168028afd058d8667e6b` |
| vector executor | `81665ef43fa9d6ef0b928b778ecc9e0897a88213e2409e7de8d0a75b50b9ec8c` |
| mutation runner | `3f0ab9e3c1d2c3f8e663a11205d7e8ec81c8224faf5d797625adc1b3f3db4d72` |
| corpus integrity artifact | `9c2417d2e08081410d49a2ff555cf5f847ee7fe35de46badf5de3d822ebb5ba4` |
| mutation summary | `eb4f2af6b9082b5c2181fb890ad247d4b889d02777f5294305db5fe9d8f16b10` |
| ProtocolMetaResult schema | `2013f9e09015469ba680c0102ede2a53f98d64e6832fdf02c15fcfed14d4efb6` |

`GT-G0-06` 的设计、实现和 commit-bound 验证均为 `Pass`。G0/R1 继续 `Validating`；Evidence
收口时 `GT-G0-07` 尚未开始。

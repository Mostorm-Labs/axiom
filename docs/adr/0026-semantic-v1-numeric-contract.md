# ADR-0026: Semantic V1 数值契约与渲染几何分层

- Status: Accepted
- Date: 2026-08-26
- Related stages: G1, G2, G3, R1, R2
- Clarifies: ADR-0016

## Context

现行 ADR-0016 以 POC/Scene 的 binary32 canonical storage 为基线；Notion 的 V1
Semantic Schema Release Candidate 则把语义几何和文字尺寸定义为有限、可规范化的 f64
值。若把两个域继续写成同一个数值契约，G1 的持久化语义会被渲染中间表示反向限制，或者
让 Scene 的 float 表示误成为 Document 的持久化格式。

## Decision

- G1 Semantic Document、Operation payload、Snapshot、canonical projection 和 semantic
  digest 使用 authority 定义的 finite IEEE-754 binary64/f64 canonical scalar；进入
  `Atomic Operation Apply` 前拒绝 NaN、Infinity、溢出和超出 schema/hard-limit 的值，并按
  authority 规则将 `-0` 规范化为 `+0`。
- RuntimeScene、渲染临时值和 POC/RF 实验类型仍可使用 binary32/float，但不得反向成为
  Semantic Document、Operation 或 Snapshot 的 durable type。
- 从 Semantic Document 到 RuntimeScene 的 f64→render representation 转换必须是明确的
  SceneCompiler/renderer 边界，并通过跨端 replay/golden 验证；转换不改变 semantic digest。
- 本 ADR 不选择 Protobuf、FlatBuffers 或 custom binary；codec 仍由 G1 codec contract
  和 descriptor/corpus evidence 绑定。

## Consequences

- `runtime/foundation::WorldPoint/WorldRect`、`SceneRevision` 和 POC-01 的 float/uint64
  表示只能作为参考或派生表示，不能直接复用为 G1 durable semantic types。
- G1 的 canonical numeric tests 必须覆盖 finite、NaN/Infinity、negative zero、范围和
  byte-stable projection；G2/G3 需要验证编译后的 Scene 与 semantic revision 对齐。
- 如果未来要改变 Semantic numeric representation，必须增加 schema/digest migration ADR，
  不能由单个平台或 renderer 静默切换。

## Validation

GT-G1-01 记录该分层及来源对账；GT-G1-04 验证 reject-before-mutate 和 numeric normalization；
GT-G1-06 验证 snapshot/replay/digest；G2/G3 验证 semantic-to-scene 的等价性和跨平台结果。

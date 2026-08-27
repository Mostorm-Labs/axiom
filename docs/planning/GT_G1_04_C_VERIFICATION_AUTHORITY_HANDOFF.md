# GT-G1-04-C Verification Authority Handoff

> 状态：**PROPOSED HANDOFF / NOT CURRENT / NOT MANIFEST AUTHORITY**
>
> owner：`GT-G1-04-C`。本文是 A/C 分界记录，不是 15-operation corpus，也不冻结任何测试结果。

## 1. 为什么 C 独立

语义 authority 回答“一个输入值是什么意思、哪些值有效”；verification authority 回答“哪些经过
人工审阅的输入和预期结果可以证明它”。前者可在本轮形成提案并等待 human freeze；后者必须在
A semantic authority 与 B implementation semantics 存在后，按照 Golden Authoring trust boundary
独立物化。

因此，C 是 `GT-G1-04` overall Pass 的必要条件，但不是 A0–A3 authority readiness 的前置条件。

## 2. C 必须接收的输入

| 输入 | 所有者 | 备注 |
| --- | --- | --- |
| 经批准的 A semantic authority | G1-04-A | 版本、batch、ObjectKind、leaf structural policy。 |
| B stateful behavior | G1-04-B | idempotency、reference/kind/resulting-state、ApplyPlan 与 no-mutation 行为。 |
| 人工审阅的 positive intent | C human author/reviewer | 覆盖 15 operation families。 |
| 人工审阅的 negative intent | C human author/reviewer | 不能由实现失败倒推。 |
| stable expected stage/path/category（若选择冻结） | C human author/reviewer | 不得使用 protobuf parser exception string 代替语义 oracle。 |
| independent fixture compiler | verification-only tooling | 可用 Reference IDL descriptor/tag、generic protobuf primitives、verified metadata；不得以 production codec 作为唯一 generator。 |

## 3. 最低验证输出

- 15 个 Operation family 的 reviewed positive/negative case metadata；
- independent generated fixture 与 provenance；
- production codec / semantic runtime differential；
- canonical byte equality（适用时）、descriptor/profile mismatch coverage；
- rejected apply 的 no-mutation oracle；
- machine-readable first divergence；
- commit-bound durable Evidence，而不是只依赖短期 Actions artifact。

## 4. 禁止事项

- 不得从 production `SemanticCodec` / canonical writer 自动 bless expected bytes 或 expected outcome；
- 不得因 case failing 改 expected；
- 不得把 C++ 与自身输出比较称为独立 correctness；
- 不得把这个 C handoff 回写成 A blocker；
- 不得在此手册中开始 `GT-G1-04-B`、`GT-G1-04-C` implementation 或 `GT-G1-05`。

## 5. 再进入条件

`GT-G1-04-C` 只有在 A semantic freeze 已批准且 B 有可观察实现语义后才可获单独授权开始。届时
需要新工作包列出 case IDs、authority refs、fixture generation command、runner、Evidence path 与
human review records。

# GT-G1-04-A Authority Coverage Matrix (P34 re-entry)

> 本表是 P34 拒绝后的 verification/planning 记录，绑定本轮修复分支；不改写历史 source `85b025e…` 或 evidence `1613211…`。
> 规则只来自 `docs/notion/manifest.yaml` 列出的 Current Authority。B/C 行明确延期，不得由 A 实现代替。

| Authority rule ID | Authority source | Owner | Implementation location | Existing/new test | 初始审计状态 |
| --- | --- | --- | --- | --- | --- |
| G104-S01/S02 | operation structural semantics | A2 | `validator.cpp::validateEnvelope` | envelope validation | COVERED |
| G104-S03 | operation structural semantics | A1 | normalizer keyed batches | normalization | COVERED |
| G104-S09 | semantic leaf closure | A3 | `validator.cpp::validVectorPath` | leaf structure | COVERED |
| G104-S10 | semantic leaf closure | A3 | rich-text delta/document validators | leaf + new authority tests | COVERED |
| G104-S11 | semantic leaf closure | A3 | stroke validators | leaf pressure tests | COVERED |
| A1-R01 / W-04 | Common Wire + RichText authorities | A1/A3 | UTF-8 helper + text carriers | new UTF-8 tests | COVERED |
| A3-R02 | ImageContent release + geometry | A3 | image validator/normalizer | new image tests | COVERED |
| A3-R03 | Connector anchor contract | A3 | connector validator/normalizer | new connector tests | COVERED |
| A3-R04 | RichText Font V1 + paragraph authority | A3 | text-style/paragraph validators | new style tests | COVERED |
| A3-R05 | Pressure + Tilt V1 | A3 | pressure curve/sample validator | corrected curve tests | COVERED |
| A3-R07 | ShapeKind registry | A3 | shape validator | new shape tests | COVERED |
| A1/A3-R08 | protocol hard limits + OrderKey | A1/A3 | named hard-limit checks | new limit tests | COVERED |
| B-REF | connector/resource/reference authorities | B | ObjectStore/ApplyPlan | B tests | DEFERRED_B |
| C-OUTCOME | verification golden/outcome authority | C | independent corpus/oracle | C corpus | DEFERRED_C |

本轮目标：所有 A0/A1/A2/A3 行变为 `COVERED`；B/C 保持明确 owner 与 `DEFERRED_*`，不进入实现。

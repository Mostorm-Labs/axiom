# Corpus ownership

本目录按验证用途分栏。目录中的 expected 结果必须版本化、可复算，并由对应 Gate Task 评审；
blocking CI 不得自动 bless 或覆盖 expected。当前只登记 ownership，语料实体由后续任务添加。

| 目录 | Owner | 说明 |
| --- | --- | --- |
| `protocol/` | `GT-G0-02` | envelope/codec vectors；本轮不冻结 wire schema |
| `semantic/` | `GT-G0-06` | deterministic semantic vectors |
| `platform/` | `GT-G0-09` | Web、Windows、Android、iOS/iPadOS 与 macOS core 场景 |
| `golden/` | `GT-G0-08` | reference/golden 输入与受评审 expected |
| `performance/` | `GT-G0-09` | 固定 profile、seed、采样和资源语料 |
| `fault/` | `GT-G0-04` | fault、fence、损坏 artifact 与 recovery 前置语料 |
| `physical/` | `GT-G0-13` | 真实设备 evidence 的 manifest；不在本机伪造设备结果 |

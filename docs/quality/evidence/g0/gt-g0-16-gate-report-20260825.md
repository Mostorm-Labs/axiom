# GT-G0-16 Gate Report Evidence

## 结论

本轮实现已提交为 `d335c7a`（完整 SHA 见 Evidence manifest），并以该实现提交重新生成
`verification/evidence/g0/gt-g0-16/g0-gate-report.json`。报告 authority 为 `G0_GATE_REPORT`，
当前结果为 `BLOCKED`：E1/E2/E3 为 `PASS`，E4 因 Release 仍只有 hosted 证据而为
`PHYSICAL_RELEASE_AUTHORITY_BLOCKED`。这与 GT-G0-15 的保守 authority 规则一致。

## 本轮实现

- 新增 `gate-report` CLI 和 `createG0GateReport`；
- 固定 GT-G0-00～15 lineage、平台 profile、E1～E4 和 promotion 字段；
- CLI 对 lineage/platform/artifact 重新计算文件字节数和 SHA-256，并拒绝缺失、路径穿越、符号链接越界和漂移；
- 新增 Gate Report Schema、valid fixture、runner/CLI 测试。

## 验证结果

| 检查 | 结果 |
| --- | --- |
| Runner GateReport tests | 39/39 workspace runner tests passed |
| CLI tests | 24/24 passed |
| Schema inventory | 23 schemas / 23 fixtures |
| E1 | PASS |
| E2 | PASS；Web、Windows、Android、iPhone、iPadOS 独立列出 |
| E3 | PASS；Nightly 与 reproducibility 均 PASS |
| E4 | BLOCKED；hosted runner 不替代 physical device authority |
| Promotion | `false` |

## 证据文件

- [Gate Report](../../../../verification/evidence/g0/gt-g0-16/g0-gate-report.json)
- [Lineage input](../../../../verification/evidence/g0/gt-g0-16/lineage-input.json)
- [Hosted input](../../../../verification/evidence/g0/gt-g0-16/hosted-input.json)
- [Artifact input](../../../../verification/evidence/g0/gt-g0-16/artifacts-input.json)
- [Evidence manifest](../../../../verification/evidence/g0/gt-g0-16/manifest.json)
- [GT-G0-16 design](../../../architecture/verification/GT_G0_16_GATE_REPORT_DESIGN.md)

## 未关闭项

当前报告的 `sourceCommit` 绑定实现提交 `d335c7a4aa5bb3e934434f8c551b88f3ab3c6a62`；Evidence manifest
记录输入和报告的字节数与 SHA-256。E4 仍因缺少 physical release authority 为 `BLOCKED`，因此 GT-G0-16
保持 `Validating`，不能标记 `Pass`。GT-G0-17 review 和 G0 晋级不在本轮执行。

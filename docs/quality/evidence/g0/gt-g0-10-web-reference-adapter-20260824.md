# GT-G0-10 Web Reference Adapter Evidence

> 任务：`GT-G0-10`（Notion locator：`WP-G0-10 / IH-10`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 当前状态：`Pass`；实现提交：`6cbc2969bca6ddb1a1ba9b7450a401498dc55477`

## 1. 来源与边界

本轮重新读取并对账 IH-10、Platform Scenario File Format、Platform Scenario Seed Set、Platform
Harness Execution Protocol，以及已通过的 IH-08/IH-09 Evidence。IH-10 的核心边界是：

- Web 是 browser page + DOM Canvas host + Axiom WASM 的薄 logical adapter；
- adapter 只把 browser/WASM 事实归一化为 observation/event facts，不读取 scenario `expected`，
  不做 PASS/FAIL 比较，也不分配 runner-owned `eventSeq`；
- Web 不声明 Arc capability；Arc-only 场景通过共享 corpus 的
  `NOT_APPLICABLE_BY_CONTRACT` 产生 N/A，而不是 capability failure；
- correctness 同步使用 logical action/event，不使用 `sleep`；PointerEvents/coalesced events 通过
  批量路径进入 WASM，不经过 React state hot path。

Notion 私有 URL 和 page ID 不写入仓库；仓库只保留脱敏来源语义和可复现的 corpus digest。

## 2. Task Matrix 对账

| 项目 | 结果 |
| --- | --- |
| Reuse | GT-G0-08 common host/hooks、GT-G0-09 的 28 场景和 3 个 fixture、既有 schema/runner protocol |
| Modify | conformance CLI 增加 Web profile/run；verification workspace 增加 Web package |
| Missing（已补齐） | browser Canvas host seam、Web profile、logical action mapping、facts-only observation、Web N/A applicability report |
| Conflict（已避免） | 不把 Web Arc 缺失伪装成失败；不让 adapter 读取 expected 或输出 PASS/FAIL |
| Blocked | — |

## 3. 实现交付物

- `verification/packages/platform-harness-web/`：`WebReferenceAdapter`、`WebCanvasHost`、
  framework-free host page 和单元测试；
- `verification/packages/platform-conformance-cli/`：`profile --adapter web` 与
  `run --suite platform-seed-v0.1 --adapter web --output <path>`；
- `verification/tools/generate_web_adapter_evidence.mjs`：生成 profile、25 份 observation、25 份
  result、N/A applicability、summary 和 manifest。

共享 28 场景中 25 个 Web applicable 场景产生事实型 observation；3 个 Arc-only 场景产生 N/A。
adapter result 只标记 `OBSERVED_AGREEMENT_OPEN`，比较仍由 Shared Runner 负责。

## 4. 验证结果

| 检查 | 结果 |
| --- | --- |
| Web package tests | Pass；6/6 Node contract/schema tests + 1/1 Playwright Chromium host integration |
| CLI tests | Pass；9/9 |
| 全 workspace verification | Pass；94 个 verification tests；13 schemas/fixtures；28/28 platform scenarios；各 workspace tests 全部通过 |
| Web seed execution | Pass（reference observation）；25 applicable + 3 N/A |
| Arc/Web contract | Pass；两个 Arc case 与 Surface ownership case 均为 N/A |
| expected/comparator boundary | Pass；adapter observation 不含 `expected`、`result` 或 runner `eventSeq` |
| correctness sleep | Pass；包含 `sleep`/`delayMs`/`waitMs` 的 action 被拒绝 |
| real browser/WASM seam | Pass；Playwright Chromium 加载 framework-free host，验证 DOM metrics、surface loss/rebind 与 PointerEvent batch→WASM facade |
| real Axiom WASM/WebGL2 runtime | Pending；G0 当前尚无产品 Runtime 可连接，真实 runtime/GPU wiring 由后续 canonical runtime 与 CI 工作包承接 |

## 5. Evidence 与状态

结构化 Evidence：[`verification/evidence/g0/gt-g0-10/`](../../../../verification/evidence/g0/gt-g0-10/)

Evidence 的 `sourceCommit` 绑定到实现提交
`6cbc2969bca6ddb1a1ba9b7450a401498dc55477`，`implementationState` 为 `COMMITTED`。
本 Evidence 提交只补充绑定产物和状态追踪，不改变实现提交内容。

实现提交绑定摘要：

| 对象 | 值 |
| --- | --- |
| commit | `6cbc2969bca6ddb1a1ba9b7450a401498dc55477` |
| tree | `0b9de2bacb4fb84f37b51c9178df96fca862f20f` |
| `git archive` SHA-256 | `d6192db3ec3a6b9d0b593bd8674c7e6164588e06ba64ca9b80b678a0f1203b94` |
| Evidence `summary.json` SHA-256 | `ca6cebd8165f4a305b6fb7b4bcca3113c7e229e088623ff33639f817b2a4a46b` |
| Evidence `manifest.json` SHA-256 | `f39f1ccfad7db0251b632674aa28ab879abc0beecd448cc01f2c027280821b19` |

`GT-G0-10` 已满足本任务退出条件并标记为 `Pass`。G0 与 R1 仍为 `Validating`，因为
`GT-G0-11..17` 尚未完成；本轮不进入下一任务。

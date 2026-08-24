# GT-G0-10 Web Reference Adapter Evidence

> 任务：`GT-G0-10`（Notion locator：`WP-G0-10 / IH-10`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 执行时间：2026-08-24（Asia/Shanghai）  
> 当前状态：`Validating`（实现已完成；待提交后重新生成 commit-bound Evidence）

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
| Blocked | commit-bound final Evidence 需要本轮工作提交；当前按用户要求未 commit |

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

当前 Evidence 的 `sourceCommit` 绑定到最近已提交基线
`be70b0cbfbb85d7a40eb88048c40740d740ac2e9`，并明确标注
`implementationState: WORKTREE_UNCOMMITTED`。提交本轮改动后，必须重新生成 manifest 和
commit-bound SHA-256，才可将任务从 `Validating` 改为 `Pass`。

因此 `GT-G0-10` 的设计、实现和本地 reference validation 已完成，但 commit-bound Evidence
尚未绑定本轮实现。G0、R1 继续 `Validating`，不得晋级 G0-11 或 G1。本轮未执行 commit、push
或 PR。

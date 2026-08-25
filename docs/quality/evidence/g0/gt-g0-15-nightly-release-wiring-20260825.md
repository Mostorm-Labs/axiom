# GT-G0-15 Nightly / Release Wiring Evidence

> 任务：`GT-G0-15`（Notion locator：`WP-G0-15 / IH-15`）
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation
> 当前状态：`Validating`

## 本轮边界

本任务把 PR 验证基础接入 Nightly 与 Release Conformance 两种节奏，建立 provider-neutral 的 Full Run-set、Platform Evidence Index、Platform Release Decision 和 Reproducibility Comparison。

本任务不生成 G0 Gate Report，不生成正式 G3 GateDecision，不发布 GitHub Release，不引入性能 SLO，也不把 hosted runner、模拟器或 emulator 标记为物理设备。

输出 authority 固定为：`G0_WIRING_ONLY`。

## 设计与实现

| 项目 | 结果 |
| --- | --- |
| Reusable workflow | `.github/workflows/g0-full-platform-conformance.yml`；Nightly 与 Release 共用同一执行图 |
| Nightly trigger | `.github/workflows/g0-nightly.yml`；schedule/dispatch，固定两轮完整矩阵 |
| Release trigger | `.github/workflows/g0-release-conformance.yml`；人工传入 40 位 main commit，权限只读 |
| Required profiles | Web、Windows、Android、iOS、iPadOS 五个独立 profile；四个平台 family |
| Trusted root | schema → protocol → semantic → platform；下游依赖上游成功，aggregate 使用 `always()` 保留 Evidence |
| Release authority | 物理 Evidence 缺失时输出 `BLOCKED_AUTHORITY`，禁止模拟器替代 |
| Shared domain | `FullConformance.ts` 负责确定 run-set、Evidence index、PG-01～PG-06、decision 和 reproducibility |
| CLI | `full-run-set`、`aggregate-full`、`compare-full`；不修改 corpus、不接受 `--bless`/`--update-golden` |
| Machine contracts | 新增 5 个严格 Draft 2020-12 schema 与合法 fixture |

## 本地验证结果

| 检查 | 结果 |
| --- | --- |
| FullConformance runner | Pass；36/36 runner tests |
| Full schema inventory | Pass；22 schemas 与 22 fixtures |
| Full CLI | Pass；21/21 CLI tests |
| Workflow static contract | Pass；4/4 checks |
| WORKTREE Evidence generator | Pass；五 profile、四 family、六 PG、五类 deliberate failure、manifest hash |
| Workspace build/typecheck | Pass |
| Workspace validate/test | Pass；完整 workspace suite，包括 Web Playwright |
| `git diff --check` | Pending final run |

本地 Evidence 位于 `verification/evidence/g0/gt-g0-15/`。当前 `summary.json` 明确标记：

- `sourceCommit: WORKTREE`；
- `hostedValidation: PENDING`；
- `physicalReleaseValidation: BLOCKED_AUTHORITY`；
- `NOT_COMMIT_BOUND`、`NOT_G0_GATE_REPORT`、`NOT_G3_GATE_DECISION`。

因此本地结果不能替代提交后的 hosted Nightly/Release workflow。最终 Evidence 必须在实现提交后重新生成，并绑定 hosted run/artifact hash。

## 当前状态与剩余条件

- Design：`Pass`（设计已获用户确认）；
- Implementation：`Validating`；
- Validation：`Validating`；
- Final：`Validating`；
- G0：`Validating`；
- R1：`Validating`。

剩余条件：

1. 提交实现后重新生成 commit-bound Evidence；
2. 在 GitHub hosted runner 上运行 Nightly workflow；
3. 验证 Release Conformance 对缺少物理 Evidence 产生预期 `BLOCKED_AUTHORITY`，而非 PASS；
4. 记录 workflow run URL、source commit、schema/corpus/profile identity、artifact hash 和 reproducibility comparison；
5. 由 `GT-G0-16` 消费本任务 Evidence，生成 G0 Gate Report。

`GT-G0-15` 未满足最终 Gate 晋级条件；本轮不进入 `GT-G0-16`。

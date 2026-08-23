# GT-G0-00 仓库与分支基线对账记录

> 任务：`GT-G0-00`（Notion locator：`WP-G0-00 / IH-00`）
>
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation
>
> 对账时间：2026-08-23（Asia/Shanghai）
>
> 结果：**PASS（仅限 GT-G0-00）**
>
> 被观察提交：`fbf170ac04a20a9d1c50e048448cf78322ded487`

## 1. 术语与结论边界

- **Repository reconciliation（仓库对账）**：确认执行任务所依赖的仓库身份、分支、提交、
  版本、依赖锁、来源目录和证据入口彼此一致。
- **Evidence baseline（证据基线）**：本任务后续工作可以引用的、绑定到明确提交和文件摘要的
  起点；它不证明尚未执行的实现或平台门禁已经通过。
- **Disposition（处置分类）**：`Reuse` 表示可以直接复用；`Modify` 表示已有基础但后续需调整；
  `Missing` 表示尚不存在；`Conflict` 表示与现行基线冲突；`Blocked` 表示缺少上游决定或外部条件。

AR-0 已在当前 `main` 正式标记为 `Pass`，因此 `GT-G0-00` 的上游依赖满足。本记录只关闭
`GT-G0-00`；G0 仍处于 `Validating`，`GT-G0-01`～`GT-G0-17` 没有因本次对账而启动或通过。

## 2. 仓库、分支与提交

| 项目 | 观察值 | 结论 |
| --- | --- | --- |
| GitHub repository | `Mostorm-Labs/axiom`（public） | `Reuse` |
| Git remote | `origin = git@github.com:Mostorm-Labs/Axiom.git` | `Reuse`；GitHub 名称大小写归一化不改变仓库身份 |
| 默认分支 | `main` | `Reuse` |
| 当前分支 | `main` | `Reuse` |
| HEAD / origin/main | 均为 `fbf170ac04a20a9d1c50e048448cf78322ded487` | `PASS`；本地没有落后或超前 |
| HEAD subject | `docs: mark AR-0 architecture gate passed (#33)` | AR-0 正式状态与提交历史一致 |
| Git tree | `58bface4a9084f92916af8a51d44340bc9bc3950` | 后续复现标识 |
| `git archive` SHA-256 | `4fe60819f74fd6526049f36928690db1fd140e98cc28f00abc9bd2721bbab490` | 后续复现标识 |
| 工作区（对账开始时） | clean | `PASS` |
| `main` branch protection | GitHub API 返回 `false` | `Modify`；G0 CI/合并保护由后续任务建立，本任务不改远端策略 |

当前仍可见 4 条非 `main` 分支引用：`codex/ar0-pass`、
`codex/axiom-architecture-workflow`、`origin/codex/macos-platform` 和
`origin/codex/windows-vertical-slice`。它们都不是当前 `main` 的祖先；前两条包含 squash 合并前的
历史提交，后两条是历史平台工作分支。它们不改变 `main` 基线，也不阻塞本任务。本轮未删除、
合并或改写任何分支。

## 3. 版本、Tag 与 Release

根工程当前声明 `project(canvas_v2_pocs VERSION 0.1.0)`、C++20，默认仍构建 POC-01；
`CMakePresets.json` 主要由 POC/RF preset 组成。因此现有根构建组织处置为 `Modify`：它可以作为
历史验证入口，但不是 G0 的正式 schema、runner、corpus 或 report workspace。本轮不修改 CMake。

当前 `main` 可达的重要供应链/历史证据 tag 包括：

- `skia-sdk-r1-full-v1-54c1999dc79d094d`；
- `skia-sdk-poc01-minimal-v1-debcbb7b9376806c`；
- `skia-sdk-poc04-richtext-v2-72f006b19ac77233`；
- POC-01/03 的物理设备 Evidence tags。

`v0.1.0-alpha.1` 仍存在，但不在当前 `main` 的祖先链上；它是历史 Release，不是当前产品版本
基线。GitHub 上的 Skia SDK 与 POC Evidence 均保留为 prerelease。未发现需要由本任务解决的
版本冲突。

## 4. 依赖与工具链锁

| 文件 | SHA-256 | 已核对内容 | 处置 |
| --- | --- | --- | --- |
| `deps.lock.json` | `daaf00c48fa3b47cb9f8412b01d9aa177c6eb51dc5143d75d3a70f4f74c559c9` | Skia `b6d1062…`、Emscripten 6.0.6、LLVM 22.1.8、Node 24.18.0、Android NDK 27.2/API 26、Web 与测试依赖 | `Reuse` |
| `skia-sdk.lock.json` | `f0dd144db4d9fac12924d63393f0e65e0e708e534aa600b26b3446218abe35d3` | `poc01-minimal-v1`、7 targets、set ID `debcbb7b…` | `Reuse`，仅作历史 POC consumer baseline |
| `r1-full-skia-sdk.lock.json` | `de966037aff53fb173f3c72f929413f98fe20f3bb05ddaff4cd893ac110c9d75` | `r1-full-v1`、8 targets、release/debug/asan、set ID `54c1999d…` | `Reuse`，后续产品 Gate 的 SDK 输入 |
| `tools/skia/profiles/r1-full-v1.json` | `37ecd74a897bf1ec6aab94014dc19b2b05fcaffece53b78f17f985cf39032248` | R1 Full producer profile | `Reuse` |

两个 SDK lock 均通过仓库 validator；Skia consumer/SDK tools 共 52 个单元测试通过。该结果只证明
锁文件和供应链工具在本机可验证，不替代 G0 协议、平台或产品验证。

本地观察工具版本为 CMake 4.0.1、Python 3.9.6、Node 22.14.0、Ninja 1.11.1、Apple clang
17.0.0。它们是本次审计环境而非新的依赖锁；例如本机 Node 与锁定的 Node 24.18.0 不同，后续
可复现 CI 必须使用锁定版本。macOS 审计没有寻找、安装或验证 MSVC runtime。

## 5. Workflow 与验证资产

仓库现有 8 个 workflow：Arc、POC-02、POC-03、RF-01、R1 Full producer contract/release/
consumer validation 和 R1 Full Skia producer。它们处置为 `Reuse/Modify`：可复用供应链和历史
验证机制，但当前不存在独立的 G0 conformance workflow、统一 G0 runner/corpus 或 Gate Report
aggregator。这些缺失项已分别由 `GT-G0-01`～`GT-G0-16` 追踪，不属于本任务的实现范围。

本任务的证据等级为：

| 等级 | 适用性 | 结果 |
| --- | --- | --- |
| E1 Contract / Unit | applicable | 仓库、锁文件、来源和 Tracker 对账通过 |
| E2 Reference / Mock | not applicable | 本任务不实现产品行为或 reference oracle |
| E3 Integration / Golden | not applicable | 本任务不产生产品集成或视觉输出 |
| E4 Physical / Demo | not applicable | 仓库元数据对账不需要真实设备 |

## 6. 来源与权威边界

本次使用以下仓库内来源：

- [AR-0 架构对账报告](../../../planning/AR0_RECONCILIATION_REPORT.md)：确认 AR-0 正式 `Pass`；
- [G0～G9 唯一路线](../../../planning/AXIOM_GATES_AND_STAGES.md)：确认 G0 任务顺序和晋级纪律；
- [Gate Task Tracker](../../../planning/GATE_TASK_TRACKER.md)：确认 `GT-G0-00` 身份、依赖和 R1 映射；
- [验证策略](../../VERIFICATION_STRATEGY.md)：确认 E1～E4、Evidence 和状态纪律；
- [来源目录](../../../architecture/review/SOURCE_CATALOG.md)：确认来源身份与读取边界。

来源目录当前为 `Reviewed / Source inventory complete`，共有 58 条唯一记录。其中
`SRC-NOTION-ARCH-V03-CAPTURE-20260823` 是本轮 G0～G9 动态来源 capture，
`SRC-USER-ARCH-REBASE-CONFIRMATION-20260823` 是用户直接确认的架构约束。动态 Notion 页面没有
不可变 revision/hash；部分历史聊天来源仍为 Partial 或缺少 fingerprint。这是来源可复现性的
已知限制，但不会把 Draft/Open 内容升级为 Accepted，也不阻塞只核对仓库执行起点的
`GT-G0-00`。私有 Notion/ChatGPT URL、page ID 和 block UUID 未写入本记录。

## 7. 对账结果与后续边界

| 范围 | 处置 | 结果或后续归属 |
| --- | --- | --- |
| AR-0 上游依赖 | `Reuse` | 正式 `Pass`，本任务可以执行 |
| repository / `main` / `origin/main` | `Reuse` | 一致且工作区起始 clean |
| 依赖 lock 与 R1 Full SDK | `Reuse` | schema validator 与 52 个工具测试通过 |
| 根 CMake / presets | `Modify` | 仍以 POC/RF 为中心；后续任务处理，不在本轮改代码 |
| G0 schema、workspace、runner、corpus、platform adapters、CI、aggregator | `Missing/Modify` | 由 `GT-G0-01`～`GT-G0-16` 依赖顺序处理 |
| G0 Gate review | `Blocked` | 必须等待 `GT-G0-00`～`GT-G0-16` 全部通过 |
| Open conflict | — | 未发现阻塞 `GT-G0-00` 的冲突 |

因此 `GT-G0-00` 的设计、执行和验证状态均为 `Pass`，最终状态为 `Pass`。这只使 G0 从
`Not Started` 进入 `Validating`；G0 不满足晋级条件，R1 Verification Foundation 也只进入
`Validating`，R1 仍未 Accepted。

对账执行阶段明确未执行：产品代码修改、`GT-G0-01`、构建系统调整、分支清理、push、merge、
PR 或远端仓库策略修改；本记录只在用户后续明确授权后单独提交。

## 8. 可复现命令摘要

```text
git status --short --branch
git rev-parse HEAD origin/main HEAD^{tree}
git archive --format=tar HEAD | shasum -a 256
git remote -v
git branch -vv
git branch -r
git merge-base --is-ancestor <ref> main
git tag --merged HEAD
gh repo view --json nameWithOwner,defaultBranchRef,isPrivate,url
gh api repos/Mostorm-Labs/axiom/branches/main --jq '.protected'
gh release list --limit 20
shasum -a 256 deps.lock.json skia-sdk.lock.json r1-full-skia-sdk.lock.json \
  tools/skia/profiles/r1-full-v1.json
python3 -m unittest tools.skia.tests.test_consumer tools.skia.tests.test_sdk_tools -q
PYTHONPATH=tools/skia python3 -c \
  'from pathlib import Path; from consumer import load_lock; load_lock(Path("skia-sdk.lock.json")); load_lock(Path("r1-full-skia-sdk.lock.json")); print("SDK locks: valid")'
```

观察结果：52 tests passed，两个 SDK lock valid；最终 Markdown、链接、fence、隐私和 diff scope
校验见对账工作区校验输出。被观察 commit 是任务开始时的基线；Evidence 文件的内容身份由用户
后续明确授权的独立提交承载，不反写尚未生成的自引用 commit。

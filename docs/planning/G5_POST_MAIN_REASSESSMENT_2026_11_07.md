# G5 合并主线后的计划复核（2026-11-07）

本次角色为 P30 实现规划与受控集成。目标是合并最新 main 并重新排序后续工作，
不启动新一轮性能实现、不调整公开 API／预算／算法 authority，不签发 Gate PASS。
当前用户已确认 4242 的暂停后缩放／平移体验可用，并要求性能优化先收口。

## 集成边界与依据

- 仓库：Mostorm-Labs/axiom；受控分支：codex/geometry-navigation-integration。
- 合并前分支：`17688e578c21372f2ed556da7d5ba34430077c05`。
- 合并提交：`91452b7a2aaaadb7c4aecc6604f770453fb6ec1f`；两侧父提交及精确验证树见本次 evidence。
- 本次拉取 main：`e32dee039e4095073b60d81895ff0788145afc3b`；SDK 版本 0.4.3。
- 当前 authority 从 `docs/notion/manifest.yaml` 定位；SDK 仍服从
  `sdk/js-v1-contract.md`、`windows-consumer-v1.md`、`shell-axis-preview-contract.md`
  （位于 docs/planning）。本文件是后续建议，不替代这些契约或已有冻结 package。
- main 已通过 `7dccf70d` 包含宽视口保留修复 `0d581f89` 和证据 `d98ac4ca`。
  本分支新增的 `17688e57` 为混合场景回归测试。自动合并没有冲突；合并相对 main
  的差异为测试、测试注册、测试说明及既有测试证据，生产实现没有另造一条路径。
- `.aegis` 当前登记未包含正式 G5 Gate；全局 state 缓存未列 blocker 不代表
  G5 通过。旧 G5 BLOCKED 材料、独立 review 和规模资格边界继续保留。

状态 owner 不变：输入／交互／camera／History 与提交由共享 Runtime 管理；
canonical mutation 由 OperationEngine 管理；Scene 为派生不可变投影；
Render 管理 coverage、source、prepared geometry、Damage、预算及 GPU 生命周期；
Skia 统一绘制，provider 提供目标和 present。App／SDK 不增加几何、文档或历史 owner。

## 已有实现与证据的正确解释

| 范围 | 当前实现 | 后续处理 |
| --- | --- | --- |
| 导航反馈、冷区域 coverage、精确收敛 | 已有共享 Render 调度、coverage 作业复用、保留画面和精确局部重放 | 维护现有回归；具体恢复长尾仍需当前版本测量 |
| contour / prepared path | 已实现局部 contour、vector prepared geometry 及 canonical 接入 | 不再作为“第 2 步尚未开发”重复实现 |
| chunk / source / Scene | 已有稳定派生 chunk 身份、局部 BVH、不可变 payload 共享、COW 发布与预算归属 | 不声称所有 metadata、Hierarchy 或 Full recovery 成本都已局部化 |
| GPU 资源 | 已有驻留／staging 分离、实际完成后释放、retirement 与跨 context owner | 维护失效／完成／预算拒绝检查；不重复设计新 GPU pool |
| 放大、暂停、再缩小／平移 | 宽／窄保留画面在同一预算内参与 coverage；已有 8 个组合和 4 个失效检查 | 保留用户确认的 4242 和冻结 14-run 证据；新版需自己的 source/artifact 身份 |
| 编辑后导航 | main 新增复制粘贴、live erase、affine masked stroke 导航和容量压力下的局部 damage | 纳入统一回归；不能用旧未擦除笔画的结果替代 |

实现、历史证据和正式资格分开。`g5-material-tile` 包含撤回实验，
`g5-retirement-arbitration` 的 BLOCKED 记录未被后来的局部修复改写。
`g5-pause-resume-repair` 证明指定复现修复，不能外推所有 DPR、恢复、平台或规模。
主线 release 文档中的本地测量与 out/ 路径同样是各自版本的历史证据，
不是本次合并已经重跑的结果。

## 调整后的顺序

### 1. 统一合并后的稳定性回归基线

本轮集成先确认两侧测试均保留，运行共享 Render／Runtime 回归、G5 Node、
boundary 和匹配 JS／types／WASM／resources 的公共 SDK 浏览器检查。
验证结果另存 `verification/evidence/performance/g5-main-integration-2026-11-07/`。
不覆盖 4242，不把旧 SDK 数据拼到 0.4.3 的测量里。

本次复核发现的首项维护工作是对齐 4 个旧创建 fixture：
`FC01Creation-arrow-arrow`、`FC01Creation-arrow-arrow-pointer`、
`FC01Creation-line-line-pointer`、`FC01CreationSources`。
它们仍假设创建一次后工具持续激活，甚至有明确的旧行为注释；
主线 `uiux-default-interaction-v1.json` 与 `selectCreatedObject()` 已要求成功后转点选。
后续需补“创建后选中”的断言，并在第二次创建前显式重选工具，继续验证同 pointer
重用和其他 source 取消。不能删除／跳过这些测试，也不回退新的产品行为。
这属于验证契约漂移，不能记成 G5 导航性能回归；本轮仅记录，不静默扩为修复任务。

本轮 1076 项原生测试分两组完整覆盖：关键 4 项通过，剩余 1072 项中 1002 项通过；
共 1006 项通过、70 项失败／未运行，其中包含上述 4 项和此前已有的 66 项。
构建仍有原先 4 个 Semantic 测试的 switch 枚举覆盖编译问题。另有 39 项 G5 Node、
9 项 controller Node、119 项 boundary 检查通过。匹配 0.4.3 安装产物的公共 SDK
浏览器检查通过混合场景暂停后缩放，以及 DPR2 的 16-source 连续擦除、Undo／Redo、
cancel、12 次连续 sweep 和后续双指 pan／zoom。安装仍为 36 文件、仅根 exports，
未安装私有头／archive。这是合并过程中对精确源树和产物的本地集成验证；
SDK 内嵌 revision/dirty 字段不能代替记录中的实际 merge tree／package SHA，
不作为新的 clean-source 配对性能基线或正式 Gate 结论。

下一轮若启动性能工作，先冻结小矩阵：原始混合场景与选中状态；
复制／变换／多次局部擦除后再导航；便签／表格编辑、图片资源替换、Undo／Redo；
前台 idle、冷区域、context lost/rebind、DPR1/2，以及单列的 DPR3/4 压力情况。
同时保留内容提交空窗、精确 receipt 等待、高清恢复、文档／operation 不变性、
最终像素和已存在预算计数。物理触控板体验与合成 wheel/touch 测量分别记录。

这一步以验证为先，只有可重复的实际退化才开启新的 P35/P36 修复。
不因旧报告出现一个数值，就恢复无边界的性能调参。

### 2. 按当前证据选择冷准备与恢复长尾

候选是资源首次 decode/upload、缺失 source raster、带历史 masks 的重放、
高 DPR 下高清 source 准备及 canonical 最终收敛。先区分这些阶段与调度等待，
再选择最小热点；缓存命中后的良好手感不代表冷路径已经闭合。

main 的 damage 快路径仅适用于已验证的 append-only 状态、可保留目标和支持的
CompactStroke。冷 miss、非 prefix／FilledPath、外部 clip/写入或失效目标仍走
完整替换。RenderGroupManager 仍比较源／mask prefix；这些是需量化的候选成本，
不是本次已确认的新根因。新增 workload 应包含 mask／segment 数，避免只数笔画点。

若确认性能缺陷，再冻结局部工作和验收：保持默认 32/96MiB、有限工作机会、
完整覆盖和成功 present 才推进 receipt。不能通过扩大预算、提前发 receipt、
降低精确像素或重新引入 Shell renderer 制造改善。

### 3. 先澄清大场景装载边界，再分层扩量

历史 `g5-complex-scale/result.json` 中，公共 SDK 已完成的最大对象档位是 10K，
单笔点轴到 100K；100K 对象 fixture 曾在 codec inspect 以 SemanticError 7
（kLimitExceeded）拒绝；1M 对象阶段有进程族 RSS 上限结果。不得称它们已通过。
单笔 1M 点被拒与文档十笔合计 1M 点是不同问题，后者已有 ON/OFF 工程观察，
仍不等于 1M 对象或正式资格。

后续先核对当前 codec／snapshot／Runtime admission 和预期产品装载范围，
由所属 Semantic／release authority 决定任何限额／兼容性修改。保留 rejection，
不把调大 limit 或绕过 SnapshotCodec 当成 Render 优化。
在被允许的装载范围内，分别量化 fixture/restore、Scene 发布与 metadata 复制、
chunk/source 查询、首帧、导航／编辑和 cache/RSS；只修复测量中主导的现有 owner。

### 4. 最后才做正式规模与平台验收

按对象数和文档总点数两个轴推进 1K → 10K → 100K → 1M，单列稀疏／高重叠、
编辑后 masks、可见点数、DPR、资源体积、预算和 peak RSS。进程族 cap／timeout
及 admission rejection 必须保留；component/index 结果不能代替 root SDK 完整渲染。
性能 profile／阈值和正式 Gate 前置需由控制面另行冻结；本文件不自动授权 P32。
Windows／Android／Apple 真机、ABI／packaging 和物理输入证据仍独立验收。

## 本轮交付与后续入口

本轮只交付 main 集成、验证记录和上述顺序调整。现有单元／像素／receipt 回归为
继续工作的底线，旧原生失败按名称比较，不通过修改 fixture 或 lock 隐藏。
下一次启动建议从“当前版本的编辑后导航与冷恢复小矩阵”开始；若保持当前可用
体验且无新退化，可以继续产品开发，把长尾和规模任务留为明确 backlog。
本轮没有要求立即实现更多优化，也不宣称 G5/P34 PASS。

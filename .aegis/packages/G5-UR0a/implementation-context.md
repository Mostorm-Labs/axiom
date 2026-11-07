# G5 UR0a implementation context

用户授权本包，仅完成观测协议与静态基线定义。控制会话负责设计/复核，实现会话负责交付；不自动进入UR0b。

受控工作树为 /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom，分支 codex/geometry-navigation-integration；不要使用会话的ambient cwd。仓库 GitHub Mostorm-Labs/axiom。

先读package.json，检查仓库、内容hash和anchor祖先关系；若HEAD是有效后继则检查增量并保留，不因HEAD前进直接拒绝。已有三份设计修订2文件为控制面输入，当前未提交，保留不改。

状态owner和主链仍为Runtime Interaction/Ink → OperationEngine → Scene → Render → Provider，SDK只作现有传输与验证。本包API影响为零。上一轮修复已在a2d99bfd归档，不重新实现拖动修复。

目标交付目录 docs/planning/g5-unified-render/ur0a/：protocol.md、metrics.schema.json、routes.json、matrix.json、review.md；可在此目录加入轻量校验与schema样例。审查结果目录 .aegis/results/G5-UR0a/ 存哈希清单、校验输出和完成报告。

静态双向追踪操作入口到coverage leaf及Raster/GPU分配到调用者。旧cumulative_erase会等待quiescence，旧drag以RAF控制发点；不能把旧统计当作立即再输入证据。当前partial erase feedback没有现成Brush ARC token；跨层计时必须区分commit、Scene与present。

UR0a不跑正式性能矩阵、不增加生产埋点，也不修改调度/预算。需要的埋点和oracle缺口列入UR0b待办；未知动态路径标MISSING。确定具体可执行测量口径和矩阵配置，不填写虚构测量结果。

完成后在本实现会话返回READY_FOR_CONTROL_REVIEW，列出交付路径、SHA256、起止HEAD、差异、校验结果、未覆盖项与UR0b前置。不自动发消息给其他会话；控制会话会读取结果。

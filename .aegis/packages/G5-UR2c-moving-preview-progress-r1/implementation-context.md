本包 G5-UR2c-moving-preview-progress-r1 是已验收 R4 之后的新性能缺陷修复，起点 a990f280d8a08c5399146f80544299e1dfebe192，工作树 /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom，分支 codex/geometry-navigation-integration。R4 结论和旧证据不变。

现状：公开 PointerInput → CanvasEditingRuntime::eraserSample 保存 trace/dirty → RuntimeCompositionCore::pumpFrames → SkiaRenderer::renderTransientOverlay → SkiaRenderCache::begin / prepareWorkingSet → 唯一 provider 发布。Operation 仍只在 Up 走原 owner 原子提交。

已确认的机制：skia_render_cache.cpp:104 在 stagedBodies_ 且新的 erase damage.bounds 存在时 resetPreparation；:848 清 pending/composite/cursor。每次 Move 都可能丢弃有效像素作业，:353 的分段准备始终未达到完成，renderer:893 返回 Retry，旧可见画面安全地保留但不跟手。同量需求的 staged latch 也可能延续到下一次手势。prepared geometry 命中不等于像素作业被保留。

控制诊断使用不重编 R2 SDK 与默认32/96MiB：19条重建线、DPR2连续更新；同文档DPR4持续Move后两手势均零可见内容写入。qualification214次transient尝试4次成功，GPU4878 tiles、CPU0；停Move仍按下约231/225ms后才各更新。原视频DPR/原始快照未取到，不声称重建就是原文档。原生强同步Metal是负对照，不能替代异步Web RED。

首先读取 execution-contract/progress-policy/inputs 与设计预检，完成共同测试驱动和原生结构测试的 RED，并用一次老正常SDK窗口固化19对象初始snapshot。不要在此之前改生产。RED之后，在现有Render owner选择最小正确实现：复用本体/历史mask/仍有效的工作，也可使用完整身份校验的局部damage/bootstrap替代。不能简单删除reset、扩大预算或清空/覆盖整个provider来假装反馈。

本轮150ms是限定场景“不再饿死”的阻断上限；p95 33.4ms继续分别报告为阶段目标，不签整体性能PASS。计时pass不读像素/不等待显示；normal只能观察target写入机会，必须与qualification中的消费prefix身份及独立活动像素验证联合，不给normal加私有API。活动像素另用固定prefix/ROI与旧独立reference验证。新的明确验收在verification.lock MP01..08，冻结后不得临时追加。

保留原子发布、失败真实像素逐字节不变、累计damage、对象顺序/透明度、全部generation/identity与CanonicalVisible。部分擦除与几何/语义Authority不变。旧GPU lease、R4 reader/oracle、normal SDK封装不可破坏。

仅在新任务namespace构建和归档。旧4297文档和server不操作。新正常Demo可用4298..4308第一个空闲端口，HTTP hash验证即可，不额外开smoke浏览器。four-ID ledger是新授权，不能沿用旧窗口或默认重试。满足全包后通知控制线程，或遇明确terminal blocker即归档/cleanup后通知；不要相互等待中间设计批准。

注意旧 transient_display_retry.mjs 的 worker metadata 固定了 d9bd…主体，source identity 也不排除本任务新结果目录。N2不能直接把这些历史值当作candidate绑定。web-plan已授权新moving_erase_regression.mjs只适配身份/新输出/自身目录排除，旧16用例、consumer、时序、oracle、AA及readback逻辑保持字节不变；旧文件本身受保护。

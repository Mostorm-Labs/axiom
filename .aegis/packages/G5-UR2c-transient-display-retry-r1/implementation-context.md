本轮修复局部擦除反馈在资源不足/Retry 时的可见目标污染和连续状态丢失。基线为 Mostorm-Labs/axiom 的 codex/geometry-navigation-integration，4d3c6c20 已发布，所有既有 UR2a/b/c 工作保留。UR2d 精确查询优化尚未进入实现，本轮优先修这个正确性问题。

阅读 package.json -> 本文 -> execution-contract.json，按需再读 authority/verification/evidence lock 和 web-plan。所有引用均以 SHA256 固定，不要重跑旧窗口或重定义旧 Gate。

当前实际链路：CanvasEditingRuntime 的局部擦除反馈使用 activeSurfaceProvider 和主 SkiaRenderer。renderTransientOverlay 在开始 move 出 partialEraseFrame_；SkiaRenderCache::drawTransientScene 先在取得的实际 canvas 绘制背景，然后资源不足才返回 false/Retry。Web provider.release 仅清 acquired；SkiaGpuLifetime 析构仍负责安全 flush/submit 已记录 GPU 工作。GPU 生命周期不能为了隐藏白屏被删除。

独立诊断已在真实 Metal 目标上观察到：先呈现有效画面，再发生低预算绘制失败，present 计数不变而2946个暗像素变为0；另一个档位只留下部分内容。高预算正向对照正常。录屏在22.475～23.225秒内容区近乎空白，工具栏和对象数仍在。没有当时资源 trace，不要断言一定触及96MiB或GPU context loss。

第一步在精确基线记录设计与旧RED：必须通过实际 renderer 的失败返回路径，不能只使用已有 helper/counter 测试。保住最后成功像素与保持资源安全是同一修复的两条独立要求。将准备尝试状态和已发布显示状态区分；失败不能推进显示证明或丢失待发布damage。仅移回 shared_ptr 不足以修复 frameSequence、generationID 或被改写内容。

实现留在现有共享 Render owner。可以选用有界准备/提交或等价经验证方案；所有资源计费，保持32/96MiB和8MiB/512。不要增加第二套 renderer/geometry/文档 owner，不改精确命中或输入/Operation/History语义，不削弱receipt。健康资源拒绝必须保留画面；真正surface/context丢失应明确unavailable、失效旧证明并恢复，不能伪造旧像素可用。

全部 DR01..06 与 web-plan 是本轮完整闭包。实现设计前置检查解决后直接继续，不再等待一次常规批准。仅两次新浏览器worker授权：Q1最多360s，N1最多600s，累计历史84次/5139s；3GiB进程族RSS guard不变。所有旧ID已关闭。不要启动额外smoke。原native RF01 line88 pointer失败保持原样，其余原绿测试与精确634+6保持。

正常最终demo使用新端口4296（冲突则下一个空闲），保留4295，不覆盖用户文档。最终准备可复核source/native/SDK/raw归档及完整结果，返回READY_FOR_CONTROL_REVIEW。任一明确终态也必须主动通知控制线程01a1b6f3-e69a-7fe2-8d9b-0222e8741f03 host local，包含精确SHA、测试、账本和余项。不要只在你自己的会话里结束，让控制线程空等。此任务不提交/推送，不签发官方P34或性能PASS。

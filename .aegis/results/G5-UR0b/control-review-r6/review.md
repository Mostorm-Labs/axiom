# r6 独立复核与采集起点缺口

维持 `BLOCKED_EVIDENCE`，r6 保持 CLOSED。独立校验162份 payload、17项结果引用并重编译原始数据：7条 accepted = 1条已证初始化 + 5条正式提交 + 1条未解析（work1/seq22）；四项像素比较通过不能补足其 native 绘制链。35次/1276秒账本与18个进程退出均核实；本轮未启动浏览器。

源码显示 correctness 的 Runtime 创建早于 collector 重置/开启，首帧异步完成可以跨过该边界。原 work1 的绘制数据没有录到，不能从当前画面或 generation0 反推补造。归类为 Verification 的启动窗口设计缺口，当前不足以认定 Runtime/GPU 缺陷。

下一任务先定义并实现 correctness 专用的初始化排空边界：实际首帧 accepted/onFrame 与 SDK 帧队列收敛后才重置/开启观察；OFF、ON 对称，保留原阶段时限、正式性能路径和严格 native 校验。先做确定性的离线 RED/回归，复核后再明确新采样窗口。精确授权见 repair-request.json；实现会话需在完成或阻断时主动通知控制会话。

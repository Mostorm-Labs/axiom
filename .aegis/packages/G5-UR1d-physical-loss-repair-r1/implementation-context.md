# UR1d physical-loss P36 handoff

继续原 UR1d；控制面已独立复核 56 native、55 Node、26 Python、120 boundaries、40 cold pixels 与8 roundtrips，唯一冻结要求失败为 U1D-PHYSICAL-LOSS-01。旧失败及窗口保留，UR2不启动。

现有 WebGlSurfaceProvider 是真实context及其completion-state的owner。service先查询attributes，锁定Emscripten对有效但lost/attributes=null同样返回INVALID_TARGET，故误入unknown-abandon。isContextLost对已删除handle也返回true，不能简单前移。按锁定API建立独立的live-registry事实，再区分真实丢失与未知弃用；不改变提交/配额/receipt owner，不增加公开API。

先核对四个JSON契约、旧control review与失败原始行，再在允许路径完成最小修复和有意义的回归。新两ID薄driver只改任务身份/输出/账本；可增加recoveryWorker私有identityRunner参数以及qualification assemble的新精确task root，原fault/consumer/pixel/queue/lifecycle predicates保持。先构建最终匹配的native/normal/private-qualification产物，冻结source/driver/artifacts后才启动两个240秒串行窗口。正常消费者保持诊断关闭；旧4288保留，新Demo>=4289。

旧Core证据只作原e4主体证据，经精确delta和相关回归评估复用，不伪称新主体运行。不增加Core/性能/采样矩阵；必要扩大scope先回传control。原物理loss失败由新Recovery Q/N pair复验，12冷图沿用独立CPU/null-cache参考及AA规则。

完成或遇阻必须主动通知控制线程01a1b6f3-e69a-7fe2-8d9b-0222e8741f03，附精确结果/源/清单hash、测试、两ID账本及清理、剩余项。无需等用户中转。实现返回READY_FOR_CONTROL_REVIEW，不能自签PASS；不提交/推送修复，等待独立复核后明确发布指令。

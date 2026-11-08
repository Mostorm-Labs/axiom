# Trace 生命周期修复独立审查

接受本次离线修复，以及原 r5 `lower_z_translucent_drag` 正确性录制的精确复用。此结论不改写原 r5 的 REJECTED/CLOSED，也不签发整体性能、平台或 P34 PASS。

- 独立回归：119 项 Node、76 项 Python、120 项边界检查通过。
- 194 份 payload、包／源码／输入引用已核验；17 份旧记录的 canonical、decoded、metrics、external、compiler provenance 字节保持一致。
- 控制面独立重编译实际 r5 原始数据，结果与实现面相同：6 条 accepted = 1 条初始化 + 5 条正式提交，无未解析项。没有删除记录，五条正式链及原四个像素比较、两个 subject proof 均未改变。
- 初始化先按已锁定 worker 协议确定最终 ON 页面的 trace 生命周期，再在其内部唯一匹配公开状态及完整 native 身份；跨页面数值相同不再造成虚假的候选歧义。未知或矛盾来源仍拒绝。
- 该记录为 correctness-only / NATIVE_ONLY，没有 measurement；metrics=null／schema MISSING 在这里为不适用，不伪造性能数据，也不把离线派生标作 fresh。

已通知实现会话准备并连续执行唯一 r6 窗口：复用精确 17 份前置与这 1 个正确性记录，仅新增剩余 5 项正确性及 30 项主矩阵。原 34 次／1264 秒保留；新 nominal 5850 秒，合计 7114 秒，仍受原 8370 秒总限、串行、3GiB 进程族 RSS 和原阶段超时约束。首个硬失败即关闭清理并回传，不自行重试。

精确决定见 `result.json`、`reuse-acceptance.json`；新执行授权见 `continuation-request.json`、`remaining-run-plan.json`。

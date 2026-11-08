# Correctness harness 独立审查

接受离线 harness 修复、单项历史测试修复，以及当前 compiler 对精确 17 份旧记录的复用适用性。完整 UR0b 仍待新的真实浏览器正确性及主矩阵证据，尚无性能或官方 P34 PASS。

- 独立重跑：114 项 Node、61 项 Python、120 项边界检查通过。Python 包含两次获准的畸形 snapshot 原生单元检查；没有浏览器采样、构建或有效 snapshot qualification probe。
- 核验 220 个归档文件，以及 package/result/source/profile。P36 只改变历史测试文件；harness、compiler、reader 和旧记录保持不变。
- 独立逐项检查 17 份原始录制及其派生结果：原始输入哈希、canonical chain、decoded、metrics、external statistics、非 compiler provenance 汇总字段均与已接受基线一致；当前严格 reader 检查通过。
- 历史复用正例使用固定 df214581 提交中的八个 compiler 文件，逐一校验旧 SHA；实际当前 compiler 仍拒绝旧 r4 packet。新独立复用许可见 `reuse-acceptance.json`，并未改写旧许可或把旧录制标成新运行。
- HUD 排除遵守冻结的公开 bounds／固定 fixture 规则，保留独立 CPU AA oracle 和像素阈值；原 OFF 失败与 timeout 不改写。离线诊断不能追认旧浏览器 worker 或完整 OFF/ON 配对通过。

已向实现会话下发 `continuation-request.json`：新 r5 窗口最多 36 次，先六项真实正确性验证，再按原条件执行三十项主矩阵。历史 33 次／1251 秒保留；新任务 nominal 5940 秒，合计 7191 秒，仍受原 8370 秒总限、串行执行、3GiB 进程族 RSS 和原阶段超时约束。任何硬失败关闭窗口并回传，不能自动重试。

实现会话需主动通知本控制会话完成或阻断终态，无需用户转述。精确决定、输入与执行边界分别见 `result.json`、`reuse-acceptance.json`、`remaining-run-plan.json` 和 `continuation-request.json`。

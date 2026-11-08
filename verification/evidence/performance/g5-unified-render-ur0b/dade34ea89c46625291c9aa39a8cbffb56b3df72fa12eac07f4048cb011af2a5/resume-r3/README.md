G5-UR0b resume-r3：FROZEN_VERIFICATION_FAILURE，返回 CONTROL_REVIEW。

已完成 fresh mechanism proof、32/96 与 64/192 MiB 精确 artifact/provenance 绑定、诊断 OFF/ON 和完整六对 AB/BA 开销资格验证。资格结果 QUALIFIED：p95 配对中位增量 0.05ms，小于 1ms 原门槛；wall 与 OFF MAD 规则通过。这只建立本 cohort 的观测开销资格，不是产品性能 PASS。

60Hz 实时输入 worker 正常结束、零丢样、零身份错误，schema VALID，但 canonical 对账为 raw accepted 2 / qualified decoded 1 / metric receipt 1。缺失的一条是首个 Operation 之前的 generation/revision 0 初始化帧（completion seq47，draw15/provider32/capture42）。当前编译链要求每条 accepted completion 对应已提交的 Scene/Operation，初始化帧没有这条观测依据；不能伪造 Operation，也不能把它静默删除以宣称通过。具体数据在 control-and-proofs/terminal-witness.json。

冻结规则已关闭整个窗口：新 worker 15 个实际运行，14 个准入通过，1 个 proof blocked，38 个 MISSING。历史 597 秒 + 本次逐 worker 向上取整 536 秒 = 1133 秒。剩余额度不授权重试。120/240Hz、六类 correctness 和30行主矩阵未执行。全部15个guard正常退出且清理完成，未触发超时、RSS或进程清理失败。

本轮只改 runner guard/accounting 与测试，未改 Runtime/SDK/Brush/collector/compiler/fixture/锁/阈值。回归：87 Node、40 Python、22 原生、120 boundary。旧结果与585个历史bundle文件逐一复核保持原样。64/192产物保留f6659200 dirty=true构建身份；32/96是f651523a dirty=false的Axiom consumer构建；二者与当前orchestration源码身份分开记录。

实现阶段停止，不提交/推送，不重开本窗口，不进入UR0c/UR1，不签发P34或性能PASS。下一步由独立控制面/Verification评审初始化帧的观测依据与准入范围，随后在明确授权下修复/离线复验；任何新采样需另行核算和授权，不能把本次失败改成成功。

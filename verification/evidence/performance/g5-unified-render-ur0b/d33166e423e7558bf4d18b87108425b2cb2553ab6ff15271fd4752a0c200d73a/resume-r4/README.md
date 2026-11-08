G5-UR0b-resume-r4：FROZEN_VERIFICATION_FAILURE，返回 CONTROL_REVIEW。

r4新runner/测试及全部机制证明完成：100Node、56Python、120boundary通过；只新增允许的两个Python源码，既有compiler/Runtime/SDK/collector/harness保持已接受字节。旧诊断、六对AB/BA和60Hz通过明确ACCEPTED_RECORDED_REUSE入口引用，原OFFLINE/source身份不重命名；没有重跑旧cohort。

新的120Hz、240Hz真实worker均通过WORKER_COMPILE、当前source/artifact、schema及完整v2 canonical证明：每项raw2=init1+committed1+unresolved0。两项guard分别40.936s、41.928s，逐项ceil扣41s、42s。

第三项lower_z_translucent_drag在冻结correctness-only30秒阶段超时，worker退出124。完整进程guard记录34.214s，ceil35s；RSS峰值1852016KiB低于3145728KiB，清理完成。OFF0/1比较已落盘，内部不一致像素分别2463/16112；但ON配对未完成，没有worker-result、最终trace或完整correctness_status，不能判PRODUCT_RED_IN_BOTH_OFF_ON，也不能通过正确性。

runner原始停止文案为“new worker requires fresh WORKER_COMPILE”，因为阶段超时发生在worker-result/trace/compiler输出前。原文和账本保留；实际原因由原guard退出124及phase-timeout记录证明，不是旧证据被当fresh拒绝。现有stage只定位到OFF两步完成后、ON产物出现前，尚不能确定精确pending await。底层超时与像素差异根因留待控制面/Verification/harness复核，不在此包中改deadline/AA/oracle或重跑。

窗口CLOSED：38计划中3个启动、2个准入通过、1个guard停止、35个MISSING。历史30次1133s + 本次118s = 累计33次1251s。全部3个guard清理完成，独立PID复查无残留。主矩阵30行完全未启动；剩余额度不授权重试。全部旧包/窗口/五份结果及1783个历史bundle文件不变。

当前revision仍f651523a并有受控dirty；未提交、推送、重建或进入UR0c/UR1；没有产品/性能/P34或整体UR0bPASS。后续需要独立审查正确性worker的阶段与配对生命周期、保留OFF像素证据，并另行决定修复及采样授权。

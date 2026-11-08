# UR0b r3 独立控制审查

结论：接受本次有界执行、停止及归档；保留六对观测开销资格验证结果。完整 UR0b 仍为 BLOCKED_EVIDENCE。主阻塞分类为 VERIFICATION_DESIGN_DEFECT，转 Verification owner 明确初始化帧的准入范围；本审查不批准修改编译器或恢复采样。

## 冻结要求审计

- RESUME-IDENTITY：受控仓库／分支、祖先关系、原包 SHA 正确；当前 full source dade34… 与证据内 27,603 项记录一致。源码改动只有 runner 的 absent-guard 扣账和非零退出拒绝，以及对应测试。Runtime、SDK、collector、compiler、fixture、锁和阈值未改。
- RESUME-BUDGET：53 项均有结果或 MISSING；新启动 15 项，逐项 ceil(guard.elapsed_s) 为 536 秒，加旧 597 为 1133 秒。无 RSS／timeout／cleanup 失败，窗口 CLOSED。38 项 MISSING 保留，余时不是重试授权。
- RESUME-PROOF：两档各 54 个 SDK artifact 与 fresh proof、bundle、实际 SDK 路径完全一致。64/192 保留 f6659200 dirty build identity；32/96 为 f651523a 的 Axiom consumer build。保留原 BuildInfo，构建输入 1079 项无变化，无依赖重建。
- RESUME-QUALIFY：诊断 OFF/ON 与完整六对 AB/BA 通过现有准入。另由审查脚本从原始 planned/handler 时间独立重算 nearest-rank p95、配对 median 和 MAD，得到 p95 增量 0.05ms（界限 1ms），wall 增量 -91.85ms（界限 204.5475ms），无 NOISY／critical loss。这只证明观测开销。
- RESUME-CLOSURE：未完成。realtime-60 执行成功后在 proof 准入失败；realtime120/240、六类 Web correctness 和 30 主矩阵未运行。不得把本次局部资格结果用于产品性能或全路径正确性结论。

## 冻结证据审计

新 bundle 780 个文件、原三份 bundle 585 个文件逐项校验均无变化。218 项 mechanism refs 全部匹配。审查以同一已绑定 compiler 将 15 份原始数据重新输出到独立目录，canonical 结果、完整 metrics、common timings、artifact、terminal、snapshot 摘要逐项等于原归档。新运行中的 7 份 ON 诊断／资格记录仍合格；realtime-60 稳定复现 raw2 / decoded1 / receipt1。

本审查新跑 87 Node、40 Python、120 边界检查，通过。原有 22 项 native 运行记录及 executable 身份已通过 hash-bound mechanism proof 核对；审查未再运行 native 或浏览器。报告中的 regression.new_browser_workers=0 是采样前机制检查的范围，整次执行的新 browser worker 数为 15，不是零。

## 初始化帧事实与缺陷归属

审查直接读取原始 seq15/32/42/47，独立验证 draw/provider 的 job/basis、capture/work/frame 以及相同完整 owner；accepted 初始化 completion 在 343800000ns，首个真实 Operation 记录在 30845100000ns。前者的 semantic/Scene generation、Scene revision 和 commit ordinal 为 0。外层公开状态在测量前也是 objects=0/generation=0，测量后为 objects=1/generation=1。初始化帧真实存在，不能伪造 Operation 或删去该记录。

当前 Runtime 的 captureCanonicalWork / receivePlatformPresented 使用同一个 completion seam 处理初始化与已提交文档帧。decoder 将缺少已观测 commit→Scene 的前者记为 MISSING_SETUP_SCENE_OBSERVATION，这是保守且符合当前实现的结果。随后 generation1 的 Operation→Scene→draw→provider→capture→completion 链完整。

冲突位于验证适用范围：原 protocol.md:50 的 qualified canonical completion 是与一次 operation/commit 需求匹配的完成；canonical_chain.mjs:6、:31–32 却以所有 raw accepted completion 为分母，强制与 mutation-qualified decoded/metric receipt 相等。冻结规则尚未定义无 Operation 的合法初始化帧如何获得初始状态依据、如何与交互 receipt 分列而仍覆盖全部 raw。现有测试均以已提交 Scene 建模，没有初始化＋首笔提交的正例。

因此主因分类为 VERIFICATION_DESIGN_DEFECT（P20）。本轮 RESUME-CLOSURE 的失败属于已冻结的准入失败，不是新增产品缺陷或后加性能门槛。现有信息不足以授权实现会话选择新的初始化准入真值；也不应继续要求初始化帧具备本来不存在的 Operation。

## 路由与下一步范围

将缺陷交 Aegis/Verification owner：明确 completion 总体的分类和初始化依据、mutation receipt 的指标范围，以及原始总数的完整对账方式。generation=0 单独不能作为通行条件；初始状态及 owner/document/view/lifetime/surface/metrics/resource/frame 的实际依据仍需成立。不能伪造 Operation、把未知当初始化或删除 raw。

先用这 15 份存档离线验证更正后的规则，保留已通过的六对开销样本和原记录。若仅更正离线 compiler，则由 Verification owner 冻结与原 artifact/collector 相同情况下的复用条件，避免重跑已经合格的完整 cohort。之后再由 Implementation owner 准备剩余验证的明确续行窗口。当前包、旧窗口与所有终态结果保持关闭；本审查无 compiler 修复或新浏览器采样授权。

这份报告只完成控制审查与缺陷分类，不签发官方 P34/G5/产品性能 PASS。

G5-UR0b-init-repair-r1：READY_FOR_CONTROL_REVIEW（完整离线修复切片）。

新P31包来自已批准的 Current 初始化验证设计。真实RT60旧行为先得到MISSING的RED，完整原生初始化链和公开init.state均已在断言中确认；不是缺字段/导入失败。新v2 proof完整划分每一条accepted raw：初始化、已提交、未证明。初始化由固定fresh-empty harness/source、实际输入字节哈希、公开初始状态及完整原生owner/draw/provider/capture/completion共同证明；generation0或operation=null单独不足以放行。document epoch取公开target.runtimeEpoch，不使用lastCommit.runtimeEpoch哨兵。

初始化另列lifecycle，不伪造Operation/Scene，也不产生mutation receipt或Up→Canonical。已提交链、全部准备attempt、外来/过时/歧义校验保持；positive gate仍要求真实committed_count>0。v1读者保留原严格含义，v2实际CLI/Python读者一致，OFFLINE不能冒充fresh，旧授权/窗口仍关闭。

最终回归100Node、44Python、120boundary通过，16组冻结oracle已映射并满足。15份原始录制离线复编译：10条accepted完成记录=1条初始化+9条已提交+0未证明；不是10次语义mutation。所有原metrics.json和external metrics字节完全相同，提交receipt/链、全部attempt、快照/终态/产品结果保持。RT60的新对账2=1+1+0，mutation receipt仍1。前一离线轮次及额外歧义RED/GREEN保留，没有覆盖原记录。

六对既有overhead样本/统计/资格结果不变；复用评估为ELIGIBLE_PENDING_INDEPENDENT_CONTROL_REVIEW，只有控制面可以确认未来适用性。离线复编译不是新采样，本轮browser worker/build均0，采样扣账仍1133秒，旧r3窗口保持CLOSED。Runtime/SDK/collector/harness/metric schema/阈值/锁均未改；既有22项CTest证据及二档SDK artifact精确哈希复用，无native/SDK重建。

本切片不等于完整UR0b或P34/性能PASS。120/240Hz、六类Web correctness和30项主矩阵仍是旧采样任务的未执行项；本包不授权它们，不自动重跑，不提交/推送，不进入UR0c/UR1。下一步是独立控制面审查离线修复、全部raw分区和既有cohort复用适用性。

Bundle: /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/evidence/performance/g5-unified-render-ur0b/2ca47a17ae94034a574379e32ae118b95b5f7c35fee9706be91dc21c46477ebb/init-repair-r1

P36-r2：READY_FOR_CONTROL_REVIEW（离线 R4 修复切片）。

成功 attempt 现在由唯一 accepted provider submit 的实际 job/basis 与完整 owner、frame、Scene、camera、surface、metrics 关联。此前的全部准备尝试保留；缺少真实返回类型的尝试继续 MISSING，不推断 Pending。
新增 canonical-chain 对账与字节/编译器绑定。采样准入要求正向 canonical 链；仅 schema 合法、零丢样或旧 QUALIFIED 标签不能放行。OFFLINE_REVERIFICATION 不能提升为新采样。沿用已关闭的授权时，更换输出目录也会在创建 ledger/worker 前拒绝。

离线复验 5 份原始记录：3 份 ON 内的 4 条 accepted completion 与解码链及指标 receipt 全部一致；OFF 保持 NOT_OBSERVED。原始时序、终态、快照 SHA、产品错误和 SDK artifact 字节均未改变。旧失败 bundle 及结果保持原样。
新回归：87 Node、32 Python、120 边界检查通过。C++/Runtime/SDK/锁未改，未重建 SDK；此前 22 项原生结果仅作为未变化的既有证据保留。

本轮没有新浏览器 worker、资格组或主矩阵，没有提交/推送，没有签发性能或 P34 PASS。完整 UR0b 仍待控制面复核并明确后续采样授权，随后完成原冻结的 qualification、realtime、六类 Web correctness 与主矩阵。不能把本修复切片的 READY 状态视作整体 UR0b 完成。

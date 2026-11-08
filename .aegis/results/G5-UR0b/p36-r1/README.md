P36-r1 返回状态：BLOCKED_EVIDENCE，需独立控制面审查。

R1 预算中立 sidecar、R2 外来 owner 拒绝、R3 无损 lease 压缩、冷 snapshot/trace 传输及 RSS 归因已实现并有机制证据。
两次新诊断均完成，没有 RSS/超时硬停止，ON 无普通或关键丢样。此处只证明该诊断，不代表性能或完整观测 PASS。

新阻断：同一 canonical frame 有 24 次准备尝试，只有 1 个成功 provider submit，且 Runtime 已接受 completion；当前解码器因要求唯一 render frame 记录而丢失两条真实 completion。
六对资格验证曾提前启动：完成 3 个 worker，第 4 个在发现缺口后由控制面 SIGINT 停止，其余 8 个 MISSING。全部观察到的工作进程已清理。无 RSS/超时原因被伪造，部分资格结果不得作为开销或主要时序结论。
30 个主矩阵、3 个 realtime、6 个联合 Web correctness worker 全部未准入。独立 CPU 无缓存像素 oracle 与六类 driver 的代码已接入，但联合 Web correctness 尚未验证。

源码是 HEAD 加未提交改动，见 source/full-source-identity.json、tracked.patch 与 untracked-source.tar.gz；不要只以 HEAD 作为本实现身份。
旧结果与旧 bundle 均保留。没有修改产品策略、预算、semantic/Skia lock、公开 API 或旧 P34；没有提交、推送或发消息。下一步应修成功 attempt 的精确关联与采样准入门，再由控制面决定是否授权新的有界资格组。无 G5、性能或 P34 PASS。

Bundle: /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/evidence/performance/g5-unified-render-ur0b/590b1565f9f69b52ed3ca8cdd19f8ca798ded09c9025917cbc47ba90b501e4f2/p36-r1

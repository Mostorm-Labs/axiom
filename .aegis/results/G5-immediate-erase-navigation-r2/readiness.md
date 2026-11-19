# G5 immediate erase navigation r2

终态：BLOCKED / PERFORMANCE_TARGET_UNMET；另有旧 B DPR4 平移 reference 像素不符。

本轮已执行冻结范围，停止新增实现和采样。候选 4348 保留供测试，4346 仍是已验收版本。

实现保留 per-contribution 身份、完整新尾段准备、真实 exact no-hit 证明、提交后完整历史等价复用、非空 exposure 当前网格进度，以及当前画面优先使用导航查询额度。OperationEngine、公开 SDK、输入语义、笔刷算法和预算未改变。

- 两轮 C 立即平移空窗：89.93 / 87.93 ms；立即捏合：86.93 / 103.13 ms。冻结目标为每轮 ≤100 ms，C2 捏合不满足。稳定后平移／捏合及收敛相对检查通过，未放宽阈值。
- 59 项最终匹配 native 回归通过；20 项 Node 检查、122 项边界检查通过；normal Web / native Render 诊断关闭构建成功。
- 真实 GPU input→arbiter→pending kernel→presentation 的 pan/pinch 各 16/16，独立完整 suffix/current-camera 像素及原轮廓 AA 边界检查通过。额外原生尾段见证分别 7488 / 9909 像素，稳定 pinch 24/24；旧完成 token 不清除新手势，cancel 无 Operation。
- C 的 DPR2/4 共 20 读回 VALID/RESTORED，8 moving 捕获各 258 个擦除内部点通过；4 个 C terminal 原始 CPU full-body / null-cache / null-prepared / sequential mask oracle 均无非 AA 差异。Undo/Redo 对象、资源、背景语义相等。
- 独立审计旧 B 的 DPR4 pan 有 8292 个非 AA 通道差异，最大 242；其余 3 个 B terminal 通过。该差异未豁免，不将 B/C 直接 byte mismatch 自动归为本次回归。
- 共 8 次新浏览器启动、785 ceil 秒；包含一次 B1 before-primary harness tmp 竞争失败及唯一 corrective，全部 cleanup；原额度不归零。剩余时间 1915 秒不授权额外窗口。

已知证据限制：D1 的三条 staging 路径在 D2／driver guard 开发中被复用，原 D1 raw/freeze/ledger 保留但旧 staging 字节不全可复解；见 diagnostic-staging-erratum.json。最终 normal source/SDK/driver 2548 绑定检查完全一致。独立 CPU oracle 的长时限为 120s/process，已记录实际耗时；不将普通 GL/public camera proxy 称为 photon 或逐帧物理 camera 证明。

所有原始失败、corrective、reference、lossless buffers、source/SDK 哈希及恢复游标都保留。db343060 后未再 commit/push/tag，不签 P34、整体 Gate 或 performance PASS。

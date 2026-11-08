# P36-r2 独立控制面审查

结论：**接纳本次离线修复切片，未发现阻断其闭合的问题。完整 UR0b 尚未完成，采样授权仍关闭。** 本次结论不是产品性能、G5 或官方 P34 PASS。

审查对象为 `p36-r2/result.json`（SHA256 `b115d492f28bbe8e8acd9482f10db06f6f7751c52e755cf8b5dfd30069ebd536`），源树 `73a237f15ce61be697067e880eb7abfe9571892309c4cbe8dc52adac39e48721`，受控分支 `codex/geometry-navigation-integration`。HEAD 仍为 `f66592001012444cd99db968961014497c0f120f`，不包含未提交实现。

## 审查通过范围

- 成功 attempt 以实际 accepted provider submit 的 job/basis 关联，并校验完整 owner、Scene、camera、frame、surface、metrics。不会把第一条或最后一条同帧准备记录当成成功提交。
- 从原始数据独立重算五份归档。三份 ON 分别有 2/1/1 条 accepted completion，共四条；四条的 raw、decoded 和 metric receipt 全部一致。成功 job 为 1582、2498、2761、2449，每条链保留24个准备 attempt。两份 OFF 均为 NOT_OBSERVED。
- 四条是帧完成，不是四次文档 mutation；归属真实 partial-erase canonical 帧，没有冒充导航 receipt。最终状态、快照 SHA、SDK artifact、原始错误及已记录外部时间均未改变。
- actual compiler 产物与归档 metrics/canonical-chain 逐对象一致。没有真实结果的 helper 保留 MISSING，未把布尔 false 推定为 Pending；精确 commit 时间不可得的指标不以调用返回上界冒充。
- 正向 canonical 链是准入条件；原始输入、proof 和当前 compiler 有哈希绑定。OFFLINE_REVERIFICATION 不能当新采样；旧授权在创建新 ledger/worker 之前拒绝，换目录不能绕开停止状态。

## 独立验证

- 当前 bundle 83个文件全部哈希一致；原236文件及P36-r1的266文件 bundle和result未改。
- 当前源树与归档一致；相对P36-r1仅变化申报的七个离线runner/test文件，无 Runtime、SDK、构建或依赖锁改动。
- 重跑87项Node、32项Python、120项边界检查，全部通过。
- 重跑实际raw→decode→metrics→canonical-chain，保留结果见 `offline-check.json`；身份负例包含错误owner、job/basis、stamp、歧义提交及缺失receipt。
- 实际调用已消耗授权的检查，拒绝结果见 `sampling-closed-check.json`。
- 未重建SDK、未重跑未变化的C++测试，旧22项原生结果仅保留为既有证据；未启动浏览器、未采样、未修改生产代码。

## 剩余边界与下一步

没有新增本切片的阻断要求。此前的资格组仍不完整：P36-r1已完成两次诊断、三次资格worker，另一次CONTROL_STOP；不能重置或补写为完整六对。三次实时校准、六类Web正确性、三十次主矩阵仍未执行。helper结果等缺失能力只能支持有限归因。

下一步是明确新的有限续跑授权，继承旧次数/时间账、原fixture、预算、门槛和硬停止规则，再恢复资格验证。离线接纳不能直接解锁采样，也不能拼接旧失败/中断组来宣称资格通过。若续跑需改变runner的授权绑定，应重新绑定源树与机制证明，保持既有raw与结果不变。无需重做本次已通过的离线修复。

本轮仅签发离线修复接纳；未开启后续worker、未提交、未推送。

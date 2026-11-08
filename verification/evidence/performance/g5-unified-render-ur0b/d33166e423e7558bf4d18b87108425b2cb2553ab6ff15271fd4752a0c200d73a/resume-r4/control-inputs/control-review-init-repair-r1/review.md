# UR0b 初始化帧离线修复独立审查

结论：ACCEPTED_OFFLINE_INITIALIZATION_REPAIR_AND_RECORDED_COHORT_REUSE。冻结的初始化帧验证缺口已关闭，批准原诊断／六对开销样本在精确原录制条件下的显式复用。父 UR0b 尚未完成，本次不签发官方 P34／产品性能 PASS，也不恢复原 CLOSED 窗口。

## 冻结要求审计

- INIT-APPLICABILITY：初始化只在完整原生 draw/provider/capture/accepted 身份链、实际公开 init.state 和固定 fresh-empty harness/source/input 依据成立时确认。generation0、operation=null 都不足以单独放行；缺少／矛盾／非空／未知来源的初始状态仍未证明。document epoch 使用 target.runtimeEpoch，而不是 lastCommit 的无提交 sentinel。
- COMPLETE-POPULATION：v2 保留全部 accepted sequence，并完整分为 initialization、committed、unresolved；计数和实际 membership 双射。未知、重复或 foreign/stale 身份保持失败。
- MUTATION-ISOLATION：初始化生命周期单列、operation_id=null 并注明不适用；不生成假的 Operation／Scene 事件或 mutation receipt。qualified_decoded_count 继续表示提交帧，positive canonical gate 仍要求 committed_count>0。当前提交链、成功 job、全部 attempts 和 receipt 未变。v1 若被读取仍遵守旧的严格含义。
- OFFLINE-REUSE：只输出独立离线结果；录制与当前 compiler/spec 身份分开。旧 stopped authority／换输出目录拒绝和 OFFLINE→fresh 拒绝保留。本轮没有修改 Runtime／SDK／collector／harness／metric schema／锁／阈值，也没有浏览器采样或重建。

## 冻结证据审计

本审查复核新包／设计全部绑定、418 个新 bundle 文件、1365 个历史 bundle 文件以及当前 27,618 项 source identity，无不一致。相对 P31 起点只有允许的10个 source路径改变；未跟踪的 helper／初始化测试同样包含在完整 source hash 中，不能仅用 git diff 的8条路径代替此边界。

独立以当前 CLI 在审查目录重新编译全部15份冻结原始输入：共10条 accepted completion=1初始化+9提交+0未证明。实际 realtime-60 正确对账2=1+1+0，mutation receipt仍1。每份 metrics.json 和 external-metrics.json 均与原录制逐字节相同；全部提交链、native owner/frame、commits/Scene、attempts、snapshot／terminal／产品结果对应不变。7份 OFF 仍 NOT_OBSERVED，7份 ON诊断／资格记录仍只有其原提交帧。

独立重现旧源码的实际失败：原始 seq15/32/42/47 初始化链和公开空状态均完整，旧 v1 仍为 MISSING/raw2/decoded1/receipt1；新 v2 为 QUALIFIED/raw2/init1/committed1/unresolved0/receipt1。新实际 reader 接受完整 v2，初始化-only 无法通过 positive committed gate，所有离线结果均无法通过 fresh 编译准入。

本审查新跑100项 Node、44项 Python、120项边界检查，全部通过。原22项 CTest证据和11个 executable SHA 精确保留；两档SDK各54个 artifact 文件精确保留，无重建／重跑 native。本审查未启动浏览器。

## 复用决定与后续边界

既有控制面从原始 planned/handler 独立计算的六对 p95／wall／MAD，与本轮新离线资格评估完全相等；结合15份输入/common/metric字节不变、产物／collector／harness不变，接受这些原始样本的精确复用。资格仅证明该冻结观测方案开销，没有建立产品流畅度或全平台性能结论。

未来续行必须显式绑定 reuse-acceptance.json 的原录制身份与当前离线 proof，不得把旧记录改名为 fresh 或回填当前 source。保留旧 r3 FROZEN_VERIFICATION_FAILURE 结果及 CLOSED事实；初始化解释的更正只出现在新派生结果中。

剩余父任务为120/240Hz校准、六类独立Web正确性以及30行主矩阵，共38个尚未执行的worker。可从这些未完成项规划新的有界窗口；无需仅因离线compiler更正而重做已经合格的六对样本。采样累计扣账仍1133秒；剩余预算不是新执行授权。本次仅完成独立审查，不 materialize 或启动新采样任务。

# UR0b 初始化 completion 验证补充 v1

本轮只修离线验证的适用范围：把真实初始化展示帧与 Operation 提交帧分别证明，再完整对账。Runtime、Brush、Scene、Skia、SDK 及测量预算都沿用现状。

| 分类 | 必要依据 | 是否生成提交 receipt |
| --- | --- | --- |
| INITIALIZATION | 固定 fresh-empty createRuntime 初始公开状态；完整原生 owner／frame／surface；真实 draw→submit→capture→completion；观察窗口内 capture 前无对应提交 | 否 |
| COMMITTED | 原 Operation→Scene→绘制→完成的完整身份链及匹配 metric receipt | 是 |
| UNRESOLVED | 任一类别缺少必要依据、矛盾、丢样或身份不符 | 阻断准入 |

全部 raw accepted = initialization + committed + unresolved。只有 unresolved=0 且全部身份／总体规则成立才可通过。generation=0 或 operation=null 本身都不是通行条件。

实际 realtime-60 的目标对账是 2=1+1+0；提交 receipt 仍是1。初始化不得计入 Up→Canonical、不得清除后续交互的 feedback，也不得生成假的 Operation。公开 lastCommit 的全零 sentinel 只表示无提交；真正 document epoch 使用 target.runtimeEpoch 与原生依据，不能误读 lastCommit.runtimeEpoch=0。

同一 source 证明下的旧15份录制先全部离线复编译；SDK artifact／输入／时间／终态／提交 receipt 保持对应。六对 overhead 资格样本保留，离线新编译不冒充新录制。是否可用于后续续行由独立控制审查确认。

本次授权实施范围由 implementation-request.json 固定：隔壁在既有工作树生成新的 P31 离线包并完整执行；不启动浏览器、不重开原窗口。完成或阻塞后主动通知控制面。所有原始结果和旧 Gate 记录保留。

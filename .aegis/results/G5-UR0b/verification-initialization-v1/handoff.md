用户本轮明确要求“继续下一步”。控制面作为 Aegis Verification owner 已完成 P20 修正，现把离线修复工作交由你作为 Aegis Implementation owner 执行。用户既定分工是本会话做实现、控制面设计与独立审查；本消息明确授权下述范围，不恢复原采样窗口。

先读新验证设计 manifest：.aegis/verification/G5-UR0b-initialization-v1/manifest.json，SHA256 65b67b3eefd556eb66479dbc037232b4a6d8c155039264d5d21b9edf69c7c382。
Current VerificationSpec：同目录 verification-spec.json，SHA256 74f54eb0d460d1076520be9e6b941878bf8ae02d1ad3ab0615b9c79f01a2fd4e。
已批准的离线实施范围：同目录 implementation-request.json，SHA256 4f78456fd79ff23ba2075924b72d412a27bcccfb2a85f0e3b4e0052b6393547c。
该目录另有 completion-contract.json、15份实际输入的 evidence-inputs.json、16组 oracle-cases.json 和 README.md；所有引用均已校验。设计结果及验证记录在 .aegis/results/G5-UR0b/verification-initialization-v1/。

请先依 work order 在 .aegis/packages/G5-UR0b-init-repair-r1/ 材料化新的 P31 package及同目录authority/verification/closure/context/evidence绑定，所有内容严格来源于本已批准设计和实施范围。完成 repository/package/evidence preflight 后，继续整个离线实现闭包；没有冲突时不用再等一次控制面批准，也不要只做到生成package就返回。旧 r3 包不授权这些修改，所以必须用此新包，且旧包／窗口／结果保持 CLOSED 和原样。source hash因新增设计及新包自然改变，禁止扩展 source_identity 排除项。

关键冻结决定：
1. proof升级v2：全部 raw accepted = 初始化 + 已提交 + 未证明；每条seq恰好一类，未证明>0仍阻断。legacy qualified_decoded_count保留mutation-only含义，metric_receipt_count只与已提交数相等。
2. 初始依据使用 archive run.init.state、冻结 fresh-empty createRuntime recording profile/source hashes和完整原生owner/draw/provider/capture/completion。generation0或operation=null不能单独放行。初始公开 target.runtimeEpoch对应document epoch，不要拿 lastCommit.runtimeEpoch=0填native epoch。measurement.before不是必需初始锚点。
3. 初始化不生成 Operation/Scene发布事件、不进入canonical交互receipt或Up→Canonical、不确认后续feedback；positive canonical gate仍要求真实committed_count>0。
4. 保持提交链、原子身份、所有准备attempt、丢样与foreign/stale校验。把新v2读者接通repair/resume/compile等实际消费者，同时保留旧授权关闭和OFFLINE不冒充fresh的检查。
5. 首先以真实RT60输入做旧行为RED：实际完整初始化+首笔提交应对账2=1+1+0且canonical mutation receipt仍1；旧compiler的MISSING才是预期失败，不能只以缺字段/import失败交RED。然后完成所有正反例。
6. 仅离线复编译现有15份记录到新目录，核对外部/common时序、文档snapshot/terminal/product结果、所有committed receipts/metrics与旧数据保持对应。初始化lifecycle另列。保留既有六对overhead样本，输出适用性复用评估，不能把离线复验称为新采样。

允许路径、完整测试命令、冻结输入hash、结果边界和明确blocker已在work order中固定。新结果写 .aegis/results/G5-UR0b/init-repair-r1/ 及对应新source-tree evidence bundle。Runtime/SDK/collector/harness/原metrics schema/锁/阈值不改，无任何浏览器worker、build/probe或采样授权；不提交、不推送、不进入UR0c/UR1。保留当前f651523a及已接受dade34… dirty修改，按祖先关系接续。

请执行至 READY_FOR_CONTROL_REVIEW 或明确blocker。用户要求终态主动通知控制面：使用 send_message_to_thread 发到01a1b6f3-e69a-7fe2-8d9b-0222e8741f03（hostId local），附package/result/evidence路径与SHA256、revision/dirty/source hash、测试、15run对账、reuse评估、未完成项和是否产生了新采样。若工具不可用须明确通知未送达，不要仅在本会话结束后等用户转述。
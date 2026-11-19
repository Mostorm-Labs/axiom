# G5 exposure region reuse r1

隔离实现位于 out/G5-exposure-region-reuse-r1/source-C，主生产工作树没有应用这些修改。

已实现 fixed64 子块及2texel gutter、完整区域发布与显式region身份、Query callback成本边界、canonical/native共享的本体coverage、独立mask结果、相位/格式/lease和预算校验。Web sRGB目标与标量白coverage格式已收敛，整块quality任务不再以AABB重叠阻塞区域frontier。

原49对象native45/48新present；21指定相关回归通过；DPR2/4公共SDK功能通过。正式ordinary8全部执行，候选最长GL内容提交空窗约34–50ms，配对基线约1133ms；GLop p95不是显示帧p95。

终态 FROZEN_WEB_HANDOFF_OR_QUALITY_FAILURE：DPR2光学wheel input100后47.7ms才出现新相机，未满足33.4ms窗口。停止剩余2个DPR4光学starts。离线完整比较另发现DPR4稳定恢复配对中位数门槛失败：wheel约1082ms、touch约860ms，限额约558/573ms。最后input360的分析截断误报另列，不作为阻断根因。48当前画面RGBA已保存，14个完全匹配旧独立reference采样可测且与基线一致，34不可用。没有签整体Gate/P34/performance PASS。

试用 http://127.0.0.1:4340/example.html，PID44610，仍属实验版。旧4336–4339和文档保持。未commit/push。精确结果见 .aegis/results/G5-exposure-region-reuse-r1/result.json。

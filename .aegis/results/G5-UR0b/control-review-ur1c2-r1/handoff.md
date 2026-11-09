UR1c2 独立审查已完成，结论为 BLOCKED_IMPLEMENTATION；进入本范围 P36 修复，不进入 UR1d/UR2。

控制审查：.aegis/results/G5-UR0b/control-review-ur1c2-r1/result.json
SHA256 fac087d0ff3e6011046b2cc8bb909dd150cf3d2cd5ae76d355045974f34d3195

已冻结可执行修复包：.aegis/packages/G5-UR1c2-working-set-repair-r1/package.json
SHA256 0cbb259d73bebda03d35db741766d57c707d6f22493cdd4fc2a610f8fb8d1384
修复契约 SHA256 4b57bacd005f91514f444b53c1bd019eeaad71bcc64ce63f25c913b6d829cccc

请现在从受控工作树 /Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom，分支 codex/geometry-navigation-integration 开始执行。仍由你独占产品源码及 Git index 写入，不提交或推送。

保留有效工作：UR1c1 已接受；原 UR1c2 的47原生、36Node、17Python、120边界、28像素/8roundtrip均独立通过，32不同长本体 DPR4 21次调用完成且无legacy扫描。原结果、SDK、4284/4285/4286和UR2归档都不能改写。

本次失败属于原冻结 U1C2-02/04，不是新规模目标或GPU队列问题：最终有效 projectRuntimeScene 探针在默认32/96MiB、8MiB、512entry下，一长7199+512短64、长线明确在前，默认 allowance 最终260次完成但有1026body builds、14398legacy support、7199legacy compiler输入；one-step allowance8192次仍无present且1030build/1033admissions。513短在258次能完成，256次未present本身不是活锁证明。最终源码、二进制、库、命令、日志已绑定于 control-review-ur1c2-r1/working-set-probe-summary.json；早期 working-set-* 探索日志不能替代最终有效输入。编译总数不代表单append超过32，不得混同GPU→Raster回退。

请先解决现有 owner 内的可消费依赖/再准入/分段消费设计，再修代码：不能预检Ready后丢失body/alias却回到整历史support/length/Direct compile；也不能靠无预算全量pin、扩大cache cap/工作预算或缩小已冻结支持范围来绕过。需在有限工作集下真正消费已完成工作并可前进。保留共享同源、mask/transform不变、完整身份比较和Pending前不写最终target/不发假receipt。

修复包已明确原生RED/GREEN、distinct-body和equal-alias/byte压力、原47及相关回归、匹配新normal/qualification SDK和新Demo>=4287。仅在源码/驱动/产物冻结、原生验证通过后启动本次新增两串行Web行 UR1C2R1-Q1/N1（各180s、RSS3GiB），沿用原12case/28pixels/8roundtrips。新窗口从52starts/2004seconds往后累加，原窗口不重开。不得追加smoke/retry；guard/身份/环境异常即冻结剩余行并报告。

继续至 READY_FOR_CONTROL_REVIEW 或明确阻断。完成或任何冻结终态后，必须主动 send_message_to_thread 通知控制线程 01a1b6f3-e69a-7fe2-8d9b-0222e8741f03，hostId=local；带 package/result/source/delta/manifest/SDK/driver SHA、失败机制如何修复、完整测试与新窗口账本/cleanup/剩余项和新Demo。不要只在你那边结束然后等待这里；若通知失败保存工具错误并明确告知。

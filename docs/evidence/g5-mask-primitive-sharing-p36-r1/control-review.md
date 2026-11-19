本包独立审查结论：PASS_WITH_FINDINGS，仅限 G5-mask-primitive-sharing-p36-r1 的 MPR1–MPR5。此前 MPS2 查询容量切换清空缓存及新增测试目标配置两项实现缺陷已关闭，原有共享 mask 路径与 32MiB 累计绘制驻留能力保留。该结论不代表完整 G5、其他平台 ABI、发布或整体交互性能通过。

| 冻结义务 | 独立审查结果 |
| --- | --- |
| MPR1 同预算跨入口绑定 | 原 actual owner / SkiaRenderer 查询探针和新 query / strict-confirm 共4项独立重跑通过；0额度切换清空、32复用、0重建，外来预算失效与过期几何拒绝保持 |
| MPR2 测试构建准入 | 普通 native 与 Web SDK 的新测试/观测器默认关闭；仅私有 opt-in 时库和测试宏一致；Metal fixture 限定 APPLE。已核对真实 compile_commands、缓存和产物 |
| MPR3 原机制及正确性 | 已核对原40项加2项新测试的42项日志、另9项仅 opt-in 测试、Node10/Python4/122项边界检查。未重复新跑四格，精确复用父包 |
| MPR4 Web | 普通 B/C/C/B 的6条路线、24项原相对阈值独立复算通过；8对 DPR2/4 像素最大通道差1，未扩大原容差2；12对同阶段 Snapshot 完全一致。Undo/Redo 的对象/资源/背景解码与 commit/presentation receipt 通过 |
| MPR5 精确边界和保护 | 235当前直接产物、1410冻结文件、1218候选源码、4套SDK资产、28构建输入、332父包直接产物已核对；源码修复仅3文件。原42项未提交改动、空index、控制面生产代码、旧Demo及用户文档保留，preview及终态回调已到达 |

8MiB 原来是局部缓存配额，并非全局内存已耗尽。绘制 mask 累计驻留按 min(32MiB, maxEntryBytes, maxBytes/4) 按需分配；本次实验配置可用32MiB。单计划/同步构造8MiB与精确查询原有较高额度分别保留，避免扩大同步尖峰或挤占精确任务额度。同预算不同消费者额度切换现在复用既有 owner，不再清空准备结果。

四个 after-erases 截图附带的 stamp 是旧 navigation semantic0，不能称为 generation3 canonical 的完整提交身份。当前 generation3/presented3 receipt、Up后的 onFrame、实际 canonical framebuffer 的 VALID/RESTORED 读回、已变化像素及独立 native legacy oracle 提供替代覆盖。最终导航读回的 semantic3 stamp 与当前状态匹配。此处仅接受有范围的像素一致性结论，不补造 marker 关联或 photon 证明。

web-correctness 的 nativeIndependentLegacy 文本链接错误，正确路径为父包 native-factorial.json；结构化 result 与 native-regressions 已精确绑定该父包。超出本包边界的历史递归审计发现4处旧元数据指针漂移，当前直接引用与父包332项均完整匹配。两者作为勘误保留，不改写冻结证据，不追加采样。

普通浏览器对照证明不回退，尚不能证明整体缩放明显更快；旧四格约130.25→125.50ms只适用于局部 mask 核心。本次没有解决所有导航成本，热路线约1秒的内容提交代理空窗和阶段1密集非均匀/剪切 FilledPath 三角化仍需另行定位。

候选普通Demo：http://127.0.0.1:4346/example.html（PID96994，诊断关闭）。旧4342–4345保留。本包未提交、推送、打tag或合并到受保护的主工作副本；本控制面未新增浏览器采样。实现会话保持STOP，下一包需另立明确范围。

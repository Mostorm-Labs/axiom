# Web Worker 功能候选 · 2026-11-12

当前可测试入口：[4330 candidate Demo](http://127.0.0.1:4330/example.html)。本地 Build 为 `0.4.3.0`，工作树分支 `codex/g5-cpu-worker`，基线 `53cd1d6625232c2112c8db272e61fa6da2a0910b` 加未提交改动。精确源码、SDK 和证据身份由 `.aegis/results/G5-CPUW3-lifecycle-repair-r3/` 下的 `source-manifest.json`、`SDK-manifest.json`、`result.json` 及 `artifact-manifest.json` 绑定。此入口补充此前的历史开发 checkpoint；不改写冻结记录。

普通 SDK 的 `AXIOM_CPU_PENDING_ERASE` 仍默认 **OFF**；此候选包为 **ON**。Demo 默认选择 `round-handdrawn` revision 4。受控 Worker 擦除域是 CompactStroke algorithm v4 的圆笔／marker，目标变换为平移、旋转及均匀缩放。文档混入不支持的旧笔画／非受控笔刷后，整份文档的候选擦除路径会判为不适用并保留原 legacy 路径；这些路径不能称为已完成 Worker 化。

已验证 Web/WASM root SDK 的局部擦除、立即再次擦除、shape／便签／表格／图片创建及待提交选中反馈与拖动、已有对象拖动／取消、mask 随笔画移动的 A/B/C 顺序、v4 绘制及 Undo/Redo／导航、失败擦除后的无关编辑、context 恢复，以及 DataPort 包释放后的关闭重试。所有 canonical 变更仍由原 OperationEngine 提交。

这不是所有编辑命令、所有笔刷或所有平台的多线程完成声明。Windows／Android／iOS／macOS 的实际设备证据、全平台 ABI、正式 100K／1M 规模验收及 SDK 发布均不在本候选通过范围。

当前仍只有 **1 个固定 64MiB Worker**，总共 **8 个待处理意图**，消息上限 1MiB、传输额度 16MiB。抬笔可快速返回，但原测量的总收敛仍为 1600 段约 9.59 秒、7200 突发约 54.56 秒、7200 持续输入约 29.59 秒；不宣称 3／5 秒性能目标通过。后续仅对同一份输入做积压归因，分别识别重复计算、Worker 实算、传输、owner 精确确认、提交与绘制等待，不能把 Worker 区间外全部当成 CPU 耗时。

本轮终态仅为 `READY_FOR_CONTROL_REVIEW`，不签发官方 G5／P34／性能 PASS。旧 [4328](http://127.0.0.1:4328/example.html) 与 [4298](http://127.0.0.1:4298/example.html) Demo 及用户文档保持保留。本轮不提交、推送、打 tag 或发布 SDK。

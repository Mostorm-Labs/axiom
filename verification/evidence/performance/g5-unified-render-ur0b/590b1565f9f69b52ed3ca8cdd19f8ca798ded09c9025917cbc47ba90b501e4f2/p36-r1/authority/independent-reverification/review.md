# UR0b 阻断复核与 P36 续修

结论：接纳本次失败结果的归档和停止处理；不接纳 UR0b 完成或主要性能结论。允许隔壁实现会话进入 **观测实现修复与有界重新资格验证**。这仍是原 UR0b，不是 UR0c/UR1 优化。

审查对象：受控工作树 `/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`，分支 `codex/geometry-navigation-integration`，HEAD `f66592001012444cd99db968961014497c0f120f` 加未提交实现。源码树 `dc523e2768928bfa6a6a0b0f0d75e95caa876cdbe8e7d95637e3d0929933c834`。HEAD 本身不包含这些实现。

独立检查了失败 bundle 的 236 个文件 SHA256，以及源清单 27,581 个条目的当前字节；均一致。检查原始日志，确认 22 项原生、65 项 Node、20 项 Python 与 120 个边界契约检查通过；本次独立重跑 21 项 UR0 Node 测试通过，但新增的实际解码反例暴露了既有测试未覆盖的错误。未启动浏览器、未重新做性能采样、未修改生产源码。

## 复核发现

### R1：诊断改变产品预算准入 — IMPLEMENTATION_DEFECT

`runtime/render/src/gpu_resource_retirement.hpp:25–27` 将 `GpuToken` 嵌入 `Ticket`，而 `:53` 仍以 `sizeof(Ticket)+sizeof(Entry)+128` 向产品预算申请。资格构建 ON/OFF 都可能在边界余额时改变准入，不能用同二进制对照排除。对应原 `UR0b-PRIVATE`、`UR0b-REGRESSION` 的观测中立义务。

修复须保持原生产计费及分支结果，新增诊断状态准确归入独立有界诊断额度；建议使用已有 owner 的有界 sidecar。不得通过增大 Render 预算修复。补 normal、qualification-OFF、qualification-ON 在边界余额及延迟完成/abandon/loss 下的准入和退休一致性测试。

### R2：实际解码链跨 owner 错配 — IMPLEMENTATION_DEFECT

`decode_trace.mjs:15` 在找不到 owner 时退到最后 owner；`:29` 仅用 afterGeneration 找 commit；`:43–45` 未完整绑定 Scene 与 frame 的 Runtime/lifetime/attachment/resource 等身份。审查反例使 runtime99 的操作进入 runtime11 的 qualified completion，且 errors 为空。见同目录 `decoder-foreign-owner-witness.mjs` 和归档输出。该反例不证明已有单 Runtime 样本真的误配，但证明当前验证器不能可靠阻止误配。

对应原 `UR0b-COMMIT-SCENE`、`UR0b-ROUTES-TARGETS`。真实采集与 compiler 必须按可证明的完整身份关联；不得按最后一条或同 generation 猜测。缺失、歧义、过期或外来 basis 应拒绝/标 MISSING。把多 owner 同 generation、同 Runtime 不同 lifetime/attachment/resource、旧 frame/surface、乱序观察等反例接入实际 compiler，而不只是独立模型测试。

### R3：普通记录洪泛 — IMPLEMENTATION_DEFECT

首轮 ON 保留 79,208 条，普通丢样 31,860、关键丢样 0；其中 70,505 条是 resource_lease。固定 Record 约208B，普通区约74,358条。测量期间未 drain，结束后 drain 也仅推进读取游标、不回收记录高水位。仅增加 flush 不能解决。

对应原 `UR0b-WORK-COUNTERS`。允许压缩/数值聚合高频工作与 lease 生命周期，保持 job/owner/class/epoch 归属、逻辑释放与实际退休的区别、准确计数及峰值。关键 submit/complete/retire、decision/leaf、commit/Scene/frame/terminal 链不得抽样或丢失后伪称完整。全部状态（含 GPU callback 关联）仍受 16MiB 总额及 1MiB 关键保留约束。热路径不增 JSON、readback 或扫描笔画；普通丢样使依赖总量的归因 MISSING，不得直接视作产品失败。以既有记录和高频模型对账，再资格验证修复后的真实开销。

### R4：实际编译链仍未完成 — IMPLEMENTATION_DEFECT / FROZEN REQUIREMENT INCOMPLETE

`compile_run.mjs:2–11` 未贯通 UR0a 完整 schema 和动态身份验证；局部 scope ID 不等于跨尝试的逻辑需求。`realtime()` 返回值缺少 externalMetrics 必需的 origin/end/after 字段。producer 已如实列为未完成，停止采样并不能替代这些交付。

对应原 `UR0b-CLOCK-INPUT`、`UR0b-MATRIX` 等。先用已有 raw records 完成实际 compiler、需求/attempt 终态、失败/删失分母、实时返回形状。身份以实际观察为准，阶段不适用或不可获得的值保留 null/MISSING，不能为了满足自写 oracle 伪造 frame/operation。apply 返回记录的观察时间可能晚于内层 Scene 发布，须保留 upper-bound 语义，不能制造负延迟或修改产品提交顺序。六类独立 Web 正确性和实际路径证明仍须完成。

### I1：RSS 保护触发已证实，根因尚未证实

三次进程族峰值分别为 OFF 2,707,808KiB、ON 2,952,384KiB、第二 ON 3,153,392KiB。保护线 3,145,728KiB，最后一次超过约7.48MiB。它是 owned process RSS 之和，共享页可能重复计数，既不是独占物理内存，也不是 Render 的96/192MiB预算。

第二 ON 已写出 measurement.json：测量4.0337秒、quiescence已出现、pending SDK RAF为0。按 worker 顺序，它随后在最终 snapshot/checkpoint区间被停止，尚未开始 trace drain；stdout 的 measurement 是最后粗粒度stage，不能据此归因热绘制或 trace导出。

现有 watchdog 没归档逐 PID/RSS 时间线，无法确定 Node/renderer/GPU 分摊。`consumer.mjs:54–56` 把 snapshot 全部 Array.from 并保存在 captures[]，worker 再跨CDP转换Buffer，存在确定的额外复制与保留；是否主导RSS仍需验证。后续 trace pages/flatMap/join 也是额外保留风险，但不是这次已发生的原因。

允许在新的 UR0 runner 内补冷阶段标记、有界父进程 PID/RSS 归因、快照/trace 分块或流式传输；保留原 frozen bounded_worker.py。snapshot 必须仍来自相同公开 SDK，字节/SHA256及语义 oracle不变；不得通过丢 snapshot 或收窄进程族口径过关。测量热段不增加 profiler/heap snapshot/强制GC/同步GPU等待。先验证快照传输与记录保留假设，不预断产品无问题。

## 续修顺序与授权

详见 `repair-authorization.json`。先在原允许文件范围内修 R1/R2/R3，并补 R4；再做相关单测/原回归、精确构建身份和有限采集诊断。新诊断最多一对 OFF/ON，各90秒、串行、同3GiB保护；不得覆盖原失败。

通过机制验证且没有新硬停止后，允许对同一修复构建新建一组完整六对 AB/BA 资格验证，使用原门槛、原输入、原预算。旧第一对不得与新构建拼接，丢记录版本的低开销不能沿用。任何硬停止都停止相应档位，不得不断重试。

资格成立且原冻结 fixture/schema/identity/正确性条件齐备后，才按原包进入未运行的主矩阵及其余验证；可靠产品 RED 如实报告，不要求速度先变好。保持 30 个主计划身份和原失败/缺失 ledger，后续 cohort 单独归档。所有正式/诊断 worker 计入累计时间账，不重置总8370秒保护。

本轮不改 Render 策略/预算、Brush/History/Scene 语义、SDK/依赖锁，也不开始 UR0c/UR1。不自动提交或推送。返回 READY_FOR_CONTROL_REVIEW 或准确 blocker；这份复核不签发产品、性能或 P34 PASS。

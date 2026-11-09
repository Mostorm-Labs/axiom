# UR1c1 补证独立审查

**PASS_WITH_FINDINGS，仅限 UR1c1。** 原实现与本次补证合并覆盖 U1C1-01..07；旧 BLOCKED 记录保持原件。

两组资格 Web 场景均验证真实 navigation → 同 renderer service 的 retained-ink：16 次 get、16 次身份命中、0 次新增 body build/hash/deep compare。每组有 33 次真实 SDK RAF；另有 33 次 sampler RAF，被明确区分。普通版同样有 33 次 SDK RAF，prepared/execution 诊断为 0。

独立复跑 Node29/29、Python14/14、120项边界检查通过；原生43/43继续沿用未改变的产品主体。核对1197处哈希、131归档成员及903份历史Runtime/SDK/schema/lock文件，产品和既有SDK没有变化。

本次12/12新像素与4/4回环已独立离线重比；8次原oracle调用、4次完整输入一致的旧独立参考复用逐项核对。候选像素都是新捕获数据；没有扩大AA容差或使用候选差异定义排除区域。既有28像素/8回环证明保留。

冷首帧仍约0.8–0.93秒，不能称为性能问题已修复，也不能把新旧功能样本当成A/B对比。本次闭合的是验证可达性与实际共享消费者。后续按用户原授权进入UR1c2：ordinary canonical/Direct接入prepared body，保持有界准备、正确像素和receipt，另行冻结具体执行范围。

旧48次1770秒保持；补证新增2次111秒，累计50次1881秒，窗口NONE，cleanup完成。审查没有新增browser/oracle执行、没有修改生产源码或Git index，没有commit/push。

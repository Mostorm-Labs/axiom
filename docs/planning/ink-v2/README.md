# Ink v2 实施包

日期：2026-11-01。仓库：`Mostorm-Labs/axiom`。

本轮交付是实施包准备，不启动产品编码、提交、推送或发布。入口为下列三个
`package.json`，按依赖顺序执行；不能把包准备状态称为功能完成或 Gate PASS。

| 包 | 交付 | 依赖 |
| --- | --- | --- |
| [INK-V2-01](../../../.aegis/packages/INK-V2-01-v0.1/package.json) | 信号与紧凑笔画基础、圆头笔规则/手绘切换、临时激光笔 | 无 |
| [INK-V2-02](../../../.aegis/packages/INK-V2-02-v0.1/package.json) | 马克笔、粉笔、蜡笔的固定算法与紧凑纹理绘制 | INK-V2-01 |
| [INK-V2-03](../../../.aegis/packages/INK-V2-03-v0.1/package.json) | 轻量水彩与六笔型完整消费者闭环 | INK-V2-01、02 |

本次用户要求覆盖六笔型：圆头笔（规则/手绘）、马克笔、粉笔、蜡笔、激光笔、
水彩笔。激光笔自动消退且不保存；水彩第一版为半透明、纸纹、湿边、叠色。

两条新增约束是所有包的固定要求：

1. **默认模拟压力。** 当前输入不要求真实压感；保留显式真实压感模式和能力检查。
   默认模式不会因为浏览器提供了一个 `pressure` 数字就偷偷切换。
2. **纹理笔刷不再逐 dab 持久化。** 新写入保存紧凑中心线、已解析压力、版本化
   配置、种子与资源绑定；固定材质算法生成可丢弃的显示数据。空间和时间分别验收。

## 文档与授权分类

- [requirements.md](requirements.md)：来自用户的当前范围与约束，作为本包需求依据。
- [design-proposal.md](design-proposal.md)：具体技术提案；不冒称现有冻结 Semantic。
- [verification.md](verification.md)：本轮固定的验证目标、样例与阻断/佐证边界。
- 每包 `execution-contract.json`：完整关闭条件；每包局部依赖和文件由 SHA-256 绑定。

`p32_authorized: false` 表示用户本轮只要求准备实施包。将来启动实施时先完成
包绑定、仓库身份、工作区增量核对，并把技术提案中的新数据载体正式定版；之后
连续执行该包直到达到关闭条件或出现明确阻断，不在普通编码检查点反复返回审批。
当前不生成会自动触发编码的 surface handoff，也不发送给其他会话。

## 已核对的实现基础

- perfect-freehand C++ 参考已存在，参考提交为
  `176e00f2399f4969e1b0965c5921d96a3e50ce9f`；保留原有 oracle 和历史语义。
- 当前主通路为 `BrushSession -> BrushCommitAdapter -> AddStroke -> OperationEngine`。
- 旧纹理记录同时包含 `confirmed_samples` 和 `dab_output`；新写入不得复制这种展开表示。
- 当前 `BrushSample` 缺少时间事实；默认压力模拟必须从现有输入事实路径接入时间。
- 当前长笔画 append 复制并求值完整历史；新通路要用实际工作计数证明改善。
- 现有手绘两批、Table、圆角与字体工作有未提交修改，均保留；HEAD 不是完整起点。

## 交付与版本

每包交付匹配的 JS/types/WASM/resources、真实 Chrome 例子和原生证据。Windows
wrapper/ABI 声明同步，实际 DLL/设备资格与 Web 证据区分，不虚报平台通过。
使用现有锁定 Semantic/Skia SDK，不改变依赖锁或重建已发布依赖。

SDK 版本遵循 `sdk/VERSIONING.md`；算法版本、信号版本、笔画数据版本独立管理。
本包不授权创建发布标签、上传 npm、改历史 Gate 或触发自动发布。


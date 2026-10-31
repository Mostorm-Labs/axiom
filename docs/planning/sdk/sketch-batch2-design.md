# Sketch v1 第二批增量契约

授权：2026-11-01 用户委托当前 axiom codex 实现 VectorPath、Table、Section。
仓库：Mostorm-Labs/axiom；可信祖先 af5b1778284211c3834957084dd0a03e48308d69。
保留第一批未提交实现与证据，入场指纹和源文件备份位于 out/sketch-batch2/baseline.json、baseline-source。
本契约是用户已授权范围内的增量，不覆盖历史冻结 authority，不作正式 Gate 或发布判定。

## Owner 与适用性

沿用 CanvasEditingRuntime appearance session → OperationEngine → canonical / History / publication。
Scene 生成不可变 renderer-neutral 路径，Skia 共用正式/预览/导出路径；Shell 仅持有参数 UI。
SketchStyleV1 字段 0x102 新增 VectorPath(3)、Section(10)、Table(11) 适用性；版本、seed、程度生命周期不变。
Group 本身继续不绘制、不递归。Image/RichText/笔迹继续排除。

| 类型 | 外观参数归属 |
| --- | --- |
| VectorPath | 原有 FillStyle/StrokeStyle + SketchStyleV1；规范 commands 与 fill_rule 不改写。 |
| Table | SketchStyleV1 只控制共享网格程度/seed，opacity 沿原字段；颜色/线宽唯一归属既有 TableContent.grid。通用 appearance stroke/fill 明确排除 Table，网格颜色/宽度沿现有 table.setGridStyle 编辑。未扩展单元格排线；原有单元格纯色与文字 owner 保留。 |
| Section | 持久化文档边界使用新增适用的 StrokeStyle 0x101，并要求同记录有 SketchStyleV1 元数据（可 disabled）。缺省仍为既有灰色 .4/.45/.5/.65、宽度 1，显式 NoStroke 保留以隐藏文档边界。FillStyle 延续原 Section 填充并新增 pattern 适用性。标题、成员、padding/header、选框和 hover 不继承笔触。 |

读回新增 strokeEligible/strokeExcluded，与 fillEligible/fillExcluded 一起按子字段聚合 mixed；seed 不参与 mixed。
对全无适用目标的子字段 patch 明确拒绝；混合选择只对声明适用的目标修改相应子字段，整批仍是一个 PatchProperties 和一条 History。
Table 的 grid 颜色/宽度不复制到属性包，因此不引入跨 Table EditTable / PatchProperties 的隐式混合事务。
新建 VectorPath 使用普通外观默认，并始终物化可 disabled 的 v1 元数据与 Runtime seed，以版本能力约束修齐 NoFill/描边/透明度绘制；无元数据且无 pattern 的历史路径保留既有普通绘制。Table/Section 只继承新建默认中的 sketch/opacity，保留各自网格/边界缺省；创建时才落入 canonical。

## 新对象能力与读取拒绝

第一批 Shape/Connector/Sticky 的 v1 算法、随机数顺序和默认映射不变，以入场路径 fixture 的散列/坐标和实际像素回归证明。
第二批为 styleVersion=1 的新对象能力。复用现有 wire 类型与字段，不变更 proto descriptor。
SnapshotCapability 内部源代码掩码扩至 uint16，原 0..255 数值保留；新增 bit256，当前完整 profile511。
VectorPath 上的 sketch/pattern、Table sketch、Section sketch/pattern/stroke 要求 extended-sketch-v1；旧 profile255 明确拒绝。
旧 Operation reader 会因原字段/值适用性或 Section stroke 适用性拒绝，不能清掉扩展属性后成功。
Section stroke 必须有版本元数据，清除 metadata 时必须同时清除不再合法的边界 stroke。

## VectorPath 与填充区域

公开增加有界 createVectorPath/readVectorPath 值 DTO，支持 move/line/quadratic/cubic/close、nonzero/evenodd、局部平移；不增加节点编辑器或可变内部对象入口。
最多 4096 个规范命令；新能力坐标绝对值不超过 1e7，非有限、未知 op/fill rule、错误子路径顺序明确拒绝。
开放子路径轮廓不补 Close；填充沿既有 Skia 语义隐式闭合，body 保留规范 fill_rule 和所有子路径/孔洞。
曲线使用原始 Bézier 命令派生控制点，端点连续；不把扁平化折线写回规范路径。
填充输出是 renderer-neutral 的 body clip + hatch paths。既有共享 Skia 对原始 Bézier/fill_rule 精确 clip，不对曲线/孔洞建立另一套近似多边形 owner，因此没有新增曲线展开误差。
排线原始路径可以穿过 clip 外部；它们必须与 body clip 一起使用，实际填充只落入规范区域。命中同样复用规范填充区域，孔洞不被当作实心主体。
子路径轮廓、填充使用独立的固定 seed 域；程度变化只缩放同一组随机偏移，颜色/变换/相机不重抽。
退化规范路径保持零面积/零长度，不从零长度制造笔触。

## Table 与 Section 派生几何

Table 每条完整横线/竖线生成一次，实线可按既有 v1 双遍笔触绘制，但不按相邻单元格重复生成共边。
横线稳定子序列由 seed + horizontal 域 + 前一行 ID 派生，顶边用固定零身份；竖线同理使用前一列 ID，左边为固定零身份。
插入/删除不让身份和几何均未改变的线换样；行列尺寸变化用同一子序列重算。整条线连续，交点自然交叉，逻辑 cellRect 不改变。
绘制顺序：单元格底色 → 共享网格 → 单元格文字（既有矩形裁剪/内边距）→ 活动单元格辅助框。文字不会被网格覆盖。
Section 的 body/outline 以本地 boundary x/y/width/height 生成；标题仍按既有 header/padding 裁剪绘制。成员归属/裁剪不依据粗糙路径。
visual bounds 含派生控制包络与描边，选框与控制柄继续依据稳定规范几何。Section 文档边界和选中辅助边框各自绘制。

## 工作量上界、预览与证明

继承 8192 条排线、32768 输出命令、64 项/131072 命令缓存策略；过密排线按原点对齐的确定性整数 stride 降密。
Table/1 既有 64×64 / 1024 cells 上界保留；VectorPath 4096 输入上界使双遍输出和 hatch 总量有界。没有逐帧随机或墙钟降级。
begin/preview/commit/cancel 继承完整 stamp、旧值、锁定/权限/生命周期校验；预览不写 canonical 或同步，确认一次提交/Undo，取消/无变化无操作。
必需证明：孔洞/填充规则和开放曲线；共享边一次、稳定行列子序列；Section 文档边界与成员/标题不变；跨对象混合参数、seed、History、Snapshot/remote、过期/跨实例 token；匹配 Chrome 外部 SDK 与实际 Demo、native/WASM 路径对照、第一批及 Table/Section 回归。
Windows ABI/设备验证为单独证据；不以 Web pass 代替。本批不自动提交、推送或发布，SDK 保持未发布候选 0.1.0，styleVersion 仍为 1。

历史无元数据路径的蓝色缺省在 readback 中如实反馈；首次实际外观变化才物化元数据与必要填充，默认值 no-op 不产生版本记录。Section 框选保留原有完整规范矩形并纳入可见边界笔触，成员判定仍使用规范矩形。

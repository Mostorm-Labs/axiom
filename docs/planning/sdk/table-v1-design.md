# 基础表格 v1：对象与编辑契约提案

日期：2026-10-31。状态：**用户已接受的本次表格功能设计；具体实施边界见 table-v1-contract.json，不是正式 Gate 结论**。
仓库：`Mostorm-Labs/axiom`，观察基线：
`cbd62f80a46f8d30c850e98334a0e20e3019519a`。

## 角色、来源与目标

本文件负责 P10–P13 的基础表格对象、行为、语义与操作提案。
用户已要求实现基础表格：编辑文字、增删行列、调整尺寸、单元格样式；
已要求 SDK 不向外部暴露内部对象和实现。产品范围无需重新询问。

当前有效的边界约束来自根 `AGENTS.md`、`docs/notion/manifest.yaml` 指定的
对象内容/Operation 规范和 SDK 消费者契约。现有实现是证据，不能自行扩展冻结语义。
本提案不替换历史规范、修改 `.aegis` 的接受状态或将自己标为 Current Authority。

核对结果：

- `runtime/semantic/include/canvas/semantic/object_record.hpp` 当前有 10 类对象，
  包含 Section，没有 Table。
- `object_content.hpp` 没有 TableContent；现有 Shape label 是对象内嵌的 RichText，
  证明共享文字路径可扩展，但不能据此假定 Table 已受支持。
- 当前 Operation 实现到 SetDocumentBackground（19）。表格操作尚无契约或实现。
- SDK 的 `readSelection`、`readState` 和普通变换不提供行列、单元格或表格文字目标。
- 已查询连接的 Notion：Product Object Model v0.2 将 Table 定义为未来结构化对象，
  要求先确定 canonical model、cell identity、editing model；其状态为 Candidate，
  不能作为已接受 Table schema。Feature Completion Track Compass 的 Current 页面也
  没有接受 Table schema。

来源链接：

- [Product Object Model v0.2](https://app.notion.com/p/3da4c57a590c8161b8d2f39e78d63bbc)
- [Feature Completion Track Compass v0.2](https://app.notion.com/p/3f64c57a590c8169863cdcb08b400d7b)
- [ObjectContent V1 Field Table](../../notion/authority/04-semantic-schema/01-object-schema/object-content-v1-field-table-v0.1.md)

目标是一个可保存、恢复、撤销、重做、回放和导出的原生表格对象；
各平台共享语义，Web/WASM Demo 作为本机交互验证入口。

## 范围

| 首版能力 | 预期行为 |
| --- | --- |
| 创建表格 | 工具栏点击表格，选择行列数后在画板放置；默认 3×3 |
| 单元格文字 | 选择单元格，双击或 Enter 进入编辑，复用现有文字/IME 路径 |
| 增删行列 | 在当前行/列前后插入，删除当前行/列；保留其余内容与身份 |
| 尺寸 | 调整列宽、行高；支持整个表格移动与已有对象变换 |
| 样式 | 单元格填充、文字颜色/字号/粗体/水平对齐；表格网格线颜色和宽度 |
| 文档能力 | Snapshot 恢复、Operation 传输、Undo/Redo、PNG/JPEG 导出包含表格 |

首版不含合并单元格、公式、排序筛选、Excel/CSV 导入导出、自动增高、批量填充、
跨单元格富文本粘贴或新的分布式冲突合并算法。普通文字输入及中文组合输入属于首版。

## 产品对象与状态归属

推荐 **独立 Table 对象，单元格内嵌内容**。表格在文档中是一个可选择、变换、删除的叶对象；
行、列和单元格是表格内部实体，不作为文档级 ObjectRecord 或可重挂父节点的对象。
不使用 Group 中若干矩形和文字的排列来代替 Table，也不依赖 Shell 私有 JSON 保存表格。

| 数据 | 归属 |
| --- | --- |
| Table ID、Transform、Placement、可见/锁定 | 既有 canonical ObjectRecord |
| 有序行列、尺寸、单元格内容和样式、网格线 | canonical TableContent |
| 当前单元格、光标、选择区、IME、行列尺寸预览 | 既有 Interaction/Text 编辑会话的扩展 |
| 单元格布局、命中几何、文字排版、画布操作栏位置 | 派生的 Scene/Text/展示数据 |
| 工具栏 DOM、菜单、下载、网络和本地存储 | 外部 Shell / Data owner |

所有 canonical 修改继续经过既有 OperationEngine、History 和发布路径；
SDK Session 只做解码、校验与转换，不持有另一份表格文档或编辑规则。

## P12：建议的 canonical 模型

新增版本化 `TableContent/1`，包括有序 `rows`、`columns` 和稠密单元格矩阵。
行与列各有稳定的 128 位身份；每个单元格由 `(tableId, rowId, columnId)` 唯一定位。
行列索引仅是当前顺序，不作为身份。插入或删除其他行列不改变存活单元格身份。

身份由 Runtime 的现有分配路径产生，避免 Snapshot 恢复或同文档多个 Runtime 冲突；
SDK 消费者不能指定行列 ID。Undo 恢复被删行列使用原身份，普通新插入使用新身份。
行列 ID 的作用域是其表格，不与 ObjectId 混用；命令验证完整表格与行列身份。

单元格持有既有 RichTextContent 语义的文字内容及版本化单元格样式。
复用同一字体资源目录、文字布局和编辑行为；不引入浏览器专属排版/持久化格式。
段落身份继续遵循既有 RichText 约束。

建议默认列宽 120、行高 48（世界坐标单位），左对齐，字号 16，白色填充，
不透明深色文字与细网格线。文字在单元格内按列宽换行，并裁切于固定单元格区域。
不自动改变行高。样式默认值、行列数量、内容字节和几何上限须在接受契约中逐项固定；
当前数字是提案，尚未写入解析器或硬限制注册表。

必须验证：行列非空；维度数量和内容总量受限；ID 非零且同轴唯一；
矩阵与行列完全对应；尺寸为有限正数；颜色、字号、网格宽度和枚举合法；
文字与资源引用遵守现有 RichText/Font 规则；未知版本必须拒绝，不能丢弃字段降级。

新增 ObjectKind、ObjectContent protobuf 分支、Operation tag、Reader/Writer capability
均须在接受的兼容契约中分配并检查唯一性。本文件不私自占用编号或重写 descriptor lock。

## P11：交互与生命周期

1. 创建工具只在确定放置时提交一次插入；取消不产生空表格或 History。
2. 普通点击先取得表格选择，再定位活动单元格。双击或 Enter 开始文字编辑。
3. 文字编辑复用既有 TextEditSession、IME effect/fact、组合完成和提交边界。
   表格单元格成为明确的文字目标，不能伪装成另一个文档级对象 ID。
4. 非编辑态 Tab/Shift+Tab 在单元格间移动，末格不自动加行；Enter 开始编辑。
   编辑中的换行、组合键、Escape 和完成语义沿用既有文字路径；
   Escape 不回滚已正式提交的文字 Operation。
5. 行列边界拖动只改变临时尺寸；确认松手提交一次尺寸操作；取消恢复原显示。
6. 结构命令先通过既有文字完成屏障；组合未完成或目标失效时明确拒绝。
   不能通过切单元格、删行列或关窗口来合成 IME 提交。
7. 首版禁止删除最后一行或最后一列；删除整个表格使用已有对象删除命令。
8. 隐藏、锁定、关闭、远端删除表格/目标行列后，按现有会话失效规则退出，
   陈旧单元格 token 不得写入新位置或相邻单元格。

## P13：修改、撤销与回放

创建、删除、恢复、移动表格复用现有对象 Operation。建议新增类型明确的
`EditTable` 操作族表达以下互斥动作，每个 Operation 整体原子应用：

| 动作 | canonical 效果与逆操作 |
| --- | --- |
| 插入行/列 | 稳定身份、相邻位置依据、尺寸、新单元格；逆操作删除同一身份 |
| 删除行/列 | 删除指定身份及关联单元格；逆操作恢复完整前像、身份与位置 |
| 调整行高/列宽 | 按稳定身份修改尺寸；逆操作还原前值 |
| 修改单元格样式 | 对明确目标应用已校验补丁；逆操作还原实际旧值 |
| 修改文字 | 明确单元格目标及 RichTextDelta，复用文本校验与逆操作构造 |
| 修改网格样式 | 修改表格网格值；逆操作还原旧值 |

提交必须检查目标存在、类型/版本、权限与锁定、目标状态和操作适用性。
结构改变使用明确结构前置条件；文字修改绑定目标单元格及当前文字状态摘要；
属性和尺寸修改保存准确前像。具体 wire 字段与摘要域在后续兼容审查中固定。
不使用“读取整表 → 修改局部 → 无条件整表覆盖”的 SDK 写法。

重复 Operation 保留现有 Applied/AlreadyApplied/Rejected 语义；远端不回声。
同一已接受的有序操作流必须产生相同结果，不新增 CRDT 或承诺任意离线修改自动合并。
冲突、陈旧目标和失效补偿应明确拒绝并保持整个 canonical 状态不变；
不能静默覆盖对方文字、重定向到相邻格或删除后来插入的内容。
插入/删除行列及其补偿所需前像必须一起准备，继续遵守现有发布容量与提交原子性约束。

Snapshot 包含完整表格结构/文字/样式，并纳入 semantic/checkpoint digest。
带表格的文档声明必要的表格读写能力。不支持 Table 的老 SDK 明确拒绝打开/编辑该能力，
不能显示为空白对象再保存，不能仅凭支持某个 Snapshot 外壳版本就推定支持表格。
旧文档不含表格时，其既有字节、默认值、摘要与恢复行为保持兼容。

## 外部接口方向（提案，尚非已提供 API）

Web 根导出拟增加 `runtime.table` 能力：创建、只读状态、选择/编辑单元格、
行列插入/删除/尺寸、单元格样式与网格样式命令。
调用者使用表格 ID、行列 ID、公开样式值和完整的只读观察戳；
返回普通 DTO、稳定身份、有效状态及既有命令结果。
不要返回 mutable TableContent、ObjectStore、TextEditSession、Scene 或渲染对象。

读取接口需给出行列顺序、活动目标、所需单元格内容/样式、不可用原因和观察戳。
布局与编辑 token 显式绑定 Runtime、文档、表格、行列和会话代次。
128 位 ID 使用 32 位小写十六进制字符串，64 位计数使用十进制字符串。
读取结果是 owned value；命令排队执行，Promise 成功只表示应用结果，不代表持久化。

Windows 使用既有共享 Session 词汇及 C/POD ABI，不导出新内部 C++ 类型。
最终 API 名称与字段在 Runtime/平台设计阶段固定，并同步 JS、TS、C++ 包装及消费测试。

## 验证要求与移交

后续实施至少要独立观察以下结果，而非仅检查 Demo 按钮：

- 结构/文字/样式的 canonical 校验与原子拒绝；插入其他行列后仍编辑原单元格。
- 每类变更的 Undo/Redo；远端冲突时不错误补偿；Operation 重复不二次应用。
- Snapshot 恢复后表格完全一致，恢复后继续插行/编辑；不同 Runtime 身份不冲突。
- 老文档兼容；缺少能力的读写者显式拒绝；未知字段/版本不导致静默丢内容。
- 真正的 Chrome/WASM 消费者通过根导出创建/编辑；选中、移动、行列尺寸与取消。
- 文字换行、字体资源、中文组合路径；派生几何与共享渲染一致。
- PNG/JPEG 导出包含单元格与文字，移动缩放后的命中正确，无选框进入导出。
- JS/TS 声明与打包边界检查；消费者包不包含内部头文件或要求私有 deep import。

验收时以真实 Runtime、匹配的 JS/WASM 和外部消费者为依据；Web 自动化不替代
Windows ABI/真实设备及物理 IME 验收。此阶段仅有设计文档，不报告功能或测试已通过。

用户已确认按本方案实现，并要求为后续手绘线条保留兼容扩展。
本次边界与已固定决策见 `table-v1-contract.json`。Table/1 使用独立的版本化网格样式，
共享渲染消费网格几何；后续手绘效果应新增明确版本/能力，不把风格烘焙为额外对象。
本次只实现规则网格，不发布新 SDK 或宣称正式 Gate/物理设备验收。

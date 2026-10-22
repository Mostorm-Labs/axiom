# FC-02 Shape label 编辑入口

本轮承接 `28e729b2a0a2fc701d32b7f48dc1dbedb81d155b`，补齐用户已确认的 Shape 内嵌 rich-text label 方案的实际入口。

## 使用方式

1. 绘制矩形、椭圆或三角形。
2. 切到 Text 或 Select 工具，鼠标双击图形内部，进入该 Shape 的文本编辑。
3. 输入文字；Enter 结束，Ctrl+Enter 分段。F12 → Control → Text format 使用既有字体与格式接口。
4. 再次双击可编辑同一个标签；Undo/Redo 使用既有 History。

Ink 双击不进入文本编辑。Text 在空白处单击仍创建独立文本草稿。没有增加触摸双击识别承诺，真实 IME 和触摸观察单独记录。

## 状态与提交

- 普通 Shape/1（矩形、椭圆）、Shape/2（三角形）没有 label 时也能建立 Interaction 文本会话。进入、预编辑、格式预设与空会话结束不写 Object、Operation 或 History。
- 初始布局宽度使用 Shape 的局部宽度，命中按逆变换映射到 Shape 局部坐标，再判断矩形、椭圆或三角形内部。
- 首次真正确认非空文字，以已有 guarded delta3 RestoreRichTextState 原子升级同一个 Shape 到 /3 并添加 label。保持 ObjectId、Shape 几何、placement、属性和 erase masks，不创建独立 RichText 对象。
- absent-label guard 使用既有对象/文档绑定摘要并区分 label absent/present；已有 Shape/3 摘要保持原编码。旧 delta1/2 不能给无 label Shape 添加标签。
- 逆操作只允许恢复对应 legacy version 的空 absent-label 状态；三角形的 /2 状态 wire 保留必要宽度。Undo 恢复原对象及 kind_version，Redo 恢复同一个标签。错误 guard 或携带非空 label 的 legacy restore 原子拒绝。
- 会话展示保留 Shape 的几何、属性、层级与文本派生，不把 Shape 临时替换成单独文本；Shape/3 Triangle 继续产生 Triangle 派生几何。

编辑、格式、Operation 和 History 仍归 Runtime/Interaction/Semantic。平台只提供输入与物理焦点，不构造 canonical 文本或 Undo。

## Web 实际入口修复

真实 CDP 鼠标序列发现：第一次点击产生空草稿后，隐藏 textarea 跟随 caret，拦截第二次点击，导致 canvas 收不到 dblclick。原先直接向 canvas 派发 DOM dblclick 的测试无法覆盖这一失败。

输入 carrier 保留键盘/IME 焦点但不参与 pointer hit testing。结束初始草稿后恢复 canvas 焦点，再由既有 sync 把焦点交给新编辑会话。新增浏览器测试不直接派发 canvas dblclick，而用浏览器鼠标点击和原生 insertText 路径验证事件目标、实际焦点与 Runtime 结果。

## 验证边界

- 233 项选定原生 CTest、17 项 Python、9 个严格 Clang translation unit 均通过；Release 入口断言保持开启。
- Chrome：三种 Shape 的真实鼠标入口、中文输入、无额外对象、空编辑无 Operation、Undo/Redo、Select 再入和 Ink 保护通过。
- 既有 Web 文本 7 组、Minimal Runtime 13 组回归通过（合成 DOM IME/触摸，DPR=2）。
- 真实 Windows/Web 人工输入、候选窗与触摸不由自动结果代替，仍为 PHYSICAL_PENDING；历史 FAIL 不改写。

精确 source revision、产物摘要、启动进程和原始结果见 [本轮证据](fc02/evidence/shape-label-entry-20261021/summary.json)。不修改历史 Authority、Gate、Golden、PHYS 或 SDK locks，不声明 FC-02 完成。

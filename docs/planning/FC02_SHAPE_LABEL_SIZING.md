# FC-02 Shape label 输入扩展与缩放

本轮承接 `15ed9c22f3231e7504c0c3b18a237179daa3c7ac`，修复 Windows/Web 共用 Runtime 的三项 Shape label 行为。

## 行为

- 已确认文字写入 Shape 后，以当前显示宽度和真实 TextLayout 行高计算所需高度。溢出时保留宽度和底边，只向 Shape 的局部负 Y 方向延伸；未旋转图形对应画布负 Y。矩形、椭圆和三角形复用各自的内部留白区域，圆形因此可变成椭圆。
- 原生 IME 的未确认 preedit 只扩展展示，不产生 canonical Operation 或 History；取消后恢复原来的几何。实际确认后才写入文本和必要的几何变更。
- 手动拖动缩小 Shape 时，临时帧和提交帧都按新边界重新布局。保持文本字号不变，以省略号隐藏超出部分，完整文字仍保留在 canonical label。
- Text/Select 可点击溢出文本右下角的加号扩展按钮。按钮先于形状轮廓命中处理，椭圆或三角形轮廓外的按钮仍可点击；该 pointer 的 Move/Up/Cancel 由 Runtime 一致消费。
- 扩展只增加高度，不自动缩回；用户可拖动调整大小。保留已有对齐、留白、字体和选中时空文本提示行为。

## 所有权和 History

平台继续只采集输入、呈现 Runtime 返回的帧。测量、文字编辑、显示省略、扩展命中和几何提交均在既有 Runtime/TextLayout 内，没有平台侧的第二套布局。

已确认文字变更和必要的自动扩展使用既有两个 Operation/History 记录：先文本，后几何；Undo 按该顺序反向恢复。单独点击扩展按钮只产生一个几何 Operation，可 Undo。preedit 取消不产生这两类记录。

## 验证

本轮回归覆盖：

- 矩形/椭圆/三角形首次 label 及已有 label 的长文本扩展、连续追加、底边保持及 caret；
- IME preedit 的真实像素扩展与取消后恢复，canonical 几何和 Operation 数不变；
- 拖动缩小时 preview/canonical 的文字像素一致，字体和完整文本持久化保持不变；
- Select 下按钮点击、椭圆轮廓外点击、完整 pointer 终止以及 Undo；
- Chrome 实际鼠标/insertText 入口的三形状输入、缩小和按钮扩展，截图核对与完整文本读回。

自动验证使用发布且锁定的 Semantic/Skia SDK。Chrome 为 headless SwiftShader 和合成输入，不能替代物理键盘、IME、触摸及候选窗观察。本轮三项用户报告保留为历史 FAIL；修复后人工复测单独记为 PHYSICAL_PENDING。

精确结果、原始日志、构建命令、产物和人工清单见 [本轮证据](fc02/evidence/shape-label-sizing-20261022/summary.json)。

# FC-02 Shape label 再次编辑、空白结束和选中 affordance

承接产品 revision `2fbba9a8e394362918335b07b9de590c0d8855b8`。用户明确补充：再次编辑及点击定位失败发生在 Web，Windows 的再次编辑正常。该 Web 人工失败保留为 FAIL，修复后的人工复测另记。

## 原因及修复

- Web 原先在每次 canvas pointerdown 将焦点从 IME carrier 移至 canvas。carrier blur 随即请求结束编辑，早于 Runtime 处理该次文字定位。现在活动 carrier 保持焦点，canvas pointer 仍进入 Runtime；从 Debug UI 或其他控件回到画布时仍恢复画布/输入焦点。
- `EditAtPoint` 原先只查找目标对象，初始化的 selection 为零。现在在同一个 owner pump 中使用已有 TextLayout 和逆变换命中定位初始 caret，后续点击及拖选继续由 Runtime 处理。宿主不测量文本，也不自行构造 selection/delta。
- Runtime 根据当前编辑对象边界判定空白点击，消费这次 pointer 并结束会话，不额外创建草稿、对象或 Operation。已确认文字仍由既有提交路径保存。原生 composition 活动时只发出 finish effect，等待真实 native result/compositionend；不会将 Update、失焦或超时当作确认。
- 溢出加号只在 Shape 为当前选中对象或正在编辑的对象时显示。Runtime 在每帧绘制 command 中提供可见性，Scene/canonical 内容不持有选中状态；最终帧与临时帧使用相同规则。隐藏加号不命中，先选中后才可扩展。

## 验证边界

新增 Runtime 入口回归覆盖再次命中、首尾光标移动、空白结束、不产生空对象/额外 Operation、合成等待，以及加号最终/临时帧像素和隐藏命中。Chrome 使用实际页面的 CDP 鼠标/输入事件覆盖矩形、椭圆、三角形，额外合成 composition 事实验证等待真实 terminal 的流程。

Chrome 自动化不是物理 IME 或触摸观察。用户报告的 Windows 正常行为保留；新版的 Windows/Web 光标、空白完成、中文候选及加号可见性需要单独人工复测。详见 [证据](fc02/evidence/shape-label-editing-20261022/summary.json)。

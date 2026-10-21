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


## 2026-10-22 Web 再次人工失败后的修复

`e1517039` 的再次人工复测为 FAIL：用户在 Select 工具、三角形、中文输入法下输入数字，Enter 后再次双击，光标不闪，文字区域内点击反而结束。该失败独立保留，不以之前合成测试通过替代。

本轮确认并修复两条 Web 链路：

- Web 空闲时没有继续提供 owner/frame opportunity，因此 Runtime 的 500 ms 光标时钟未推进。现在仅在活动文本会话且页面可见时，每 100 ms 提供一次机会；光标可见性、时钟和重绘需求仍归 Runtime，实际只在闪烁边沿提交帧。结束、隐藏及销毁时停止定时机会。
- 旧入口只给 HTML 加 `rev`，它静态导入的 JS、WASM 和字体 data 仍使用无版本 URL。启用浏览器缓存，先加载真实旧版 editor，再部署当前文件，可以复现再次进入后内部点击结束。现在 HTML → host → editor/glue/WASM/data 传递同一个版本；未显式传 `rev` 时使用 CMake 生成的源码及 JS 指纹。启动 ready Promise 在动态导入之前公布，保持 iframe 消费者可等待的合同。

真实用户浏览器未被检查，因此不能断言缓存就是该人工 FAIL 的唯一原因。新版本需要按同样输入法、三角形及 Select 操作再次人工确认。Runtime 的文字命中、布局、合成确认和结束策略未被宿主复制或替代。

新增自动回归包括：三种形状 × DPR 1/2、Select 再次进入、中文多行及数字合成事实、光标局部截图变化、内部点击和外部结束、结束后无连续帧；另一用例开启缓存，从真实旧 JS 升级到当前版本，验证完整资产图的版本一致性。合成 CDP IME 不是本机真实 IME 验收。详见 [本轮证据](fc02/evidence/web-label-reentry-20261022/summary.json)。

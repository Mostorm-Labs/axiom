# 人工触摸验证记录

本表只记录人工观察。自动 fixture、合成 Win32 消息、测试进程创建窗口、
真实 D3D12 provider 的自动测试均不能使本表变成 PASS。
尚未收到用户对下列各项的实际观察。状态统一为 **PHYSICAL_PENDING**。
发现过的实现失败另记在诊断历史中，不能用本表的 PENDING 覆盖 FAIL。

运行前由 launch.json / machine.json / displays.json 确认 exe hash、源码 revision、
PID、设备和 DPI。运行日志由 AXIOM_MINIMAL_INPUT_LOG 指定；F12 打开 Debug UI。
反馈格式：项目 ID、输入顺序、actual、PASS/FAIL、约略时间或对应日志行。
预览跟手、视觉位置、触摸 UI 和设备切换结果必须由用户描述。

已由用户澄清 B3：保留 AutoIntent 原逻辑，250 ms 内第二指参与缩放，
超过 250 ms 第二指落笔作为第二笔书写；不修改冻结策略。
第一指已越过 Ink slop 的既有分支也保持不变。

| ID | 输入/expected | actual | 状态 |
|---|---|---|---|
| A1 | 单指画笔连续绘制；preview 跟手 | 未人工观察 | PHYSICAL_PENDING |
| A2 | 松手；canonical 与 preview 同位置 | 未人工观察 | PHYSICAL_PENDING |
| A3 | 连续 10 笔；采样不会逐笔降低 | 未人工观察 | PHYSICAL_PENDING |
| A4 | 矩形、圆形、三角形、Line、Arrow 均完成 | 未人工观察 | PHYSICAL_PENDING |
| A5 | Shape 后再画笔迹；笔迹在 Shape 上方 | 未人工观察 | PHYSICAL_PENDING |
| A6 | Arrow→Arrow、Line→Line、Shape→Shape 连续绘制 | 未人工观察 | PHYSICAL_PENDING |
| A7 | partial eraser 只擦笔迹，Shape 不变 | 未人工观察 | PHYSICAL_PENDING |
| A8 | Undo/Redo、选择、移动、缩放、对齐线和取消正常 | 未人工观察 | PHYSICAL_PENDING |
| B1 | 双指进入导航后平移；不提交 Ink | 未人工观察 | PHYSICAL_PENDING |
| B2 | 双指缩放；未承诺 rotation | 未人工观察 | PHYSICAL_PENDING |
| B3a | 第一指尚 pending，第二指在 ≤250 ms 加入；进入缩放/导航 | 未人工观察 | PHYSICAL_PENDING |
| B3b | 第二指在 >250 ms 落下；按原策略提交独立第二笔书写 | 未人工观察 | PHYSICAL_PENDING |
| B4 | 第二指快速加入；两个 Down 均保留 | 未人工观察 | PHYSICAL_PENDING |
| B5 | 一指抬起另一指继续；保留原手势所有权，结束后可重画 | 未人工观察 | PHYSICAL_PENDING |
| B6 | 导航时 capture lost、焦点/窗口切换；逐指取消且不遗留 session | 未人工观察 | PHYSICAL_PENDING |
| B7 | 导航结束后单指重新绘制 | 未人工观察 | PHYSICAL_PENDING |
| B8 | 独立 MultiInk 策略；Runtime 支持，Minimal UI 无新增策略切换入口，已有自动回归另记 | 未人工观察该策略 | PHYSICAL_PENDING |
| B9 | source/pointer/generation 复用；旧事件不影响新手势 | 未人工观察 | PHYSICAL_PENDING |
| B10 | Debug UI 上触摸；不穿透画布 | 未人工观察 | PHYSICAL_PENDING |
| C1 | Ink/Shape 控件经 RuntimeFacade 生效并读回 | 未人工观察 | PHYSICAL_PENDING |
| C2 | Brush/Marker 等预设经 RuntimeFacade 生效并读回 | 未人工观察 | PHYSICAL_PENDING |
| C3 | 颜色、线宽、透明度经 RuntimeFacade 生效并读回 | 未人工观察 | PHYSICAL_PENDING |
| C4 | Undo/Redo 经 RuntimeFacade 生效并读回 | 未人工观察 | PHYSICAL_PENDING |
| C5 | Eraser 模式经 RuntimeFacade 生效并读回 | 未人工观察 | PHYSICAL_PENDING |
| C6 | 选择、移动、缩放经 RuntimeFacade 生效并读回 | 未人工观察 | PHYSICAL_PENDING |
| C7 | viewport 状态读回与操作一致 | 未人工观察 | PHYSICAL_PENDING |
| C8 | frame/retry 状态读回与实际事实一致 | 未人工观察 | PHYSICAL_PENDING |
| C9 | provider/resource/lifecycle 状态读回与实际事实一致 | 未人工观察 | PHYSICAL_PENDING |
| C10 | F12 Debug UI 开关；UI 不直接操作 D3D12/编辑内部状态 | 未人工观察 | PHYSICAL_PENDING |
| D1 | resize 多种尺寸后继续画 | 未人工观察 | PHYSICAL_PENDING |
| D2 | maximize/restore/minimize 后 canonical/preview 保留且可重画 | 未人工观察 | PHYSICAL_PENDING |
| D3 | 多显示器/DPI 缩放下坐标正确；本机 Win32 当前报告 DISPLAY1、2560×1440、192 DPI | 未人工观察触摸坐标 | PHYSICAL_PENDING |
| D4 | preview/canonical 无偏移 | 未人工观察 | PHYSICAL_PENDING |
| D5 | 空闲画布无异常持续重绘 | 未人工观察 | PHYSICAL_PENDING |
| D6 | capture loss 后下一次绘制正常 | 未人工观察 | PHYSICAL_PENDING |
| D7 | 提交后 brush handoff 退休 | 未人工观察 | PHYSICAL_PENDING |
| D8 | surface 不可用/occluded 时 retry；canonical feedback 不丢 | 未人工观察设备场景 | PHYSICAL_PENDING |

自动测试与本表分开：47 个选定 CTest 项包含上述可自动观察的入口、身份、
坐标、取消、相机恢复、连续采样、失败 Down、facade、surface 和 frame retry 回归。
逐个机器结果以 selected-tests.xml / selected-tests.log 为准。

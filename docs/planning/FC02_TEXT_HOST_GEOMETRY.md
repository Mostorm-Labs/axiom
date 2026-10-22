# FC-02 文本几何与宿主输入接线

本轮承接 `c68a057ce3e9a80cfb203f3c88abed20b8286295`，在
`codex/fc02-runtime-boundary-design` 实现文本工具、编辑几何、Windows IMM32 和 Web 原生事件适配。
只推进本轮实现与本地验证，不宣布 FC-02 完成、独立审查、Gate 或 PHYS 验收通过。

## 已接入

TextLayout 使用同一 SkParagraph/ICU 产生 caret、selection、hit-test 和 grapheme 边界。
几何 token 绑定完整文本会话、目标、camera、布局及选择范围；resize 后旧 token 拒绝使用。
预编辑文本只进入当前帧的派生呈现，确认后的内容继续走原有 RichText2/delta2/History。
空新草稿没有 Object、Operation 或 Undo。缺字返回明确诊断，保留已确认的 Semantic 文本。

Runtime 接收 Text 工具单击创建、鼠标双击编辑、指针定位与拖动选择。
宿主只转发 UTF-16/native facts 和 typed intents，不能创建 Paragraph、delta、历史记录或另一套布局。
Enter 结束、Ctrl+Enter 换段；IME 正在消费候选按键时优先交给 IME。
切工具、受控关闭请求完成原生合成并结束编辑；Debug UI/宽度控件请求完成合成后保留编辑。
只有真正的 native result/compositionend 可以提交，Update、超时和失焦不能伪造确认。

Windows 回调排队，owner pump 处理 WM_CHAR、IMM32 result/preedit/cursor、取消与完成失败。
保留完整 session/projection 身份；连续字符允许沿自身成功提交推进，部分 result 后的继续合成
只绑定自身接受的终态。UTF-16 高代理项也绑定其 issued 会话。
候选窗口位置消费 Runtime 几何；F12、Debug UI 焦点及窗口关闭走同一完成屏障。
设置 `AXIOM_MINIMAL_INPUT_LOG` 可记录输入身份、序号、Runtime 状态和对应 frame/对象/Operation。

Web 隐藏 textarea 仅作为原生输入 carrier，不绘制或测量文档文本。composition/beforeinput
转为完整身份事件，uint64 用十进制字符串、ObjectId 用完整 128-bit hex，终态可重放去重。
actual Web Minimal Host 增加 Text 和独立宽度控件；Debug UI 仍只通过 RuntimeFacade 操作。
强制销毁或浏览器最终离页无法保证异步原生完成，不能声称自动保存成功。

## 本地结果

| 验证 | Expected | Actual / Result |
|---|---|---|
| 接线 RED | 缺少真实几何与宿主入口时失败 | 修改前 4 FAIL；修复后 4 PASS |
| FC-02 集成 | 真实字体/预编辑隔离/终态/宽度/历史/身份正确 | 24 PASS，包含于 CTest |
| Windows 消息适配 | 回调不直接写入；字符顺序、部分提交、失败及代理项正确 | 新增生产入口回归 PASS；OS 观察为 synthetic |
| 范围回归 | Brush/frame retry/composition/facade/identity/capture/resize/TextLayout 等 | 226 CTest PASS |
| schema 与边界 | 冻结 descriptor/语义与新增宿主接线均满足 | 13 PASS，含 4 项接线诊断 |
| Chrome 文本 | 创建、预编辑、去重、格式焦点、宽度、键盘、双击、切工具 | 6 组 PASS；synthetic DOM IME |
| 原有 Web Minimal Host | Ink/Shape、多指、Debug UI、resize、idle、retry 等 | 13 组 PASS；DPR=2 |
| 严格编译 | 不降低 /WX 或 -Werror | MSVC 19.51 x64 原生构建；Web Emscripten 构建；15 个本轮 C++ 源文件 Clang 22.1.8 syntax-only PASS |

初次 Web 回归在 DPR=1 浏览器中执行了要求 DPR=2 的断言，实际 ResizeObserver 恢复了浏览器
DPR。按原测试的 DPR=2 环境重跑通过，没有修改旧测试或 resize 策略。
严格 Clang 发现 UI 关闭构建中的未使用函数，已用对应条件编译修复。
所有中间失败分类保留在证据中，不能用最终通过覆盖它们。

## 阻塞与人工验证

锁定 Skia SDK 的 Noto CJK 字体只有 8732 bytes，覆盖 U+662F“是”，不包含“你好世界”或 Latin。
两宿主使用已锁定 Roboto 作为初始字体，并对缺字显示 `font-glyph-unavailable`。
通用中文显示为 **FAIL / MISSING_REQUIRED_INPUT（字体资源）**，没有使用系统字体静默替换、
修改 SDK lock 或自行生产字体 SDK。完整中文体验需要后续准入具备所需覆盖的字体资源。

| 人工项 | Expected | Actual / Result |
|---|---|---|
| Windows 实际中文 IME、候选窗与完成/取消 | native result 正确，坐标跟随 caret | 未人工观察，PHYSICAL_PENDING；普通中文显示另受上述字体阻塞 |
| Chrome 实际输入法、软键盘、粘贴与焦点切换 | 不重投、不丢终态，失败可见 | 未人工观察，PHYSICAL_PENDING；synthetic 测试不替代此项 |
| 触摸创建/定位/选择及鼠标双击编辑 | 不形成 Ink、不穿透 Debug UI | 未人工观察，PHYSICAL_PENDING；无单独触摸双击识别支持承诺 |
| 多显示器/DPI、resize/minimize/restore 与候选窗 | 几何不偏移，旧 token 不复用 | 自动 resize 回归通过，人工 PHYSICAL_PENDING |
| 强制销毁与受控离开区别 | 受控失败可见，强制销毁无伪造成功 | 自动失败路径覆盖；实际 IME/浏览器离页 PHYSICAL_PENDING |

本机为 HJS-OPS-ADLP-A08、Windows 11 Pro 22631、i5-12450H、Intel UHD 驱动 32.0.101.7082，
枚举到 HID touch screen 与 pen。设备枚举不等于真机输入验收。
本轮不修改 Authority、Golden、历史 Gate、PHYS、Android、SDK locks 或旧 Playground 接线。

## 复现与产物身份

命令、精确目标、环境事实、每项结果和原始日志在
[`fc02/evidence/host-geometry`](fc02/evidence/host-geometry/commands.txt)。
`qualification-artifact.json` 绑定提交前资格构建及对应源码 Git blob；
`source-files.json` 不把 Windows 换行转换当成源码差异。
提交后以 clean HEAD 重建，最终 revision、exe/JS/WASM/data 的 SHA-256、实际进程 PID/path
记录于本机 `out/fc02-host/final-artifacts.json`，交付时另行返回。
这份最终身份文件不自引用写回同一提交，以免生成另一份 dirty 产物。

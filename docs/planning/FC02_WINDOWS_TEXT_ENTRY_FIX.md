# FC-02 Windows Text 工具入口窄修复

基线：`2959c92df1d6b5e0560e9fa8343d4a82b46181b1`。
分支：`codex/fc02-runtime-boundary-design`。本记录追加人工失败与修复证据，
不改写前轮字体报告、Authority、Golden、历史 Gate、PHYS 或 Android 状态。

## 已观察的失败

用户报告：选 Text tool 后点击画板，仍出现画笔，无法进入打字。
该次 Windows 人工项目 W1 记为 **FAIL**，原始日志、exe hash、PID 和复现步骤
在 [`user-failure.json`](fc02/evidence/text-tool-entry/user-failure.json)。
最后一次画布点击的身份为 source=1 / pointer=1 / generation=9，
对象数和 Operation 数从 8 增至 9，没有文本输入回执。

旧日志没有记录工具选择载荷、工具读回或控制终态，不能单凭这份日志裁定
Text 请求未生效、被拒绝或后来被其他请求替换。

隔离 Win32 测试复现了一个确定的实现缺口：
实际 Debug UI Text 控件经 Router/Facade 生效，画布点击后工具仍为 Text，
会话为 Editing，但键盘焦点仍在 Debug UI HWND。鼠标与触摸均 RED。
直接把 WM_CHAR 发给主窗口的旧测试绕过了这个焦点条件。
在修复前，连续 10 笔 Ink 后再选择 Text 的工具及会话测试也通过，
所以这里没有把“工具自动变回 Ink”宣称为已复现的 Runtime 状态错误。

## 修复和边界

- 鼠标 Down 进入画布时恢复主窗口键盘焦点，再执行既有 capture/输入路径。
- 有效触摸/笔 Down 在既有 Windows 采集与映射后恢复同一焦点；Move/Up 不抢焦点，
  触摸不获取全局鼠标 capture，取消和 promoted mouse 过滤保留。
- 输入日志追加工具读回、文本 phase/session 和主窗口焦点。
  Debug UI 控制日志追加 request、action、终态、error 和 detail。
- 不改变 Runtime 文本、手势、Semantic、History 或布局策略。
  Debug UI 继续只通过 RuntimeFacade 提交和读回。

## 自动复验

`windows_text_tool_entry_test.cpp` 使用独立隐藏 HWND 和真实 ImGui 控件、Router、
Windows window procedure、Facade、Runtime 与锁定字体。
仅测试输入事实为合成数据；不会移动用户鼠标或控制正在人工测试的窗口。

| 测试 | Expected | 修复前 | 修复后 |
|---|---|---|---|
| Text 控件→画布 | Text/Editing，无 Ink 对象 | PASS | 见 summary.json |
| 鼠标焦点交接 | 字符送往画布 HWND，可输入中英文字 | FAIL：canvas_focus=0 | 见 summary.json |
| 触摸焦点交接 | 同上，且无全局鼠标 capture | FAIL：canvas_focus=0 | 见 summary.json |
| 连续 10 笔后选择 Text | 工具不回退、可创建文本会话 | PASS（工具及会话） | 见 summary.json；新增打字/结束/再创建检查 |
| Enter 后再次点击 | 保留 Text 工具；空草稿不创建额外对象 | 本轮补充回归 | 见 summary.json |

完整输出在 [`evidence/text-tool-entry`](fc02/evidence/text-tool-entry/summary.json)。
原生回归复用前轮 226 项清单并追加 4 项入口测试；相关 Python 与严格 Clang
检查同时保留。本轮没有修改 Web 生产代码，Web 人工 IME 验证仍待进行。

构建使用已接受 Semantic/Skia SDK、VS 2026 / MSVC 19.51 x64；
Debug UI=ON、profile=full、Render/RF01/Skia headless=ON。

```powershell
python tools/setup_build_environment.py --semantic --offline --facts-output out/fc02-text-entry/environment-native.json --status
$taskFacts = Get-Content out/fc02-text-entry/environment-native.json -Raw | ConvertFrom-Json
foreach ($taskFact in $taskFacts.environment.PSObject.Properties) { Set-Item -Path "Env:$($taskFact.Name)" -Value $taskFact.Value }
# 在 vcvars64.bat 的 x64 环境中执行
cmake --build out/fc01-closeout/build --parallel 6 --target axiom_minimal_runtime_host_windows axiom_minimal_windows_text_tool_entry_tests axiom_minimal_windows_text_pipeline_tests axiom_minimal_windows_input_pipeline_tests axiom_minimal_windows_touch_pipeline_tests
ctest --test-dir out/fc01-closeout/build --tests-from-file <out/fc02-text-entry/ctest-names.txt 的绝对路径> --output-on-failure --timeout 60 -j 4
python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract verification.tests.test_fc02_host_contract verification.tests.test_host_text_font_resource -v
```

## 人工复测

新版与旧版采用不同运行目录，保留旧测试窗口及其内容。
新 exe 的完整路径、SHA-256、源码 revision、dirty 标记、PID 和运行路径
在本机 `out/fc02-text-entry/physical/session.json`；输入日志为同目录 `manual-input.log`。

复测顺序：F12 → Control → Text tool → 点击画布空白处 → 输入中文并确认 →
Enter 结束 → 再次点击并输入；再检查画过 Ink 后切换 Text。
预期出现文字和 caret，不新增笔迹；候选窗、触摸与呈现位置须由用户观察。

原人工 W1 失败保留为 FAIL；本次修复的人工复测在实际用户反馈前为
**PHYSICAL_PENDING**。自动测试不是人工验收，本文不宣布独立 Review、Gate 或 PHYS PASS。

# FC-02 中文字体接入与人工验证

承接 `987aa6d1442084b6c48477a08194eb467817cd2e`，继续同一 FC-02 分支。
本轮采用未修改的 **Noto Sans CJK SC Regular 2.004**，作为应用资源接入现有
ResourceManifest/TextLayout/Render；没有修改 Semantic/Skia SDK lock、Authority、Golden、
历史 Gate、PHYS 或 Android。原字体缺字的历史 FAIL 保留，本轮以新增证据记录修复。

## 字体与身份

- 来源：[notofonts/noto-cjk](https://github.com/notofonts/noto-cjk/tree/f8d157532fbfaeda587e826d4cd5b21a49186f7c/Sans)。
- 原始文件：`assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf`，16,437,364 bytes。
- SHA-256：`2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b`。
- SIL Open Font License 1.1，允许随软件嵌入/分发；原许可、Adobe 版权与 Noto 商标通知均随包保留。
- 新资源 ID 为 `0xFC0202`，collection index=0、weight=400、italic=false。
  原 Roboto ID 不替换，不启用系统字体 fallback 或合成字体样式。

`resource.json` 是本应用字体的来源与内容锁定记录。CMake 检查字节数/hash，再生成宿主
hash 常量。运行时继续使用既有资源校验和 exact face resolution。Windows 和 Web 包含同一
OTF；Web `.data` 内包含完整许可与版权文件，页面目录也保留可读副本。
这不是 Skia SDK 的重新生产，也没有把字体格式或布局逻辑搬进宿主。

## 自动验证

| 项目 | Expected | Actual / Result |
|---|---|---|
| 原字体 RED | 中文预编辑几何不可用，必须失败 | 1 FAIL，修改前复现；`font-red.log` |
| 新字体实际渲染 | 中英/简繁/标点有真实字形；预编辑无 Object/Operation，确认只提交一次 | 新增集成测试 PASS，含实际 raster pixels；FC-02 共 25 PASS |
| 资源完整性 | 原始文件 hash/size/face/license 正确，包含 GB2312 与测试混排字表 | 3 PASS；44810 个 cmap 映射；未承诺所有 Unicode/emoji |
| 范围回归 | 既有 composition、Brush、retry、identity、capture、resize 等不退化 | 226 CTest PASS，0 skipped |
| Python 合同与资源 | 既有 schema/接线与新增资源测试均正确 | 16 PASS |
| 严格编译 | 维持 MSVC /WX、Web -Werror 与严格 Clang | Windows/Web 构建 PASS；本轮 3 个原生 C++ 源文件 Clang syntax-only PASS |
| Chrome actual Web target | 中文预编辑/确认/几何可用；缺字仍显式报错且可恢复 | 7 组文本测试 PASS；native DOM events 为 synthetic |
| 原有 Chrome 回归 | Ink/Shape、多指、Debug UI、resize、idle、retry 等保持正常 | 13 组 PASS，DPR=2 |
| 打包一致性 | Windows/Web 字体 hash 相同，Web 内嵌 license/notice | PASS，`package-checks.json` |

Web 最初构建失败于 CMake 对重复 `--preload-file` 的去重；使用 `SHELL:` 参数组修复，
没有移除许可文件或降低编译检查。缺字负例改用本字体没有编码的有效 scalar U+10FFFD，
原 Roboto 单独测试中的中文缺字负例仍保留，防止引入隐式 fallback。

## 人工验证计划与记录原则

每个宿主独立记录。软件打开、事件日志出现、自动测试通过都不能代替用户对候选窗与
触摸的观察。用户未报告的项目保持 PHYSICAL_PENDING；观察到的实现失败记录 FAIL。

| ID | 用户操作 / Expected | Actual / Result |
|---|---|---|
| W1 | Windows 选 Text 后输入“你好，世界！Axiom 中文输入测试123”；预编辑可见、候选窗跟随 caret；确认后位置不变且无重复 | PHYSICAL_PENDING |
| W2 | Ctrl+Enter 换段、Enter 结束；F12 修改独立宽度产生换行，Undo/Redo 可恢复 | PHYSICAL_PENDING |
| W3 | 未完成合成时切 Ink/Shape、Alt+Tab、受控关闭；仅原生确认结果提交一次，失败可见；Esc 取消不误提交 | PHYSICAL_PENDING |
| W4 | 触摸创建/定位/选择；Debug UI 不穿透；resize/maximize/restore 后文字、caret、候选窗一致 | PHYSICAL_PENDING |
| B1 | Chrome 执行同样的中文预编辑、确认、换段和结束；无重复或丢字，候选窗位置正确 | PHYSICAL_PENDING |
| B2 | Chrome 宽度、Undo/Redo、切工具、实际焦点/页面切换；受控失败可见；强制离页不伪称自动保存成功 | PHYSICAL_PENDING |
| B3 | Chrome 实际触摸、Debug UI 不穿透、resize/DPI 后文字几何一致 | PHYSICAL_PENDING |

Windows 设置 `AXIOM_MINIMAL_INPUT_LOG`，记录完整文本 session/projection 身份、序号、
状态与对应对象/Operation/frame；指针日志保留 source/pointer/generation。
Chrome 人工窗口使用隔离 profile；CLI 仅观察可信原生事件及 Runtime 读回，不生成输入。
人工日志绑定当次 exe/页面的精确 revision、dirty、PID/path、字体 hash 与应用产物 hash。
后续人工结果作为追加记录，不反写本文件中的历史自动验证或历史 PHYS 状态。

## 复现

使用已锁定 SDK、VS 2026/MSVC 19.51 x64 与原构建 cache：

```powershell
python tools/setup_build_environment.py --semantic --offline --status
python tools/setup_build_environment.py --semantic --target web-wasm32 --offline --status
cmake --build out/fc01-closeout/build --parallel 6 --target axiom_fc02_text_tests axiom_minimal_runtime_host_windows axiom_minimal_windows_text_pipeline_tests
cmake --build out/p32-web/build --parallel 4 --target axiom_canvas_demo_web
ctest --test-dir out/fc01-closeout/build --tests-from-file <原226项测试清单绝对路径> --output-on-failure -j 4
python -m unittest verification.tests.test_current_semantic_descriptor verification.tests.test_semantic_contract verification.tests.test_fc02_host_contract verification.tests.test_host_text_font_resource -v
python -m http.server 8769 --bind 127.0.0.1 --directory out/p32-web/build/apps/axiom_canvas_demo
.deps/emsdk/node/24.19.0_64bit/node.exe docs/planning/fc02/evidence/host-geometry/chrome-test.mjs web_fc02_text_test web_minimal_runtime_test
```

证据在 [`fc02/evidence/cjk-font`](fc02/evidence/cjk-font/summary.json)。本机最终运行路径、
hash、PID 与人工记录保存在 `out/fc02-font/`，不得把未观察项目转换成 PASS。

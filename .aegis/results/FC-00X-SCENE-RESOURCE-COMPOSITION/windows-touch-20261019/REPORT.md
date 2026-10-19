# Windows Minimal Runtime Host 本轮结果

实现和构建源码 revision：`b68e8f9f2c1bca65277f832dd78193f4ae0e2c0f`。基线：`f1d72f22ad20209c8ccdbdd7474d86c651434914`。
后续证据提交只保存日志/报告；它不会更改这份源码或 exe。

Windows 触摸接入、完整 pointer key、逐指取消、promoted mouse 去重、mask-gated
history/pressure/contact/tool、失败 Down 清理和 resize/focus/capture 接线已实现。
Runtime 的取消相机收尾和压力能力传递已修复。真实启动另外复现并修复了
192 DPI 下 production clip 错用整数 fixture 校验的问题；headless 合同保留。
AutoIntent 的原有 250 ms 逻辑保持不变，没有增加 rotation 支持。

MSVC 19.51 `/W4 /WX` 构建通过；48 个选定测试逐项见
[test-results.md](test-results.md)，原始断言输出在 [JUnit](logs/selected-tests.xml)。
真实 D3D12 首帧和 CLI 空闲采样见 [运行日志](logs/native-input.log) 与
[idle observation](facts/idle-observation.json)。这不构成人工触摸或视觉 PASS。

exe：`C:\Users\NearHub\.codex\worktrees\p32-windows-touch\axiom\out\p32-touch\build\apps\minimal_runtime_host\axiom_minimal_runtime_host_windows.exe`

SHA-256：`6f9d85b1cbcc276b8821721d5269d2f4fcee325f9cddef15b3e202f8cf1e7bdb`；实际进程 PID：`15820`，进程路径和文件 hash 相符。
启动日志内嵌源码 revision，`dirty=0`。Debug UI 编译启用，F12 显示仍待人工观察。

设备为 HJS-OPS-ADLP-A08、Windows 11 Pro 22631、i5-12450H、Intel UHD Graphics
驱动 32.0.101.7082，HID 触摸屏 VID_08D3/PID_1000。
Win32 当前桌面报告 DISPLAY1、2560×1440、192 DPI；VideoController 另报告 3840×2160，
两种 OS 查询结果均保留，未将它们混写为同一实测分辨率。
详见 [machine](facts/machine.json) / [displays](facts/displays.json)。

原始 Clang `/WX` 仍为 FAIL：未改动的 stable_order_key.hpp:29 有 sign-compare 告警。
仅屏蔽该确切旧 header 的预包含诊断后，本轮改动的 9 个 translation units 通过
clang-cl 22.1.8 `/W4 /WX /Zs`。不声称完整 Clang 构建通过。

## 构建和复核命令

在仓库根执行；原始实际参数保存在 facts/selected-commands.json 和日志中。

```powershell
python tools/setup_build_environment.py --core --semantic --facts-output out/build-environment.json --status
python tools/setup_build_environment.py --semantic --offline --status
cmd /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && python .aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/windows-touch-20261019/configure.py && python .aegis/results/FC-00X-SCENE-RESOURCE-COMPOSITION/windows-touch-20261019/run_checks.py'
```

配置保留 full Debug UI、Render、RF01、Skia headless；SDK facts 中的 host protoc/runtime
通过 consumer_cmake_arguments 消费。Skia 复核为 installed、0 下载字节；没有重编 SDK。
工具链精确版本见 [toolchain](logs/toolchain-facts.log)。

## 失败历史与回归

以下 FAIL 均保留为已观察事实；最终回归通过不覆盖历史失败。
部分早期诊断针对未提交的中间工作树，其输出不冒充精确提交的 Gate 证据。
最终构建和回归统一绑定上面的精确 result_revision。

| 诊断 | 观察到的 FAIL | 原始日志 | 最终回归 |
|---|---|---|---|
| Windows pointer 入口 | expected=1 actual=0 | [log](logs/touch-red.log) | PASS |
| 混合 source batch | 接纳了混合 batch 的 Down | [log](logs/regression-red2.log) | PASS |
| 旧 generation/native cancel | 旧取消结束了复用后的 pointer | [log](logs/regression-red2.log) | PASS |
| 取消后的相机基线 | 新手势 scale 跳变 | [log](logs/regression2.log) | PASS |
| 缺失设备压力 | missing-pressure policy expected=.5 actual=0 | [log](logs/pressure-red.log) | PASS |
| 失败鼠标 Down/capture | GetCapture 仍是 owner | [log](logs/mouse-capture-red.log) | PASS |
| 真实 192 DPI 启动 | 首帧持续 invalid transform or clip | [log](logs/native-stderr-first-fail.log) | PASS |
| D3D12 fractional clip oracle | progressed=0 canonical=0 | [log](logs/fractional-red.log) | PASS |

## changed_files

- `apps/minimal_runtime_host/CMakeLists.txt`
- `apps/minimal_runtime_host/tests/windows_surface_binding_test.cpp`
- `apps/minimal_runtime_host/tests/windows_touch_pipeline_test.cpp`
- `apps/minimal_runtime_host/windows_main.cpp`
- `apps/minimal_runtime_host/windows_pointer_input.inl`
- `runtime/composition/CMakeLists.txt`
- `runtime/composition/include/canvas/runtime/runtime_composition.hpp`
- `runtime/composition/src/runtime_composition.cpp`
- `runtime/composition/tests/brush_batch_frame_retry_test.cpp`
- `runtime/input/src/platform_interaction_ingress.cpp`
- `runtime/input/tests/platform_interaction_ingress_test.cpp`
- `runtime/interaction/src/canvas_editing_runtime.cpp`
- `runtime/render/src/skia_headless_backend.cpp`

- 本目录下新增诊断、命令、facts、逐项测试结果和人工矩阵。

## 未完成项

[人工矩阵](manual-matrix.md) 全部维持 PHYSICAL_PENDING，等待用户逐项 actual。
无本轮自动回归失败；是否需要下一轮 Windows 输入窄修复取决于这些观察。
基础层 Clang 告警需另行授权的窄修复。未修改 Authority、Golden、历史 Gate、PHYS
或 Android 状态；没有宣称独立 Control Review/Gate/真机验收 PASS。

Win32 采集依据为 Microsoft 的
[pointer history](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getpointertouchinfohistory)、
[pointer capture](https://learn.microsoft.com/en-us/windows/win32/inputmsg/wm-pointerdown) 和
[optional touch masks](https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-pointer_touch_info)。

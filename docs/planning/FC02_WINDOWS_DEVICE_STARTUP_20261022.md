# FC-02 本机 Windows 启动失败与分发恢复

日期：2026-10-22。承接分支：`codex/fc02-device-startup-20261022`。
精确源码/文档基线：`164dfa2fc0af84df397b3ca2de0f6543fd50da02`；实际测试产品
revision：`3d7afa351dfbd42f18c449bda35f861089b1fb4f`。

## 启动故障的结论

本次故障属于 **进入 WinMain 后资源初始化失败、主动返回退出码 1**。用户确认
从原机器只复制了 exe。原目录既没有 `axiom-font.otf`、`fonts/`，也没有
`icudtl.dat`。同一个 SHA-256 的 exe 补齐既有 CMake 打包资源后持续运行、记录
startup 与首帧。没有修改 Runtime、渲染器或安全策略，也没有安装 VC 运行库。

精确失败点由源码及逐步资源对照共同定位：

- `windows_main.cpp` 在创建窗口、Debug UI、两个 D3D12 surface 和绑定 surface
  之后，调用 `bindHostTextFonts()`；失败直接返回 1，早于 startup 身份日志和
  ShowWindow。此路径没有输出具体字体失败原因。
- `host_font_resources.hpp` 要求 exe 目录下主 CJK 字体和 imported catalog 中
  每个字体文件均存在，大小正确，之后由 Runtime 按 manifest/resource 绑定。
- 原日志已经记录 canonical/preview 的 `resize ready`。因此不能将此次退出归为
  系统拒绝执行、未进入 main 的 DLL 缺失，或已经证实的 GPU 初始化失败。
- stderr 的 `SkIcuLoader: datafile missing` 是真实缺失证据，但**仅补 ICU 不足以
  恢复**。本次直接阻断启动的是不完整的字体分发；ICU 数据也按原打包规则补齐。

| 独立运行 | 资源条件 | PID | 实际结果 |
| --- | --- | --- | --- |
| 原目录原文件 | 只有 exe，缺字体及 ICU 数据 | 40004 | FAIL，退出码 1；ICU 缺失诊断；两个 surface ready；无 startup |
| ICU-only 对照 | 原 exe + 锁定 ICU 数据 | 73160 | FAIL，退出码 1；不再报告 ICU 缺失，仍无 startup |
| ICU + CJK 对照 | 再补主字体和 notice/license，未补 catalog 字体 | 40380 | FAIL，退出码 1；仍无 startup |
| 完整资源 | 再补整个 fonts/ | 72572 | CLI 启动检查 PASS；响应正常，startup/首帧出现；人工功能另记 |

原始 FAIL 永久保留为 FAIL，后续试验使用不同目录。所有运行的 exe SHA-256 均为
`bf917e023f35f0342e21d0dc5dab54d665bb079da12eba229b970bda8fdc53c8`，
文件大小为 12,933,632 bytes。未以重编后的产物替代原复现。

## 源码和现场身份

原文件路径为
`C:\Users\NearHub\Desktop\12450hshare\axiom_minimal_runtime_host_windows-3d7afa35.exe`。
复制方式的具体传输协议未提供；用户明确确认只复制 exe。原文件、目录 manifest、
stdout/stderr、输入日志、退出码和 PE 信息在修改/构建之前已保存。

已确认 Git remote 为 `https://github.com/Mostorm-Labs/axiom.git`。fetch 后远端
`codex/fc02-windows-ime-resume` 正好为 `164dfa2f`，新增 delta 为 0。
`3d7afa35` 是其 ancestor，两者差异仅涉及 42 个 docs 路径。独立 managed worktree
从精确 `164dfa2f` 创建承接分支；原 clone 的旧多指证据保持不动。

本机：Windows 11 Pro 10.0.22631 x64，Intel i5-12450H；Intel UHD Graphics 驱动
32.0.101.7082，并存在 OrayIddDriver 虚拟显示设备。活动 monitor API 读回为
2560×1440、192 DPI；Win32_VideoController 的模式字符串另为 3840×2160，保留两项
原始事实而不混用。HID VID_08D3/PID_1000 同时声明 touch/pen，系统报告最多 50
触点；本次不以设备声明值推导实际功能通过。

PE 为 x64 / Windows GUI。直接依赖为 USER32、GDI32、IMM32、dcomp、d3d12、dxgi、
D3DCOMPILER_47、KERNEL32、SHELL32、dbghelp；均在本机存在。完整运行实际加载
Intel D3D12 UMD。源码按高性能偏好枚举、跳过 software adapter、要求
D3D_FEATURE_LEVEL_11_0，不静默切 WARP；本次没有修改它。实际选中 adapter 的
LUID/描述未在此 exe 日志中直接输出，不把已安装 GPU 清单当作这项精确读回。

本次失败窗口的 Application/WER 1000/1001 查询没有匹配事件；这是普通 exit=1
而非已观察到的异常。较早另一项 surface-binding 测试的 c0000409 事件不属于
本次 exe/PID，没有误当本次 crash。未见系统拦截提示或预 main loader 错误；未
关闭 SmartScreen、杀毒或系统策略，也未启用全系统 dump。

## 完整交付和本机验证

完整目录与 ZIP 位于桌面 `Axiom-FC02-3d7afa35-Windows-x64-20261022`，详见
[delivery.json](fc02/evidence/device-startup-20261022/delivery.json)。包保留既有 CMake
资源结构、字体及 SDK notices，附独立启动日志 launcher 与逐文件 SHA-256 manifest。
不包含 imgui.ini；不能再次只复制其中的 exe。测试进程使用 worktree 的
`out/fc02-device/distribution-complete`；桌面副本与 ZIP 的不可变文件逐项比较。

本次没有产品代码修改和产品重建，因而没有套用迁移的 CMakeCache，也没有把历史
234 项 CTest、17 项 Python 通过结果冒充本机新运行。只执行本机分发、资源身份、
逐步启动、原生进程和日志检查；产品自动回归标记 NOT_RUN_DEPLOYMENT_ONLY。
如后续产品修复需要构建，仍须按任务要求新建 MSVC 19.51 /MT /WX Release build、
解析 Semantic SDK 并二次 offline 验证，使用冻结清单；不能无差别构建旧 Playground。

锁定 Skia consumer 对本机已有安装执行全量校验，返回 source=installed、下载 0
bytes、SDK ID `29ebc4ef2655e182c7da5996c2a0b23cb07214f1efcacc2bf149dd7c841c99fa`。
资源取自该验证后的 SDK 和精确 worktree 中逐项匹配 manifest/catalog 的字体。
没有源码重编 Semantic/Protobuf/Abseil/protoc/Skia，没有修改 SDK locks。

人工验证按用户指定十项顺序继续。CLI 对象/operation/receipt 及事件可辅助定位，
不能代替用户观察候选窗、光标位置、真实触摸与显示。逐项 expected/actual/result
在 [manual-matrix.json](fc02/evidence/device-startup-20261022/manual-matrix.json)；
未确认的项目保留 PHYSICAL_PENDING。此记录不宣布 FC-02 完成或独立审查通过，
不修改 Authority、Golden、历史 Gate/PHYS，也不启动其它 FC 工作。

本轮 [证据入口](fc02/evidence/device-startup-20261022/README.md)保留原始 FAIL、
逐步对照、完整资源清单、日志、进程/设备事实及剩余限制。result revision 通过
`git log -1 --format=%H -- docs/planning/fc02/evidence/device-startup-20261022/result.json`
解析，不将文档提交的 revision 写进原 exe 身份。

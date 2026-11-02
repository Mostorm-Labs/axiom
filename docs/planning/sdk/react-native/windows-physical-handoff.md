# Windows React Native physical verification handoff

Copy the prompt below into Codex on the Windows machine. This verifies the private
RN 0.2.0 development SDK, not the independently published Web/Windows SDK 0.1.0.
Use the latest verified descendant of the branch anchor; bind evidence to the exact
clean source and rebuilt binaries. No native package publication is authorized.

```text
请在这台 Windows 真机验证 Axiom React Native SDK。目标是真实 RN Windows
App + AxiomCanvasView；不能用 Web、WebView、普通 Win32 demo 或 host tests 代替。

仓库：https://github.com/Mostorm-Labs/axiom.git
分支：origin/codex/react-native-sdk
最低基线：1c8aebc4909c410465cd1bd3c554d01b64be170a
SDK：0.2.0 development，private / NOT_QUALIFIED，禁止发布或修改 v0.1.0。

1. 先检查已有工作树与未提交文件，git fetch origin；使用独立 worktree 和
   codex/rn-windows-physical 分支，从上述远端分支当前 head 开始。
   不覆盖本机已有改动。记录 git rev-parse HEAD，并确认最低基线是其 ancestor。
   阅读 AGENTS.md、docs/notion/manifest.yaml、sdk/react-native/README.md、
   docs/planning/sdk/react-native/design.md，以及 SDK 的 js-v1-contract.md、
   windows-consumer-v1.md、shell-axis-preview-contract.md。

2. 确认 x64 Windows、Node 22、Python 3.12、CMake/Ninja 和 VS C++/Windows SDK。
   用户于 2026-11-02 接受 Windows 独立版本线：RN 固定 0.84.1，React 固定
   19.2.3，react-native-windows 固定 0.84.0。其他平台原有 0.83 证据不重标记。
   当前 RNW 工程要求 v145；核对接受的 Skia/CRT/toolchain 兼容性，不能盲目降级。
   在匹配的 x64 Developer PowerShell 中执行构建。
   Semantic 必须使用 semantic-sdk.lock.json 的 windows-x64-msvc-static；
   Skia 必须使用 r1-full-skia-sdk.lock.json 的 windows-x64-d3d12。
   禁止 bootstrap --semantic-codec、.deps/protobuf、从源码重建 Semantic/Skia
   或为了通过而改 lock。下载失败先诊断 resolver/Store/网络，不改依赖身份。

3. 在干净源码上构建和打包（输出目录必须是新目录；不能覆盖旧证据）：

   python tools/release/build_native_sdk.py --target windows-x64 --build out/rn-windows-native --output out/rn-windows-install
   python tools/release/native_package.py --install windows-x64=out/rn-windows-install --output out/rn-windows-package --revision (git rev-parse HEAD)

   builder 会执行第二次 Semantic offline resolution，保留日志。
   核对 native-manifest.json/package manifest 的 source_revision、版本与哈希。
   仅 sdk/react-native 源码不是可安装 SDK，必须安装 staged package。
   正常消费者只能得到成品 DLL/import lib、公开 C header、资源和许可证，
   不得包含内部 Runtime/Skia/Protobuf headers 或静态库。

4. 在 out/ 下创建独立 RN Windows 0.84.0 / RN 0.84.1 Fabric 消费者工程。
   先阅读官方当前模板/本地 CLI --help，再调用 init-windows/run-windows，
   不猜 CLI 参数，不切换到旧架构。安装 out/rn-windows-package 后确认
   autolink/codegen 包含 AxiomNative TurboModule 与 AxiomCanvasView component。
   参考 sdk/react-native/example.tsx 和 iPhone evidence 的 consumer/App.tsx
   构建 toolbar：Round round-handdrawn@4、Marker marker-compact-v1@4、
   Chalk chalk-compact-v1@3、Crayon crayon-wax-v1@3、
   Watercolor watercolor-lite-v1@3、Laser laser-transient@1，
   Object Eraser、Partial Eraser、Undo/Redo、Detach/Reattach。
   UI 只能调用包根入口的 Controller 和 AxiomCanvasView，不使用私有 deep import。
   本地 file: symlink 时给 Metro 配置 staged package watchFolders 和 app
   node_modulesPaths，按 RNW 的 platform resolver 解析 react-native。
   确认 axiom_native.dll、ICU/fonts/assets 实际复制到 App 可访问的位置。
   非打包应用必须初始化 app-local Windows App SDK。C++/WinRT provider
   的输出名为 MostormAxiom.dll（与 activation namespace 一致）；不要再用
   AxiomReactNative.dll 的旧输出冒充本轮构建。Canvas HWND 必须在自己的
   有界矩形内位于 RNW ContentIsland 兄弟窗口上方，并验证命中目标。
   构建并启动真实 x64 RNW 窗口，保存完整构建、部署、启动日志。

5. 真机逐项验证并保存前后截图/录像、Controller state 和 command results：
   - 六笔型，快速/慢速、多笔连续绘制；鼠标是必测，触屏/笔按实机能力记录。
   - 绘制中 preview 连续，松手 canonical 接续，位置一致、无残留/闪烁。
   - canonical/preview 独立 Skia providers；记录 D3D12/Composition backend。
   - partial eraser 实时显示最终擦除效果；object eraser 松手删除；Cancel 不提交。
   - Undo/Redo，Detach/Reattach 保留文档和 History，旧 attachment/token 拒绝。
   - 控件点击不画到 canvas，画布输入不破坏 RN toolbar。
   - resize、最小化恢复、切换焦点、多屏/DPI（可用时）、设备/surface lost/rebind
     （存在受支持的触发方式时）；记录 generation、dirty 和恢复结果。
   - 支持的双指缩放/触控笔真实压力需要物理输入；无设备标记 Unavailable。
   - RN hosts 当前原生 IME 未实现，必须明确 OPEN/BLOCKED；程序化 text command
     和旧 Windows SDK 的 IME 不能充当 RN IME 证据。不要因此伪造全平台通过。

6. 若遇到集成/编译问题，可以在本轮独立验证分支做最小修复；先定位根因，
   编辑/几何/History 仍由既有 Runtime owners 负责，平台只做适配。
   修复必须提交到干净 source 后重建 native install 与 package，证据记录新 SHA。
   不允许 JS 几何、GDI/WebView fallback、跳过失败检查或重写旧产物身份。
   如涉及公共 API/ABI 或架构新增，先给出具体缺口，停止相关范围并继续独立检查。

7. 保存 docs/planning/sdk/react-native/evidence/<实际日期>-windows-<run-id>/：
   observation.json、device-metadata.json、构建/运行日志摘要、截图/录像、
   native/package manifests、consumer 工程关键配置和 SHA-256 manifest。
   内容必须含 exact source revision、native DLL/JS/resources SHA-256、OS/CPU/GPU/
   driver/输入设备/屏幕与DPI、RN/RNW/React/toolchain版本、backend/provider identity、
   每一步 expected/observed/pass/fail/unavailable、state/receipt/generation、capture hash。
   未从公开状态拿到的内部计数应写 unavailable，不能猜值。

最终报告实际测试范围、失败和未验证项。只有完整的本轮证据可交付时标记
READY_FOR_CONTROL_REVIEW；环境/构建/实际输入验证未完成则返回
BLOCKED_EVIDENCE: <具体原因>，并保留已经完成的局部证据。
RN 0.2.0 仍保持 NOT_QUALIFIED；不得签发 P34/Gate/release PASS。
```

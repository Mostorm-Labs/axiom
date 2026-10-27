# FC-08 Create Section — actual host/button repair

用户在 f3d9e675 产物上的“Create Section 按钮无效”结果为 **FAIL**，保留于 user-failure.json；新复测单独记录。

实现 revision：`7c09dc7f8fd43e975228e719698a8546f822ef7a`，分支 `codex/fc08-section-scene-host`。现有 D/E v0.5 包的宿主和 Debug UI 修复范围内执行，未增加语义或接口授权。

根因有两处：

1. Windows 实际挂载的 MinimalRuntimeFacade 没有转发 readSection，真实面板取得默认 selectionRevision=0。Runtime 正确拒绝为 Stale。create-button-red-010 实际按钮记录：owner_state=4、owner_section_failure=3，对象数仍为 0。
2. Section 请求没有登记 pendingProduct_，Debug UI 没有查询终态。仅补 readSection 后，真实对象已创建，但界面 Applied 断言仍失败，见 create-button-receipt-red-011。

修复只转发现有 Runtime 读回并沿用已有 UI 回执跟踪；没有去掉过期保护，没有修改 Runtime、Schema、SDK locks、冻结输入/expected/oracle 或全局控制记录。

新增 FC08MinimalSectionDebugUi 经真实 DebugController、ImGui 控件、MinimalRuntimeFacade、Composition/History 和 Scene 执行：Create → 实际 canonical 像素出现 → Release 后恢复空白；并覆盖 Wrap、世界坐标保持、Undo/Redo 和 Applied 终态。控件激活是测试夹具输入，不冒充人工触摸验证。

精确提交的验证：create-button-native-exact-014 中 8 项 CTest 全部通过，0 skipped；Windows /WX Release 和 Web Minimal Host 构建 exit 0；create-button-browser-005 实际 Chrome/WASM 42 项通过，内嵌 source revision 正确且 dirty=0。完整冻结 302 变体与先前两项基线失败的状态没有被重写，本次未重跑/宣称它们通过。

新版 Windows 实际运行 PID **273592**，完整路径/hash 见 running-windows-002.json；启动日志确认上述 revision、dirty=0，已接受首帧。人工按钮复测仍 PHYSICAL_PENDING。live manual-windows-002.log 为持续输入日志，局部忽略；提交的 snapshot 日志是稳定的启动截面。

新版位置：`out/fc08-section-button/7c09dc7f/axiom_minimal_runtime_host_windows.exe`。F12 → Control → Create Section；也可测试 Wrap selection、Release Section 和 Undo/Redo。

为了不覆盖用户运行中的原 exe，构建输出移至版本目录。原打包函数固定复制字体到原 binary directory，首次移位启动缺字体而退出；已将原目录经校验的 fonts、axiom-font.otf、axiom-font.LICENSE.txt、axiom-font.NOTICE.txt 原样复制到新版目录，保留 ICU，第二次启动成功。首轮失败及补齐记录均保留，没有扩大修改打包源码。携带程序时保留 resource-manifest.json 列出的相邻资源。

前置、RED、GREEN、精确构建、浏览器和审计目录均在上一级。该修复不声明 DEV/P34/PHYS/Gate PASS，也未合并 main。

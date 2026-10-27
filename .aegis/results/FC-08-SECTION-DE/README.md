# FC-08 Section D/E — bounded implementation result

实现提交：`f3d9e67508b4b8f6e2ddbf756b51c543efeafa55`。物化分支：`refs/heads/codex/fc08-section-scene-host`。
返回 CONTROL_REVIEW；本记录不宣布 FC-08、DEV、P34、PHYS 或 release 通过。

## 实现范围

- Scene 复用已有 Placement 森林，祖先变换只组合一次；Section 使用自身边界，子对象超出边界仍可见。支持负边界原点、Group 根级选择、现有作用域与一致发布/恢复。
- `RuntimeFacade::submitSectionIntent/readSection` 通过已有队列、Composition、Interaction、真实 F/U 与 EditorHistory 路径提供创建、包裹、释放、重挂父级、边界缩放、重命名和选择。快照有完整目标身份与选择 revision；回执区分 canonical 提交和 Scene 就绪。
- Windows/Web 共用 Debug UI 的 Control → Section。宿主只提交意图。Section 标题由既有 TextLayout 注册字体排版，晚到字体资源可刷新；没有新增 canonical RichText 子对象。
- 拖动预览与取消沿用既有路径；边界缩放不移动子坐标。默认删除一个 Section 释放子对象；混合破坏性删除在写入前拒绝。

## 验证

| 项目 | 原始结果 |
| --- | --- |
| PackageBindingPreflight / EvidenceContractPreflight，真实 worktree `--repo .` | PASS，见 preflight-exact-007 |
| Windows /WX Release 与实际 Web Minimal Host | 构建 exit 0，源码 exact revision，dirty 0 |
| Native scoped + impacted CTest | 38 项：37 PASS，1 既有失败，0 skipped，原始 CTest exit 8 |
| 继承的语义/History 矩阵 | 690 项：689 PASS，1 既有失败，0 skipped，原始 CTest exit 8 |
| Axiom Python contract tests | 10 PASS；运行时边界检查 101 个合同文件通过 |
| 实际 Chrome/WASM | 42 个断言通过；包括真实 glyph、世界坐标、Undo/Redo、重复/过期请求及继续渲染 |

两项失败均保留原始失败状态：

- `StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds`：继承 A/B/C 已独立复现的 Shape RichTextTarget 旧债。
- `FC01TriangleConnectorRaster`：精确旧基线 `af1aff80` 的隔离工作树同样在未修改测试的第 751 行、同一像素失败；见 baseline-raster-001。没有修改 expected 或旧测试。

RED（几何、普通消费者入口、默认删除、Group 选择后置结果）与修复后的 GREEN 均保留。早期构建、工具或桥接输入失败没有覆写。

## 复现与人工测试入口

在本 worktree、VS 2026 x64 环境下运行（phase 使用新目录名，已有证据不可覆盖）：

```powershell
python tools/fc08/section_de.py preflight --repo . --phase <new-preflight>
python tools/fc08/section_de_build.py --repo . --profile windows --phase <new-native>
python tools/fc08/section_de_build.py --repo . --profile web --phase <new-web>
python tools/fc08/section_de_regression.py --repo . --phase <new-semantic>
python tools/fc08/section_de_regression.py --repo . --phase <new-python> --python-only
node tools/fc08/section_de_browser.mjs --repo . --output .aegis/results/FC-08-SECTION-DE/<new-browser>
```

具体展开后的命令、环境、工具链、原始退出码见每个目录的 invocations.json；Chrome 为 invocation.json。消费已锁定 SDK，不重编依赖。

Windows：`out/fc08-section-c/native/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe`；保留同目录 fonts、ICU 等资源。完整路径/hash 见 artifacts.json。
Web：`out/fc08-section-de/web/apps/axiom_canvas_demo`，本机 loopback 静态服务器打开 index.html。

人工重点：F12 → Control → Section 创建；选中现有对象后 Wrap selection；移动整体再调整 Section 边界；Rename；Release 与 Undo/Redo；设置 move target 后 Move into target / Move to root。边界调整时子对象不能移动，释放后子对象不能丢失。

## 未完成范围和下一最小步

完整 302 变体验收 runner 和所有 E01–E10 资格证明尚未完成；本轮测试不是对它们的替代。实际鼠标/触摸、IME、DPI 与原生 GPU 人工验收仍 PHYSICAL_PENDING。故障/分配资格矩阵和 G5 性能结论未声明。
公共释放命令当前一次释放一个 Section；多 Section 同时删除明确 Unsupported。自动拖放重挂父级等 INT-01 高级交互继续归 FC04/FC10；PH01–04 继续归 FC13。
Debug UI 标题编辑有 4096 字节本地输入框限制，超长标题禁用该输入；公共入口支持有界完整 UTF-8，不静默截断数据。
下一最小切片是将已落地的真实命令/Scene 观察口接入剩余冻结用例，并完成 Windows/Web 真机回归；不新增另一套容器或 History。

精确 owner/source 绑定见 ImplementationDesignPreflight.final.json；所有原始证据摘要见 evidence-manifest.json。最终交付身份见 terminal-result.json。

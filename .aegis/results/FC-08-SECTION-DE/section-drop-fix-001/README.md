# FC-08 accepted Section drop completion

用户在 revision 7c09dc7f 上确认 Create Section **PASS**，但 Shape/Stroke 拖入 Section 后没有归属、不随 Section 移动为 **FAIL**；分别保留于 user-observation.json，不覆盖先前 Create Section 的原始 FAIL。

实现 revision：`68565bdd2eeff0594052c3f3f867831ff48db1ce`。物化分支：`refs/heads/codex/fc08-section-scene-host`。授权范围为 D/E v0.6 补齐 P13/P15/P16 已接受的 drop；先前报告把自动拖放误列为 INT-01 延期项不准确，INT-01 指混合破坏性删除，原零写入拒绝保持。

## 行为和 owner

- Select 下当前单对象拖动松手时，以 confirmed release point 命中实际仿射 Section boundary，采用最上层且当前 scope 可编辑的 Section；排除自身、后代、隐藏/锁定或 scope 外目标。不按 AABB 或宿主画面重叠写成员关系。
- 穿越过程只有 preview，无 canonical membership/History 写入；Up 一次 RelocateObjects 提交最终 confirmed preview world pose、parent、order 和 local-after。没有先 move 再 reparent 的两步提交。同一父级移动继续 SetTransforms。
- 拖出所有 Section 后回到当前 Group scope 或 root。Section 移动只改根 Transform，整棵结构后代经已有 Scene 组合跟随一次；边界 Resize/Rotate 不触发 drop。
- 复用 CanvasEditingRuntime planner、真实 F/U 和 EditorHistory reservation、OperationEngine 与 Scene publication/handoff；摄像机、surface/metrics、semantic generation 和 selection revision 失效时不提交旧手势。没有修改公共 Facade 类型、Schema、SDK locks、注册表、冻结输入/oracle/expected 或历史证据。

## 实证

- section-drop-red-001：真实普通消费者 Composition 指针输入，Shape、VectorStroke 和仿射目标共 3 个新用例因无成员关系失败；原失败保留。
- section-drop-native-exact-005：24 项 CTest 全部通过，0 skipped，原始 exit 0。其中 Section consumer executable 共 14 个 GTest，覆盖拖入/拖出、最终位置不跳、一步 Undo/Redo、随 Section 移动、嵌套与防环、实际仿射边界、Cancel/metrics 变更和迟到 Up；并运行既有 History、Text、Image、Connector、Brush、UI 和宿主回归。
- Windows /WX Release 与实际 Web Minimal Host 构建均 exit 0；source manifest 与精确提交对应。
- section-drop-browser-exact-008：实际 Chrome/WASM 的 111 个检查全通过，source revision 正确且 dirty=0。包括通过真实画板入口创建 Shape 和实际画笔 Stroke 后的指针拖放、归属、Section 跟随、Undo/Redo 和拖出；输入为合成夹具，不冒充真实设备。
- 浏览器初次 section-drop-browser-006 把实际 Brush 的 identity transform 当成 Shape 的 (20,40) 原点而失败。该新增补充 runner 的预期依照独立算术修正为同一个 pointer delta (330,220)，实际可视内容位置一致。原失败及依据在 browser-fixture-correction.json；未修改任何冻结输入或 oracle，也未放宽 tolerance。
- Python 10 项合同测试与 101 个 runtime boundary 文件检查通过。所有预检、审计、原始命令、退出码、观察结果与 hash 保留于对应目录。

## 当前测试程序

Windows 已另开空白窗口 PID **270264**，启动日志确认本实现 revision、dirty=0，首帧 accepted；不关闭已有窗口。exe 完整路径/hash 见 running-windows.json，相邻资源完整摘要见 resource-manifest.json。资源从锁定的已验证构建目录原样组装，未修改打包源码。

`out/fc08-section-drop/bin/axiom_minimal_runtime_host_windows.exe`

切 Select，先选中 Shape/Stroke，拖入 Section 内松手；再选中 Section 拖标题区域，应一起移动。拖出后不再跟随；Undo 一步恢复移动前位置和归属。人工复测仍 PHYSICAL_PENDING。持续输入日志位于 out/fc08-section-drop/manual-input.log；已提交 startup-windows.log 为稳定的启动截面。

完整 302 变体验收、独立 Gate、DEV/P34/PHYS 和 G5 性能结论不在本次通过声明中。既有两项 baseline debt 未改写为通过；本轮运行受影响的 24 项 regression，未冒充重跑无关旧矩阵。

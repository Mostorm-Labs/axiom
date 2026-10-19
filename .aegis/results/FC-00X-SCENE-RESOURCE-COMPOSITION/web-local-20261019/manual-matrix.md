# Local Web functional testing — 2026-10-19

Windows 用户汇总反馈：“经测试，各项功能正常”。原始 Windows 运行日志另存为
`windows-user-observation/native-input.log`；此反馈不替代逐项观察记录，也不改变历史 Gate/PHYS 状态。

Chrome 页面：`http://127.0.0.1:8765/ink_playground/`。
使用 Debug UI 按钮打开控件；这是现有 Ink Playground 交互入口。
P32 `RuntimeComposition` Web reference 是单独的 `/axiom_canvas_demo/` 入口，
该页当前只有小画布和 surface/pointer 接线，没有等同 Windows Minimal Host 的完整工具 UI。

| 项目 | expected | actual | result |
| --- | --- | --- | --- |
| 单指连续绘制、松手后 canonical 与 preview、连续 10 笔 | preview 跟手，位置一致，采样不逐笔降低 | 待用户观察 | PHYSICAL_PENDING |
| Brush/Marker 等预设、颜色/线宽/透明度 | 已启用 RuntimeFacade 控件生效并读回 | 待用户观察 | PHYSICAL_PENDING |
| Ink/Shape、连续 Shape/Line/Arrow、Shape 上的 Ink | 已启用工具按冻结规则工作 | 待用户确认当前 Web UI 是否启用这些工具及实际结果 | PHYSICAL_PENDING |
| Partial Eraser、Undo/Redo、选择移动缩放对齐、取消 | 沿用现有 Runtime 语义；擦除不修改 Shape | 待用户观察 | PHYSICAL_PENDING |
| 双指在 250 ms 内加入 | pending AutoIntent 进入现有双指导航/缩放；保留原有 Ink slop 判定 | 待用户观察 | PHYSICAL_PENDING |
| 第二指超过 250 ms 后落笔 | 原有 AutoIntent 第二笔书写/MultiInk | 待用户观察 | PHYSICAL_PENDING |
| 一指抬起、剩余指继续、结束后重新绘制 | 不丢 Down，不残留旧会话 | 待用户观察 | PHYSICAL_PENDING |
| capture lost、焦点/窗口切换、touch cancel、ID 复用 | 旧 pointer/session 不影响下一次操作 | 待用户观察 | PHYSICAL_PENDING |
| Debug UI 触摸、开关和工具切换 | 不穿透画布，Facade 状态一致 | 待用户观察 | PHYSICAL_PENDING |
| resize/maximize/restore/minimize、DPI/多显示器 | canonical/preview 一致，后续可绘制 | 待用户观察 | PHYSICAL_PENDING |
| idle、handoff 退休、WebGL context loss/rebind/retry | 不异常持续提交，不丢 canonical feedback | 待用户观察 | PHYSICAL_PENDING |

浏览器中“导出 Web 证据”可保存当前页面既有 trace/环境材料。
自动 CLI 检查单独记录，不能替代以上人工触摸结果。

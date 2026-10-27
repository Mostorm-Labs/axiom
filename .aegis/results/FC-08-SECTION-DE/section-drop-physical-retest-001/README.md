# Section drop — Windows 人工复测

被测实现：`68565bdd2eeff0594052c3f3f867831ff48db1ce`，窗口 PID 270264；exe SHA-256 与原运行记录绑定。

用户直接反馈：“拖入跟随、拖出均正常”。本次单独记录为：

- 拖入 Section 松手后，移动 Section 时对象跟随：PASS。
- 拖出 Section 后不再跟随：PASS。

用户没有逐一列出测试的对象种类，不将此答复扩展为每种 ObjectKind 的单独真机通过。Undo/Redo、Web、嵌套/仿射/metrics 等人工项不在本次确认中。已有自动回归仍按原精确提交记录，本次没有重新运行或扩张其结论。

原 FAIL、此前 PHYSICAL_PENDING 运行截面和实现证据保持原样；当前两项人工复测结果见 user-retest.json。没有修改源码或声明完整 PHYS/Gate 通过。

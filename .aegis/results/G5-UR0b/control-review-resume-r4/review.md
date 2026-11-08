# UR0b r4 独立复核

结论：接受两项实时输入资格验证和按规则停止的执行结果；完整 UR0b 仍为 `BLOCKED_EVIDENCE`。不能由部分 OFF 像素差异判定产品绘制错误，也不能把离线诊断改记为原 worker PASS。

- 120 Hz、240 Hz 均通过。独立重编译原始 trace 后，metrics、canonical chain、原始汇总的非 analysis 字段均一致；保持 `WORKER_COMPILE` 与 `OFFLINE_REVERIFICATION` 身份分离。
- 首项低层级半透明拖动正确性 worker 触发原有 30 秒阶段超时，退出 124；RSS 未超限，清理完成。只有 OFF 两步，ON 配对未完成；其他 5 项正确性和 30 项主矩阵未启动。
- 确认测试遗漏选中装饰：切手形工具后公开 selection 仍为 1，Runtime 会画选中轮廓，但测试传入空 HUD 范围。原 OFF1 的 16,112 个差异全部落在由归档公开 selection bounds 计算的既有 HUD band 内。此文件级诊断不更改原 FAILED 判定。
- OFF0 加入归档公开实际 bounds 后仍有 2,441 个差异。颜色与吸附提示／选中装饰吻合只是线索，尚不足以排除全部真实内容差异。测试假设移动 24/12，而提交变换实际为 24/10.505767822265625；live 画面与 post-Up reference 的适用性、装饰排除规则需要先由验证设计明确。
- 超时根因未定位到单个 await。现有约 15.1 秒和 13.8 秒的 OFF 区间同时包含绘制／等待和 RGBA 传输，不能据此断言 close/reload 卡死、GPU coverage 失败或产品交互耗时。
- 新增执行 3 项、记账 118 秒；累计 33 次、1251 秒。r4 保持 CLOSED，没有自动重试、构建、提交或推送。

独立核验：217 个新归档文件、1783 个历史归档文件及身份绑定一致；100 项 Node、56 项 Python、120 项边界检查通过。审查没有启动浏览器、原生像素 oracle 或构建。

下一步先补验证规则和最小 harness 修复范围，再授权原失败项续验。保留像素阈值、独立几何 AA oracle、30 秒阶段上限、原始失败证据和有效实时输入资格结果；不得用按差异生成的遮罩或调高超时让结果转绿。本复核只向 Aegis／P20 交接，不授权实现会话越过当前关闭窗口执行。

机器结果见 `result.json`；像素诊断见 `pixel-diagnostic.json`；时间线见 `timing-diagnostic.json`；归属与交接见 `findings.json`、`ownership-handoff.json`。

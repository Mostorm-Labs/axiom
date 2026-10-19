# Web consumer entry reconciliation — 2026-10-19

结论：上一轮打开了错误的 Web consumer。当前源码没有交付可与 Windows Minimal Host
执行同一完整人工矩阵的 Web 页面。本轮仅核查归属并建立 RED，没有修改产品代码。

## 实际入口与所有权

| 入口/目标 | 所有权路径 | 当前状态 |
| --- | --- | --- |
| Windows `axiom_minimal_runtime_host_windows` | `MinimalRuntimeHost → RuntimeComposition`；`MinimalRuntimeFacade → DebugController` | 用户已汇总反馈各项正常；保留该有效工作 |
| `/ink_playground/`，上一轮实际打开 | `InkPlaygroundHost` 直接继承 `CanvasEditingRuntime`，资格验证宏开启；旧 Web facade/DUI 接线 | 错误测试对象；用户报告不能绘制、只有 Debug UI 能打开，记录 FAIL |
| `/axiom_canvas_demo/`，P32 Web consumer | `WebSurfaceHost → RuntimeComposition`，提供 `facade()` | 正确 composition 路径，但页面仍为 G3-10 320×180 reference；完整工具/Debug UI 未接入 |

源码定位：

- `apps/minimal_runtime_host/CMakeLists.txt:16` 只有 Windows 平台可执行入口，没有 Web 可执行入口。
- `apps/ink_playground/CMakeLists.txt:22` 显式 `AXIOM_RUNTIME_QUALIFICATION=1`；Web bridge 第 32 行直接使用 `InkPlaygroundHost`。
- `apps/axiom_canvas_demo/CMakeLists.txt:80` 定义 `axiom_canvas_demo_web`，第 83 行只链接 common；没有 Web Debug UI ON 条件下的链接与绑定。
- `apps/axiom_canvas_demo/platform/web/web_surface_host.hpp:29` 暴露 `composition_.controlFacade()`；bridge/HTML 尚未使用它接入完整控件。
- `apps/axiom_canvas_demo/platform/web/index.html:3` 保留 G3-10 Reference Host 标题，第 4 行固定小画布。

冻结任务包 `implementation-context.md` 的 R4 要求 Windows/Web 使用同一 composition，
Minimal core 不链接 Playground/DUI，DUI 作为可选 sibling consumer。
`verification.lock.json:83` 要求真实 input/surface 生命周期与 consumer equivalence。
因此继续修旧 Playground 不能填补 Minimal Web 的交付缺口。

## 真实 WASM 的正确入口 RED

命令：`python out/p32-web/run_entry_audit.py`。

Chrome headless CLI 加载实际 `/axiom_canvas_demo/` HTML/JS/WASM。DOM 测试仅将
synthetic PointerEvent 无法获得的 native capture 设为 no-op；实际 pointer handler、
WASM 和 Runtime 保持原样。测试不代表人工触摸/capture 验收。

Down → Move → Up 三次均在 HTML 第 68 行调用 WASM 时抛出
`TypeError: Cannot convert ... to a BigInt`；Runtime `pointerSamples` 从 0 到 0。
第 70 行把 Number 时间戳传给 C++ `uint64_t` 参数，是已定位的 JS/WASM 入口失败，记为 FAIL。
`reference-entry-audit-result.json` 保留实际错误和值。

另存的 Playground 诊断显示输入被接收、canonical commit ordinal 增长，但读取像素为零。
这是错误入口的诊断数据，不把它提升为 Minimal Web 的根因，也不在本轮修补该路径。

## 对上一轮材料的补充限定

上一轮 `f3df28791cf00535f9035eb0ebbb1f5f1964f8d5` 的 Chrome 启动 PASS 仅适用于
旧 Playground 页。8 项源码合同检查也属于该 consumer，不能当作 Minimal Web 交互证据。
1 项原生 `G3WebSurfaceHost` 测试验证的是接线合同，不能证明浏览器绘制和 UI 等价。
原始材料不改写；本记录追加明确纠正。Windows 已验证工作、Authority、Golden、历史
Gate、PHYS、Android 均保持不变。

下一轮正确对象是已有的 `apps/axiom_canvas_demo/platform/web` composition consumer：
先修该入口的 JS/WASM RED，再核对 frame/presentation 生命周期；可选控件必须连接其
RuntimeFacade，不移植旧 Playground 业务状态。完整 Web 人工项目保持 PHYSICAL_PENDING；
已定位的入口失败和已观察的错误页绘制失败保持 FAIL。

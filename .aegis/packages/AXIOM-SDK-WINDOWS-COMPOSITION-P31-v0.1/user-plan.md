# Runtime Composition SDK 收口与验证计划

本计划把已在 Minimal Host 中运行的 Runtime 能力收拢为 C++ SDK 接入路径。近期先完成 Windows 的一个完整纵向切片：同一个 RuntimeComposition，通过受控 Windows 平台适配器支持真实窗口、双 surface、输入和 Facade；Minimal Host 与仓库外 SDK 示例分别验证。Web 随后复用同一公开边界。无需先完成一套稳定 C ABI。

审查基准：`Mostorm-Labs/axiom` 分支 `codex/sdk-cpp-first-multi-bridge`，`8e195cda25f3fb4d44e75412bc378c696d365883`。执行时重新读取当前 HEAD、工作区和已有包；若基准前进，审查增量并保留有效工作。

本文件是用户授权的实施方向和待落地接口设计，不是宣称已完成的 SDK。内部结构/命名可以结合代码调整，但必须实现本文件的边界与验收，不以仅安装头文件或只跑无窗口消费者作为完成。

## 1 当前 RED 和已有能力

- 现有安装目标安装 `canvas/runtime/runtime.hpp`，未安装 `canvas/runtime/runtime_composition.hpp`。新仓库外测试因此在 include 时失败，说明公共入口尚未接通。
- 仓库内 `RuntimeComposition` 已经拥有编辑器、Facade、资源、双 surface、输入和帧协调。其当前头包含 `CanvasEditingRuntime`、Scene、Skia provider 等，不能原样安装为普通产品头。
- 新增 `Runtime` 为每个实例新建一个 Composition 并暴露部分转发；Minimal Host 另行持有 Composition。两个类可以各自有实例，但同一画板不能同时使用两份 owner。
- 现有 Minimal Host 已有 Windows D3D12 canonical/preview provider、native input、IME、DPR 和帧反馈；Web Demo 已有 WebGL 对应链路。迁移重点是封装接线和保持身份，不重做帧/编辑算法。
- 已有安装消费者已经验证无窗口的 Section、Shape/Sticky 多选样式和 Undo/Redo；要在迁移后继续运行，不能以新测试覆盖旧测试为由删除。

## 2 目标模块与头文件

下列路径是 SDK 公开头的目标，具体新增目录按仓库 CMake 结构确定；一个公开头可以包含其他受控公开值类型头。不要为凑名称重复实现现有 Facade。

| 目标头 | 对象和使用者 | 最小职责 |
| --- | --- | --- |
| `canvas/runtime/runtime_composition.hpp` | Product Shell、示例、平台装配 | 唯一公共实例入口；构造/打开、取得同一 Facade、必要输入接入、状态、关闭；通过私有实现隐藏 Editor/Scene/Skia 和内部对象布局 |
| `canvas/runtime/runtime_facade.hpp` | Product Shell、Debug UI | 现有 typed 工具、对象、选择、History、查询和回执；仍由 Composition 内同一个 Facade owner 执行 |
| `canvas/runtime/diagnostics.hpp` | 可选 Debug UI/诊断消费者 | 只读诊断；一般产品状态、错误、回执不能仅存在此处 |
| `canvas/input/platform_input_contract.hpp` 或收窄后的等价值类型头 | 平台适配器及需要自带输入的 Host | source/pointer、时间、坐标、设备、压力、采样历史、取消及身份；不暴露 Editor |
| `canvas/platform/windows_runtime_adapter.hpp` | Windows Host；SDK 公开平台适配目标 | 接受已有 Composition 和 HWND/窗口相关事实；装配现有 D3D12 双 surface 和原生输入/执行上下文；处理 resize/DPI/生命周期及真实反馈 |
| `canvas/platform/web_runtime_adapter.hpp` 或 JS/TS 接入文件 | Web Host；后续包 | 接受同一个 Composition/实例及 canvas，复用现有 WebGL/input/rAF/IME 链路 |

Windows 头中只公开必要的原生窗口类型或受约束的 opaque handle。Skia、`SkSurface`、Swapchain、Editor、Scene、OperationEngine、内部 MultiSelectionPort 和原始内部 provider 不能成为 Product Shell 公共头的传递 include。`RuntimeFacade` 当前使用的一部分 Semantic 值类型需要以白名单方式判定哪些是外部值类型；不能直接安装整个 semantic 目录当作隔离已完成。

`RuntimeComposition` 内部可用 Pimpl、私有实现文件和受控平台端口承载已有逻辑。平台适配库可以使用与 Runtime 同版本构建的私有接口；普通 Shell 不包含这些私有头。C++ 当前按约定的编译器、CRT 和版本使用，不宣称稳定跨编译器二进制 ABI。

Windows SDK CMake 目标建议 `Axiom::Runtime`、`Axiom::PlatformWindows`；Debug UI 按独立可选目标提供。Web 目标在其实现包内确定。目标名称如果与已接受仓库规范不一致，以该规范为准并记录映射。

## 3 所有权与调用顺序

```text
Product Shell / Minimal Host
  1. 创建 RuntimeComposition（唯一 owner）
  2. 创建 WindowsRuntimeAdapter，绑定此 Composition 和宿主 HWND
  3. 如需 Debug UI，将 composition.controlFacade() 及只读 diagnostics 接入
  4. 窗口消息交给 adapter：输入、DPI/size、帧机会、lost/restore
  5. toolbar/Debug UI 通过同一 Facade 提交意图并读取真实回执
  6. 停止输入和新任务 → 解除 Debug UI → adapter 停止和释放平台资源
  7. 完成 Runtime 关闭屏障 → 销毁 Composition
```

这不是新的产品 API 源代码样例；方法细节以实现的公开头为准。Composition 和 adapter 的引用/析构次序、owner thread、回调重入和关闭后迟到反馈规则必须写进接口文档。不得通过“把用户新建的 Runtime 绑定到旧 Composition”而保留两个编辑 owner。

Canonical/Preview 是两个受同一 Runtime 协调的角色。Runtime 负责帧需求、工作身份、旧反馈拒绝和交接退休；Windows adapter 负责已有 provider、GPU 线程、执行与真实呈现反馈；Host 不直接判断 preview 的退休。平台资源需遵循现有所有权契约，不让可见 backbuffer 被两个不同 owner 共同持有。

Debug UI 可选，绑定相同 Facade 与 Composition 的 diagnostics；关闭 Debug UI 时工具、输入、绘制、状态和关闭仍能运行。

## 4 Minimal Host 和 SDK 示例各做什么

### Minimal Host：项目内真实画板与行为回归

继续使用 `apps/minimal_runtime_host`，不再创建第二个应用以替代其职责。让 Windows 主程序仅使用公共 Composition/Facade、Windows adapter、可选 Debug UI 接口；可保留窗口类、菜单、资源选择器等纯 app 代码。

Host 不应 include 或调用 `canvas/interaction/...`、`canvas/scene/...`、`SkSurface`、`SkiaSurfaceProvider`、`MultiSelectionPort`、内部帧状态或 qualification 入口；相关 provider 与特定平台/GPU 代码迁至适配器内部。特殊测试确需白盒时单独放在测试 target，不能借宏使普通 Host 越界。

最低行为回归：画笔/Shape、实时 Preview 和松手交接、选择与多选描边宽度/Undo/Redo、partial eraser 只作用已支持笔迹、DPI/resize/maximize、source cancel/关闭；Debug UI 开关均可用。同一操作从 toolbar/Debug UI 读到的是同一个对象与回执。复用现有 D3D12 和选择测试，不把旧测试替换成 mock。

### SDK 示例：仓库外真实安装消费者

需要一个**小型 Windows SDK app**，它证明独立项目能用安装包创建可见画板；这不是第二个产品或第二套绘图实现。示例源码可放 `samples/sdk/windows/` 或现有 `verification/sdk_cpp_install/consumer/`，验证时复制到仓库外全新目录，通过 `find_package` 链接安装后的 Runtime 和平台适配目标；不 add_subdirectory 仓库、不引用 worktree 头/库。

示例只负责创建窗口、创建一个 Composition、绑定 Windows adapter、接入基本命令及关闭；它不能内置 Runtime/Editor 的实现或依赖 Minimal Host 私有源文件。Debug UI 关闭版本必须可编译运行；如本阶段提供独立 Debug UI target，再验证开启后与同一实例交互。若暂未可安装 Debug UI，保留 Minimal Host 的同实例 Debug UI 验证并明确外部安装版 Debug UI 未交付。

自动化最少验证：真实安装、移动/解压后 configure/build/link；实际创建窗口与 canonical/preview provider，真实输入链创建/选择对象，检查 Facade 回执和 semantic readback；观察真实平台提交/反馈并复验 resize/DPI/关闭；不允许只用无窗口 `bindSurface(width,height)` 判定完整产品 SDK。实际 scanout 与合成输入/真实人手输入各自记录，不互相替代。

现有 3 个 headless 消费者继续作为更快的语义与安装测试；把它们迁移到 `RuntimeComposition` 并保留原断言。新 SDK app 是额外的端到端接线验证，不复制笔迹/对象引擎。

安装依赖必须明确：当前 Windows x64、MSVC、Release `/MT` 和锁定 Semantic/Skia SDK 限制保留。允许 SDK 包明确声明并从持久 SDK Store 解析锁定依赖，也可随包合法分发；验证不能隐式指向另一个开发 worktree，需证明从隔离目录按文档安装可用。不得源编已发布依赖。

## 5 实施顺序与退出条件

按连续纵向切片执行，中间构建/测试成功是检查点，不能把工作停在头文件 RED；冲突和实际环境障碍明确返回。

1. **确认生产入口与所有权。** 检查当前分支和任务包；列出现有 Composition、Runtime 包装类、Facade、Windows/Web 实际调用。固定一个 Composition owner、callback/thread/close 顺序及平台私有端口。保留已有改动和用户测试结果。
2. **实现公共头及兼容过渡。** 让可安装 `runtime_composition.hpp` 不依赖内部类型；使用 Pimpl 或同等机制保留原有实现 owner。外部 headless 单 Composition RED 转 GREEN，原 3 项消费者迁移到相同 Composition 并继续通过。暂时可以用内部迁移 shim，但不公开第二个产品入口。
3. **提取 Windows adapter。** 从现有 Minimal Host 迁移 D3D12 provider、双 surface、input/IME/resize/反馈的接线，明确 adapter 与 Runtime 的受控接口和销毁顺序；Runtime 保持既有帧身份和退休判断。
4. **迁移 Minimal Host。** 使用公共 Composition + adapter + Facade；Debug UI on/off 通过同一实例。原行为和已有回归不下降。更新源码引用、头边界与构建关系，产品代码不引用内部编辑/渲染头。
5. **SDK Windows app 外部验证。** 安装包移动到仓库外，单独 configure/build/link/run；有真实窗口及双 surface，验证真实请求、编辑/读回、预览与反馈、resize/close。同实例测试确保 SDK controls 与 Debug UI/画板作用于同一对象；SDK app 不含自建编辑逻辑。
6. **移除重复 Runtime 类。** 在已迁移的所有正式消费者、CMake 安装、文档和测试不再需要后，删除 `runtime.hpp` 的 Runtime 包装及对应实现/target；复跑既有外部测试、Host 与边界测试。若确有仍依赖的消费者，完成迁移或在确切契约冲突时返回，不能仅留下长期双入口。
7. **独立交付与 Web 后续。** 给出 Windows 代码、可重复命令、生产 SHA、产品 artifact、普通 public/平台 public/private 头清单和限制。Web 独立切片把现有 Web 适配到同一 Composition 和 platform 契约，提供实际浏览器消费者；Android/Apple 不计入本切片通过。

## 6 必须保留的回归和失败案例

- 单实例身份：同一 Composition 下 Product Facade/Debug UI 编辑同一个对象；两个 Composition 的对象、surface 和工作反馈相互隔离。
- 同一 source 的 pointer 重用：捕获接触身份、source 重绑、旧 Up/Move/Cancel 晚到，验证旧事件不会改变新接触；当前重排探针只是条件性证据，真实适配层如何产生/约束序列需一并说明，不能将其冒充真机复现。
- Preview A/B 与 Canonical A/B 的反馈必须绑定原有工作身份；重复、跨实例、旧代际、resize/lost/restore 和 failed submit 均不能误退休或污染新帧。平台报告真实可证明的 presented 程度。
- Surface 配对和 metrics 原子变化，失败保持已接受状态或按已有合同进入 Lost；close 是终态，迟到输入/回调不能复活它。适配器销毁必须不留对 Composition 的悬空回调。
- 保留 permission unknown fail-closed、partial/eligible、逐对象描边 width 保留原色/cap/join/dash、一次 Undo/Redo；只读属性读取能力需根据实际权限合同处理，不私自放宽访问。
- 保留现有 3 个外部消费者和 SDKCppPublicMulti/Composition/Frame 回归。迁移前后分别核验是否命中真实 owner；绑定到构建 commit 和日志，不能用单纯的头检查证明可运行。
- 测试入口要有真实 source 和可执行命令，新增文件是输出。执行环境必须使用锁定预编译 SDK。D07 的 post-commit OOM 仍独立未解决；不以本切片 PASS 改写其状态。

## 7 明确不挤进此 Windows 切片

Document 打开/恢复/保存、本地 Operation 输出与 close publication barrier、资源完整注入/取消、Web 公开安装包、Android/iOS 和完整稳定 C ABI 仍属于后续独立能力。Windows adapter 为运行真实画板所需的现有字体加载和必要资源应接通；不以“以后做资源”放弃本切片的真实窗口和字体验证。现有 C ABI V1 权威和 manifest 原样保留，是否扩展须单独核对。

Windows 真实窗口与自动输入是自动化证据；真实笔、多指、IME 和用户主观观察单独标记 `PHYSICAL_PENDING`，不能把自动窗口/Chrome 回归写成真机 PASS。

## 8 交付与当前工作衔接

执行分支：继续 `codex/sdk-cpp-first-multi-bridge`，先检查 HEAD 和未提交修改；不 reset、不覆盖、不 force push、不直接合并 main。当前 `8e195cda` 的 RED 和既有已通过消费者均保留。

根据现行 Authority/任务包完成预检和适用的执行授权；新接口/实现若触及冻结合同，则把完整收口要求一次性补进相应包，分别固定允许路径、测试和真实窗口 oracle，避免在 P33 反复追加局部目标。已有 accepted 语义和历史 Gate 不改变；需要新 Semantic/Operation 时返回对应 owner。

完成时返回精确 starting_revision、result_revision、reviewer 可解析 materialized_ref、公开头/target 清单、外部 SDK 示例和日志、Windows/Web 实际覆盖与 `PHYSICAL_PENDING`、Document/资源/D07 剩余项。不得自行声称独立审查或 Gate PASS。

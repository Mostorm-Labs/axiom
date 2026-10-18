# S1/S2 独立代码复验 — 2026-10-19

结论：主链显著收敛，仍有两处 IMPLEMENTATION_DEFECT。建议保持 BLOCKED_IMPLEMENTATION，待这两项修复后再交人类确认。不修改 Gate，不作物理验收结论。

## 精确基准与范围

- Repository: Mostorm-Labs/axiom，origin 已核对。
- Branch: codex/fc00x-scene-resource-composition-p32-f08-resume。
- starting/anchor: ae1a2e79b1f4e2eb36f81655f6819cc4a8a8173a。
- 产品实现: 7453884345a175f6aebad52225cfafaf337f2f3e。
- 证据材料: 7d22700a5020974c38cff690a530a23abbcd8d21；复验开始时 git ls-remote 与本地 HEAD 相同。
- 74538843 → 7d22700a 的 diff 只有 .aegis/results 证据；没有产品源码变化。
- 审查依照人类授权的 s1-instance-s2-lifecycle-repair-handoff.txt 的十组固定矩阵。没有引入 S4、旧 Playground 或命名清理。
- 所有构建/探针在 D:/axreview/fc00x-20261019/s1-s2-independent-review-v1/source 的精确 git archive 副本上运行。开发工作树未改，原 imgui.ini 保留。
- 使用既有发布 Skia SDK 与共享轻量依赖；独立聚焦构建 codec OFF，没有重建 SDK。Clang-cl 复用原审查的历史 warning 降级参数，非严格零警告结论。

## 独立执行结果

1. 从归档源码重新 configure/build，221 步聚焦构建 exit0：configure.log、build.log。
2. 固定十项及相关保护测试共24项重新执行，全通过、无 failure：ctest-results.xml，build/Testing/Temporary/LastTest.log。
3. 独立构建真实 Windows Minimal exe 和 D3D12 paired binding test，exit0；后者执行1项，无失败、无skip：build-native.log、native-test.log、native-test.xml。
   合计实际独立执行25项通过。
4. 新编译 public-only boundary_probe.cpp，连接本轮独立库：compile.log/link.log exit0，程序exit2；exit2准确表示下面两个期望行为未满足，不能写成“探针全通过”。
5. 独立复核46个 indexed evidence文件和8个本地产物，全部长度/SHA-256匹配：audit-results.json。
6. 检查 authorities/gates/evidence/integrations、两项SDK锁及verification/corpus指定保护路径，没有新增变化。

## 已验证修复与保留项

- PreviewWork 实例nonce、view、寿命、work/content/surface/metrics身份生效；跨实例token不能确认另一实例，当前合法token仍可完成。
- 同实例迟到A不误确认Move/resize后的B；纯失败保留相同token重试；重复回执无修改效果。
- 旧控制请求在Lost/rebind/direct-rebind后无效，fresh控制正常；控制时钟与300次owner pump后Debug UI创建链正常。
- 表面注册不再直接激活候选，registerSource不能跳过正式bind。
- logical100x80/physical200x160/DPR2的Runtime与WebSurfaceHost accepted metrics一致；metrics-only更新独立推进，重复更新幂等。
- 真实Raster像素/公共创建选择移动、DPR1/1.25/2、选择chrome与brush像素相关正式断言通过。
- 第二角色准备失败后的统一Lost/恢复，以及候选首阶段失败保留旧绑定等已有正式断言通过。
- 补充探针注入第二角色activateBinding失败，验证统一Lost且随后恢复、fresh frame正常。
- 真实D3D12测试覆盖预备provider采用、metrics-only复用、破坏性resize后第二角色失败、Lost/恢复、替换与fresh frame。
- 准备期间单个resize/lost/close中断、pending Brush跨preview provider替换两种反馈顺序，已有正式断言通过。
- Web回执mapping-full无状态污染，平台A/B乱序不误确认同实例新帧。
- 原本共同Facade、公共owner pump、Debug UI真实创建状态、S3私有capture/render/ack检查保持正常。
- 以上仅针对本精确分支；不表示main已交付。

## R1 — [P2] Canonical反馈仍缺实例/寿命归属

分类：FROZEN_REQUIREMENT_FAILURE / IMPLEMENTATION_DEFECT，对应S1工作回执归属和固定矩阵9。已修的是PreviewWork通道；不能把Preview隔离测试等同整个帧回执链已隔离。

位置：
- runtime/composition/src/runtime_composition.cpp:442-444
- runtime/interaction/src/canvas_editing_runtime.cpp:3124-3140
- runtime/render/include/canvas/render/presentation_tracker.hpp:45-54
- runtime/render/include/canvas/render/runtime_frame_coordinator.hpp:12-19

真实公共口复现（非qualification editor）：
1. 创建A、B两个RuntimeComposition，使用相同允许复用的ViewId=1，分别open/bind128、注册source1、启用deferred presentation。
2. 两边各通过公共输入画一个笔迹Down/Move/Up。
3. serviceFrameOpportunity得到两者Canonical及Preview工作。因Canonical计数和绑定计数均为实例内初值，Canonical identity比较相等。
4. 只正确确认B自己的PreviewWork。
5. 将A的Canonical PresentedFeedback交给B的receivePlatformPresented。
6. B接受A的反馈，清除自己的submitted frame，并退休自己的preview；没有提供B的真实Canonical呈现反馈。

原始输出：
OBSERVED canonical cross-instance: equalIdentity=1 acceptedAinB=1 BsubmittedAfter=0 previewBefore=1 previewAfter=0 pendingBefore=1

实际影响：跨实例/实例重建后的迟到回调被错误路由时，未完成真实呈现的Canonical被误判为已呈现，preview提前消失/交接错误。当前单窗口正常路由不等于已发生该现象；这是公开接口的隔离缺口。

现有独立覆盖与漏检原因：25项正式测试通过；新增实例测试只交换PreviewWork，没有交换Canonical工作反馈。补充probe独立连接真实Runtime，并观察pending Brush退休，不仅比较字段或检查枚举。

最小修复方向：让共同工作/完成资格覆盖两角色，Canonical完成同样验证Runtime实例、寿命与submission归属，或采用等价的不可碰撞工作关联。平台原样回传关联身份；不要依赖复用ViewId/本地FrameId，不把隔离正确性转嫁给Host。保留稳定C ABI和canonical时钟语义，无需另造Facade或多窗口产品。补同view实例、close/recreate、跨实例旧Canonical反馈负向与当前正确Canonical正向，以及两种Brush反馈顺序。

## R2 — [P2] 重入resize可覆写Closed终态

分类：FROZEN_REQUIREMENT_FAILURE / IMPLEMENTATION_DEFECT，对应固定矩阵7“准备期间事件使旧候选无修改效果”和close终态。

位置：
- runtime/composition/src/runtime_composition.cpp:117-124
- runtime/composition/src/runtime_composition.cpp:492（close方法）

真实Runtime + Raster provider，仅在resize边界同步注入事件：
1. Runtime已Ready，注册并接受带resize hook的真实Raster provider包装。
2. 外层resize进入prepare，surfaceTransitionActive=true。
3. hook先调用公共close()，得到Closed（枚举7）。
4. 同hook再调用公共resizeSurface；该方法先进入surfaceTransitionActive分支，无视Closed，执行state_=Lost与surfaceBindingChanged。
5. 内层resize返回false，外层resize返回false，但最终状态为Lost（枚举3），不是Closed。

原始输出：
OBSERVED reentrant after close state=7
OBSERVED reentrant resize after close accepted=0 state=3
OBSERVED outer after close+resize: result=0 state=3 closedEnum=7 available=0

影响：关闭后的迟到resize会改变终态和身份；消费者可能继续按Lost发起恢复并永远失败，因为内部editor已closed。没有观察到已关闭Runtime恢复编辑，不能将本结论夸大为能继续改Document。

现有覆盖与漏检原因：正式测试分别验证单个close、单个resize或单个lost中断，没有验证close后同一prepare栈内迟到resize的组合。补充probe命中同一公共生命周期路径。

最小修复方向：owner/state准入先保护Closed/Stopping/Quiescent等终态，再处理重入abort；重入路径只能取消原事务，不能覆写已经提交的更终结状态或再次推进身份。补close→resize、close→lost等迟到事件组合，确认Closed保持、旧候选不发布、所有编辑/工作拒绝；原单事件恢复正向继续通过。

## 范围与终点

上述两项是固定实例/寿命及prepare期间关闭规则的遗漏组合，未要求新对象、S4控制接线、动态Registry、改名或新的平台线程。
目前给出独立审查结果，生产代码与正式测试未改；没有自动发起下一轮产品修改。由人类确认后再安排窄修复。

Windows/Web人工可见呈现、DPI/浏览器缩放、真实原生输入继续PHYSICAL_PENDING；Android/APK DEFERRED。
Windows独立exe:
D:/axreview/fc00x-20261019/s1-s2-independent-review-v1/build/apps/minimal_runtime_host/axiom_minimal_runtime_host_windows.exe
这次只编译该exe和运行隐藏窗口的资源测试，未把它写成手动真机UI通过。Web本轮独立核对已有产物/日志与接线源码，未重新执行浏览器页面，不能宣称浏览器验证完成。

## 可解析证据

- boundary_probe.cpp：独立public-only检查、两项失败oracle。
- output.log：新编译探针运行exit2的完整stdout/stderr。
- compile.log / link.log：探针编译链接exit0。
- configure_build.py / configure.log / build.log：独立构建入口与日志。
- build_native.py / build-native.log / native-test.xml / native-test.log：真实D3D12构建执行。
- ctest-results.xml：独立24项回归。
- audit.py / audit-results.json：证据与产物字节校验。
- baseline.json：精确归档身份。


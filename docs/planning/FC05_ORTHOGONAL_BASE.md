# FC05 正交连接基础版

范围依据：用户在本会话采纳“有界、可取消、多障碍物”的基础版方案，要求先取得
Windows/Web 正确性与耗时基线，再推进套索与 G5。承接现有 FC-05-P31 包、
FC05_P20_FREEZE.md 与 FC05_ROUTING_CONTRACT.md；保留已有 Sticky 完成标签。
本记录不声明 FC05 Gate、独立审查或 PHYS 通过。

## 使用入口

F12 → Control → Connector / Sticky：

- **New connector routing**：Straight / Orthogonal (avoid objects)，控制后续
  Line、Arrow 及选中对象连接点创建的连线。
- **Selected connector routing**：选中已有 Connector 后切换类型，一次 History，
  可 Undo/Redo。普通消费者使用同一个 RuntimeFacade::setConnectorRouting。
- **Route / Search**：显示实际派生状态与搜索工作计数；普通消费者可按完整对象 ID
  调用 readConnectorState，不依赖 Debug UI 或选中状态。

验证：创建两张 Sticky 或 Shape 并连接，将第三个对象放在线路中间，切正交；
拖动连接目标与障碍物，再测试 Undo/Redo、取消拖动、直线/折线往返切换。
成功线路各段水平或垂直，并绕开对象。Pending、NoRouteInDomain、BudgetExceeded、
InvalidEndpoint 等显示橙色虚线诊断弦，不承诺该弦避障，不冒充成功路径。

## 实现边界

- Scene 中的 ConnectorRouteJob 使用 f64 的坐标压缩正交图与方向状态 A*；
  距离、折点及完整路径字典序决定候选，固定次序使结果可复现。
- Rectangle/Ellipse/Triangle/Sticky/Image 使用既有端点合同。Auto 先解目标 local ray；
  两端均附着时使用另一对象中心，避免 perimeter 的递归依赖。Triangle 四端口在真实
  边界上；角点允许任一支持外法线的出线方向。
- 障碍物是可见 Shape/Image/Sticky 的保守 world AABB；文本和 Connector 不作为障碍。
  安全间距包含 8 单位、Connector 半描边和障碍物描边外扩，搜索边界向外量化到 1/64。
  起终点的出线短段只豁免其自己的对象，不能穿过其他障碍物。
- 依次搜索 64、256、1024 单位扩展域。无路径和工作预算耗尽分开返回；不以穿模折线
  或旧成功路径作为当前结果。新建/切换失败不留下部分对象或 History。
- 单个作业上限为 256 个相关障碍物、8192 个保留/展开状态、65536 个碰撞谓词、
  8192 个候选记录检查及 256 个简化顶点。每 quantum 最多 1024 次记录检查/展开、
  8192 次谓词；总预算跨 quantum、域和出线尝试累计，不在重试时清零。
- 共享 Runtime owner pump 最多推进四个 quantum；预览优先取至多两个，剩余给
  canonical。各保留一个活动搜索 arena。新 pointer 位置替换旧预览；代次变化、取消、
  loss/close 清理旧作业。失败后只随输入变化重试，不空闲自旋。
- 路由结果通过现有 Scene prepared publication 一起更新绘制、bounds、spatial、hit。
  路由刷新不写 ConnectorContent、Operation 或 Undo；箭头使用最后一段方向。
  Windows/Web 的宿主不持有另一套路由器。
- 公共一次性 create/set-routing 的合法性预检使用同一有限预算求解；持续预览和
  已接受对象变更后的重路由按 owner quantum 推进。

## 验证与证据

目录：`docs/planning/fc05/evidence/orthogonal-base/`。

- RED：旧算法在共线端点被阻挡、两个单拐角均被挡的用例中穿过障碍物。
- 路由回归：独立线段/矩形内部判定、独立整数网格最短路、端点/变换、预算与取消。
- 公共消费者：创建/切换、失败无副作用、第三障碍物插入/删除、派生刷新不增加 History、
  Undo/Redo、关闭拒绝；Debug UI 使用同一接口作对照。
- 预览回归：拖动端点跟随临时 transform，canonical 保持原像，Cancel 清理预览。
- probe 运行同一 40 项 native/WASM 语料，比较状态、路径与计数；另测
  100/20、1000/200、10000/1000 的对象/连线规模与 Ready/BudgetExceeded 分布。
  时间单位为微秒，是单机求解基线，不等同于端到端帧延迟或 G5 验收。

具体命令、编译器、结果、源码字节摘要由本目录的验证记录绑定；核心 WASM probe 与
完整 Web Host 构建分别记录，不能把前者当作浏览器/平台接线通过。

## 基础版限制及后续

保守 AABB 可能使三角形/椭圆周围的路线更长。当前为安全起见按场景输入变更使路由失效，
仍沿用既有完整 Scene 投影与不可变输入快照；候选枚举有上限，未声称完成大型空间索引
查询、细粒度缓存、跨对象全局布局、交叉优化或 G5。密集场景可以明确超预算，不能宣称
支持任意数量障碍物。曲线、手工折点、平行线排布继续不在本次范围。

真实 Windows/Web 鼠标、触摸和视觉质量需要单独人工确认，未观察项保持 PHYSICAL_PENDING。

## 本轮结果

实现提交：`6840cb95ed4c732f4579ee2f69065ac984b8f171`。
Windows D3D12 Host 与完整 Web Host 构建通过；40 项 native/WASM 核心语料完全一致。
Chrome 13 组基础回归、7 组文本回归通过；Python 17 项通过、0 skipped。
开启断言的 CTest 对照矩阵为 27 PASS、1 FAIL、0 skipped。失败为未修改的旧 Image
摘要期望缺少 FC03 content_mode/source 字段；保留失败与诊断，不更新历史期望。

本机 i5-10400 的 100 对象 / 20 连线语料全部 Ready，单作业 p95：native 1.008 ms、
Node/WASM 1.544 ms。密集 1000 / 10000 对象语料全部触发 256 相关障碍物上限，
不将其短耗时误记为吞吐能力。详见 validation.json 与 benchmark JSON。

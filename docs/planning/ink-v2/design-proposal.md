# Ink v2 实施设计提案

状态：为实施包准备的具体技术提案；用户需求见 requirements.md。本文没有改写
现有 Semantic authority、分配新的 wire tag 或宣布执行授权。启动编码前的
ImplementationDesignPreflight 必须把新载体、能力与参数默认值落实成明确版本增量。

## 1. 所有权与提交路径

平台 collector 保留输入来源、sequence、时间和能力事实；既有 Interaction owner
冻结一次笔画的工具配置；Ink 负责信号处理与几何/材质计划；Runtime adapter 构造
唯一 AddStroke；OperationEngine 原子应用；Scene/Render 管理可丢弃派生数据。
SDK/common 只解码与适配，Web/Windows 公开拥有明确寿命的 DTO。激光状态属于既有
Runtime 的瞬态交互通路，不构造 AddStroke，不增加平行文档/History owner。

`prepare -> validate/reserve -> commit -> publish -> present` 继续沿既有 owner；
预览失败、预测替换、取消、数据端口过载、重复 seal 与 close 使用现有回执语义。
远端 packet 的 Applied 不代表本地持久化或云端确认。

## 2. 默认模拟压力与真实压力扩展

- 公开 `pressureSource = simulated | device`，默认永远为 simulated；第一版不增加
  会按设备悄悄变化的 auto 模式。模式在 begin 冻结，只影响之后开始的笔画。
- 浏览器提供静态 0.5 或 0 的 pressure 不是硬件压感证据。simulated 忽略这些数值；
  device 必须具有来源能力事实并逐样本检查 finite [0,1]。显式 0 不等于缺失。
- device 请求缺少能力或中途缺失有效压力时，整批拒绝、不部分接受、不偷偷换模式；
  调用方可在新笔画显式改回 simulated。当前无压力设备必须能完成全部六笔型流程。
- 信号处理接收 confirmed 时间与坐标，时间非递减，sequence 严格递增。相同时间戳
  合并待处理区间，直到获得正 dt；不得每个重复时间戳虚构 1ms。非法回退时间拒绝。
  稳定样本速度使用实际 elapsed time，不能只把相邻点距离当时间速度。
- 提议的模拟器 `speed-pressure-v1`：由速度的 smoothstep 映射产生目标压力，再用
  elapsed-time 指数滤波。基准参数为 pMin=0.20、pMax=0.80、vLow=80、vHigh=1200
  画布单位/秒、tau=32ms；初始压力 0.5。目标为
  `pTarget = pMax-(pMax-pMin)*smoothstep(clamp((v-vLow)/(vHigh-vLow),0,1))`。
  这些常量进入版本化配置，视觉标定若改变它们，要在新版本配置中记录。
- 先对原始 confirmed 时间轨迹求压力，再按弧长重采样；预测使用可丢弃的分支，
  不推进 confirmed 压力状态、随机序列或累计距离。
- 规则圆头宽度恒定；手绘圆头把已解析压力交给变宽轮廓。禁止在此后再次开启
  perfect-freehand 的压力模拟，避免同一信号被模拟两次。
- 持久化的是量化后的 **effective pressure**，不是为了重放再次模拟的原始事件。
  模拟/设备 provenance 只描述来源，不改变读取端的压力解释。实时预览也先进行
  相同 canonical normalization，防止松手和恢复时跳变。
- timestamp、倾斜能力保留在 Runtime 输入契约；新紧凑记录不强制存全量事件时间。
  未来依赖倾斜/速度的参数必须固化其有效信号或实际输出，不能靠机器时钟重算。

## 3. 紧凑笔画数据：存中心线，不存展开 dab

提议新增带显式能力要求的 `CompactStrokeV1`（实现时确定非冲突 kind/version、
ObjectContent 分支、wire tag 与 Snapshot 要求）：

| 项 | 记录内容 |
| --- | --- |
| identity/placement/transform | 复用 ObjectRecord 的唯一对象身份和变换 |
| brush snapshot | 有效 family、算法/信号/材质版本、参数、颜色、整笔透明度 |
| spine | 唯一一份中心线 x/y，packed binary64；count 和字节序明确 |
| effective pressure | 与中心线等长的 uint16 LE 数组，解码 p=q/65535 |
| resources | 去重资源 ID、内容 SHA-256、解码版本、采样规则；字节通过既有资源通道 |
| seed/material basis | 稳定 uint64 seed、笔画局部纹理坐标基准 |

这是字段级提案，不是允许调用方直接构造 Operation 的新 SDK。

- 每个点只存一次位置和一次有效压力。不并存全量 raw samples、另一个 outline、
  dab 数组、逐 dab size/rotation/opacity、base64 图片、SVG 路径字符串或每笔截图。
- 用户的“固定算法”解释为固定 family + evaluator/material version，不是每笔固定
  一个 bitmap。总空间仍随必要中心线长度增长，不承诺无限长笔画 O(1) 存储。
- 中心线首先在明确误差预算下按路径距离重采样并保留急转/短点；不能仅每 N 个
  DOM 事件保留一个。建议 spacing=max(0.25,min(2,size/8))，几何最大偏差
  epsilon=min(0.25,size/32) 画布单位；压力折点保留至误差 <=0.02。实际 verifier
  检查这些误差，不能只检查点数减少。
- uint16 压力在存储前归一化并用于实时求值，量化误差 <=1/65535。坐标第一版保持
  binary64，避免为压缩新引入不可控的坐标量化；不以 gzip 后体积掩盖规范数据膨胀。
- count、长度、有限坐标、范围、版本与资源完整性在分配/修改文档前校验。点数上限
  取现有协议硬上限与 262144 的较小值，超限明确拒绝，不截断/换成粗劣笔画。
- 圆头、扁头及纹理都从同一紧凑骨架派生。旧 (kind,version) 的 outline/dab 记录
  仍按旧语义读取/绘制；不自动重写旧文档，不把旧 brush version 原地改义。
- 新版回放从固化中心线、压力、版本、资源生成；不依赖 live preset catalog、原始
  DOM 事件频率或当前时钟。旧的“已存 outline 无需 evaluator”保证仍属于旧数据分支，
  新分支明确要求对应 evaluator 能力，不提供错版本近似回退。

## 4. 固定纹理算法与性能

新笔型主路径为“连续可变宽度 coverage + 固定材质场”，不是沿路径制造并保存
大量印章。CPU 提供确定性的参考/几何结果，现有 Skia Shader/RuntimeEffect 能力
承载纹理计算；使用接受的 Skia SDK 验证能力，不为此重建依赖。

- 马克笔：固定扁头扫掠形成连续 coverage，笔头角度和扁率真实改变几何。
- 粉笔：连续 coverage 中以纸纹与稳定噪声调制孔隙/边缘，压力控制局部覆盖和宽度。
- 蜡笔：复用 coverage/纸纹基础，采用更密实的覆盖/阈值曲线和沉积参数；必须通过
  相同轨迹的视觉样例证明与粉笔不同，不能只是颜色或预设名字不同。
- 水彩 Lite：coverage 的内部变化 + 边缘距离带颜料增强 + 纸纹，整体半透明合成；
  跨笔叠色。无画布拾色、时间扩散、持续干燥或全画布依赖。
- 固定纹理坐标随对象局部材料基准变换，重绘、移动、旋转或缩放不重新抽粒子。
  各随机域分离；按稳定路径/分段身份取值，不取依赖 append 次数的 RNG。
- 区分 flow/coverage 与 whole-stroke opacity；整笔透明度只在合成处应用一次。
  同一笔内部不因事件更密而额外变黑；不同笔画按明确定义的 SrcOver 叠色。
- 不对整条超大笔画的 AABB 分配巨型 bitmap；使用可见裁剪和有界的临时 tile。
  新派生缓存每 Runtime 合计预算 32MiB，临时 tile 工作集 <=16MiB，资源缓存沿
  既有 owner。淘汰后按相同输入重建；派生缓存不写入 Snapshot/操作/资源清单。
- 增量求值保留 confirmed 状态、稳定前缀/可替换尾部或分块结构，不反复复制全部
  历史。不能未经证明硬截最后固定 N 点而破坏 reference 行为。seal 可完整求值一次。
- 纹理显示允许生成有界的临时批次；调试展开 dab 仅用于对照，不能进入正式新写入
  或形成完整常驻影子列表。轮廓/coverage、预览、命中和导出共用同一算法解释。

## 5. 六笔型及消费者行为

默认 source=simulated；规则圆头只禁用宽度变化，手绘圆头使用已解析压力。
真实 device 模式用合成 capability+pressure trace 做入口验证，不声称完成硬件验收。
马克笔/粉笔/蜡笔/水彩有效参数均固定在 begin 的 snapshot；改工具配置影响下一笔。

激光笔保留位置和单调时间，仅在 Runtime 瞬态层管理拖尾与失效。提议默认 fade=1000ms、
最大空间尾长=300 画布单位、颜色/宽度可调；有界环形缓冲、失焦/关闭清理。
它不增加对象数，不生成 Operation、Undo、Snapshot 或普通图片导出内容。未来协作
指示应使用 presence 通道，不挤进规范笔画操作。

公开 SDK 提供 brush config/readback/presets 及既有输入路径；不提供 raw shader、
GPU、可变文档或 brush 内部对象。64-bit ID/seed/revision 使用既有无损字符串。
JSON/C POD ABI 的 buffer 所有权、线程、重入和错误语义维持既有约定。

## 6. 定版点与禁止项

编码前把新载体编号、参数默认、版本/capability 规则写入当前增量契约并冻结；本提案
本身不抢占历史注册号。禁止让执行者把范围外产品决策伪装为参数调优。
不开发任意节点图/刷包导入器，不新增云同步协议，不删除旧读取能力，不伪报 Gate。


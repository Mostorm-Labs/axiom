# UR1c2：canonical / Direct 共享 prepared body

用户已授权 UR1c1 通过后进入此步。控制审查25d28035已接受 UR1c1 的 owner/index/source/真实 Web 共享证明；不得再把旧缺口当作待实现，也不得改旧 BLOCKED 原件。

当前 owner 是 SkiaRenderer 内的 PreparedGeometryCache + SourceRetentionPool + ResourceBudgetManager。canonical `SkiaRenderCache::prepare` 已有有界 coverage job，但 admission、CompactStrokeRasterJob 构造仍各扫 knots 求support/length，每个对象建立自身 contour/checkpoints。普通 Direct shared mapper 在 skia_headless_backend.cpp 的 compact 分支仍给 drawCompactStrokeToSkCanvas 传 nullptr prepared，damage-miss也没有取得共享body。已有 ready body不能自动生效。

本步要把已存在的一份不可变 body 真正交给这些消费者，同时解决 cold-first-use。不能只是把同步 get/build 插到新叶子：现有 `build/buildReplay`、distance/support/path/group 初始化都可能全量走历史。应在同一 service 内区分 lookup-only 和可恢复 request/advance，body-job仍在 owner线程；同源请求共享未完成工作，按当前 frame allowance 推进，不给每个副本另开8ms。新编译advance沿用32knot量级，256knot checkpoint可分段构造/重放，源和结果按既有配额/lease计费。

Ready body供实际canonical raster job、普通Direct/isolated和v4damage-miss路径使用；相同body跨copy/transform/mask保持有效。像素失效不代表几何失效。pixel hit完全跳过coverage是正确行为，不要强制重画。独立原CPU/Direct reference保持nullprepared/nullcache，不受全局候选服务影响；旧oracle二进制继续作Web参考。

新job Pending必须保留进度，并在写最终provider target前完成可用性预检；不得刷出半帧或签发receipt。现有composition needsPaint包含active+dirty，SDK已有pending/retry RAF，不需要改SDK或Interaction。改变相机只失效设备绘制资格，仍有效worldbody工作可以保留；更换source/config或surface generation要拒绝过期发布。资源真实不足保留可恢复工作，结构不可满足明确拒绝，不能全帧同步CPU逃逸或每RAF从头再来。

本步支持已提交 CompactStroke v4 的现有GPU路径。Raster/legacy/range/其他brush保持现有显式兼容策略。不得改Brush算法、Scene/History/Operation/input/ARC/postUp，预算还是32/96MiB，preparation allowance不加大；也不把整个headless共享helper做物理迁移。

验证以真实叶子与独立像素为准：旧代码 native7199knots×1/8/16/32copies 的重复准备机制RED；新one-step source work、Pending/resume、同源inflight共享、quota/lease/cancel/stale与actualcanonical→Direct/nav/ink顺序。Native DPR1/2/4混合对象/透明/变换/自交/历史mask测试保护前后遮挡；Web用同一S0/S2 canonical/drag-cancel/settled erase-navigation，两串行匹配构建worker，沿用修好的功能事件排程。

新普通Demo使用>=4286的空闲端口，不覆盖4284/4285及旧产物。没有commit/push授权。旧累计50starts/1881seconds，新UR1C2-Q1/N1各一次180s/3GiB，保持同一个8370秒总cap，结果按实际ceil记账。Normal无诊断，资格版统计actualcanonical/direct消费者、fullbodybuild、首次比较/support/length/编译与boundedreplay分开；不靠较大budget或只看GPU=true宣称优化。

源码/编译/profile/driver/资源身份冻结后才运行两个worker；匹配JS/WASM/resources只从新任务install使用。Semantic/Skia使用锁定SDK，不重建依赖。按包完成到READY_FOR_CONTROL_REVIEW或真实阻断，并主动通知控制线程；不自行进入UR1d/UR2或签P34/G5/performancePASS。


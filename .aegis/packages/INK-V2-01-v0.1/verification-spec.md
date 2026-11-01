# Ink v2 验证基线

状态：实施包的固定完成目标；本文件描述将来需取得的结果，不是现有通过证据。
测试必须观察实际 SDK/Runtime/编码器输出。命令、输入、构建和资源哈希随结果记录。
生成的新测试名称可以按仓库约定取名，下面的 requirement/oracle/acceptance 不得缩水。

## 固定样例

所有速度样例保留原始单调时间与无压感能力事实。记录模拟 source，不能用固定
pressure=0.5 的 trace 替代默认模拟验证。

- G01 单点、两点短划、水平线、圆、S 曲线、90°/180°折返、自交、点后停顿。
- G02 同一条 2 秒解析轨迹，以 60/120/240Hz 采样，速度包含缓慢、快速、加减速；
  每种又分单点、8 点、31 点 batch，最终输入内容相同。
- G03 device trace 含 0/0.25/0.5/1、缺失值、NaN、capability=false、跨源与时间回退。
- G04 笔宽 2/8/32，透明度 .25/.5/1，白底/深底，红/蓝交叉，1x/2x/4x 观察。
- G05 长线 x=8*i、y=0、t=8*i ms；N=512/2048/8192，batch=1；再加闭合波浪长轨迹。
- G06 128 条 4096 单位的纹理路径、32 个可见对象，其余在视口外；恢复、缩放和平移。
- G07 同样 confirmed 前缀，分别无预测、预测追加、预测替换及错误预测批次。

## 必需义务

| ID | requirement | oracle | acceptance |
| --- | --- | --- | --- |
| V-PRESSURE | 无硬件压力也有有效模拟；device 入口保留 | 真正 source/capability ingress + G02/G03，独立公式参考 | 默认忽略伪 pressure；慢/快稳定段平均 p 差 >=.15；同轨迹 120/240Hz p 最大差 <=.03；同输入 batch 分区结果一致；非法批次零部分接受 |
| V-PERSIST | 固化有效压力，不在回放重算 | 真实 AddStroke/Snapshot/远端应用；禁用 live simulator/catalog 后恢复 | 压力/配置/seed/version exact roundtrip，输出几何在 frozen tolerance 内相同；缺失 evaluator/resource/version 明确拒绝 |
| V-COMPACT | 新纹理数据不存展开 dab/outline/raw 副本 | 实际 protobuf Operation 与 Snapshot 独立解码，字段计数和 byte 长度 | dab/outline/raw 计数全为零；位置与压力只有一份；每笔数据 <=16KiB+18*K（K 为 retained knots，资源 blob 另计且全局去重） |
| V-SIZE | 实际空间明显下降，覆盖所有持久化出口 | 同 trace 的当前 chalk-r4 实际编码；新算法的紧凑/诊断展开两种真实编码；不使用 gzip 比较 | G05 N=2048/8192 的 chalk 新 Operation 与单笔 Snapshot 增量均 <=旧 chalk-r4 的30%；新纹理表示均 <=同算法诊断展开表示的40%；分别报告总量/新增资源/metadata，不移入别的字段绕过 |
| V-RESAMPLE | 压缩中心线不丢几何/压力特征 | 独立距离/宽度 oracle，G01/G02 高密度基准 | max 几何偏差 <=min(.25,size/32)；压力重采样误差 <=.02；短点与急转保留；120/240Hz 编码体积差 <=15% |
| V-WORK | 长笔画不逐次重算/复制完整历史 | G05 实际输入访问、历史复制、几何生成/上传 byte 计数，非墙钟伪计数 | N=2048 相对512、8192相对2048的总工作量比均<=5；append 无全历史 one-shot 调用；seal 全量求值至多一次；几何正确性不能因性能优化放宽 |
| V-CACHE | 派生内存有界且可丢弃 | G06 真实 retained/upload/tile bytes + 清缓存后恢复 | derived<=32MiB、临时tile<=16MiB；无完整常驻 dab 影子；视口外不生成纹理 tile；连续无变化帧生成次数0；清缓存后同像素/容差结果 |
| V-PEN | 圆头规则/手绘可用 | 公开设置与 G01/G02，native + Chrome | 规则模式主体宽度恒定、圆头；手绘体现有效压力；尖角/收尾不意外断线；不对旧 R01 golden 反向改期望值 |
| V-MATERIAL | 参数实际作用，笔型可区分 | 相同轨迹材质矩阵 + 独立像素/coverage观察 | 马克笔角度/扁率改变几何；粉笔密度改变coverage；粉笔与蜡笔结构差异不是换色；同笔事件密度不造成无意义加深；opacity只应用一次 |
| V-LASER | 瞬态激光不污染文档 | 注入单调时钟的真实 Runtime/SDK + pause/close/focus/cancel | 对象数/operation数/Undo/Snapshot字节不变；1000ms默认消退且空间/内存有界；无需新输入也继续动画与失效；普通导出无激光 |
| V-WATER | 轻量水彩与正确叠色 | G04 场景，独立 alpha/边缘/纹理 oracle | 湿边比邻近内部更浓，纸纹非均匀；相邻/重叠笔画确定性合成；调参数真影响结果；不取当前时钟或画布历史来重建旧笔画 |
| V-LIFECYCLE | 预测/预览/提交/取消一致 | G07、data overload、retry/close、foreign token/source | prediction不改变confirmed/seed/提交；cancel无操作；每笔一次Undo；重试复用operation身份；正式材质预览与提交一致 |
| V-CONSUMER | 公开接口与同构资源真正可用 | 匹配 JS/types/WASM/resources Chrome外部消费者、原生shared Session、TS正负例 | 本包交付笔型可选择/读回，第三包覆盖全部六种；默认无压力可绘；invalid枚举/token/数值拒绝；不用private deep import；Demo实际参数影响图像 |
| V-REGRESSION | 保留既有功能与旧数据读取 | 基线旧brush fixtures、手绘两批/Table/Section/字体/导出consumer，boundary/descriptor/package检查 | 受影响必需项通过；新失败修复；历史失败绑定执行前真实记录，不直接沿用旧的22这个数量 |

V-SIZE 的两个比较回答不同问题：旧 chalk 比较衡量真实现状改进；同算法紧凑/展开
比较隔离视觉算法变化。二者不能互相替代。若 baseline 不满足同一输入或统计包含
范围不同，先修正测量，不宣称数据瘦身通过。

按包绑定义务：01 使用 V-COMPACT 验证新数据载体与圆头笔，不要求尚未实施的纹理
材质；02 对马克/粉笔/蜡笔执行 V-MATERIAL 与 V-SIZE；03 对水彩执行 V-WATER，
V-SIZE 仅要求水彩的同算法紧凑/展开比较，旧 chalk 30%结果作为02精确证据复用。
每包的 verification.lock.json 明确 profile scope，不能把未来包功能变成前包硬依赖。

## 证据边界

阻断完成：对应包要求的原生日志、实际编码统计、工作/缓存计数、匹配资产的 Chrome
消费者结果、必要视觉矩阵与受影响回归、精确来源/结果哈希。图像用于识别明显材质
退化，不能代替字节数、算法或生命周期 oracle。

佐证：具名机器的 p50/p95/p99 墙钟时延、更多设备、盲测打分、额外截图。
宿主 Windows DLL/physical pressure qualification 不因当前工具可用性成为本地三包
默认硬门槛；声明与 wrapper 必需同步，不能以 native/macOS/Web 通过宣称 Windows ABI
或真实压感硬件验收完成。实际运行的 hosted/设备证据单独列出。

既有 build 可重用，但每次记录匹配 revision/dirty source manifest/输入/资源哈希。
不要求为证据另建远端提交；用户本轮未授权发布或发送会话任务。


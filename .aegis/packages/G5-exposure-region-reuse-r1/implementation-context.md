# G5 曝光区域与表示复用：单条闭环

当前任务是修原49对象缩小停画，不继续只调瓦片/LOD参数。旧WH结果仍失败；查询成本边界修正可复用，失败的disjoint调整不可直接提升为候选。

## 已证实的问题与基线

4339 exact normal1079为B。WH trace确认：冷load约2900us被算作后续Query估计，超过2000us deadline，反复拒绝查询；修正只计真实Scenequery后任务恢复。随后同一个job每帧推进128GPUprimitive但67个机会没有发布；首次mandatorycell有49sources。一次分离strip+优先级调整即使推进49152primitive仍0/48present。因此必须改变完成单元和复用，不能只说“把四条边拆开”。

## 选定的实现结构（内部类型名可按代码调整）

1. 保留256texel world tile作为存储/LOD坐标。把必需曝光的准备/完成单元改为tile内固定4×4子块（每块64×64该层texel），外加现有2texel采样gutter；仅调度实际hole相交的子块。4个bootstrap父tile至多64个待需子块，保存位图/有界需求表，不每相机创建队列。单块源查询使用其gutter扩展world区域，或使用已完成的较大源集合做有证明的局部筛选；不能用disjoint区域外包矩形成为关键依赖。
2. 在已有prepared geometry上保留局部组件选择/重放游标，组件cull只作安全负筛选。共享同体几何，避免每副本每子块从头编译/扫描全长线。继续使用现有request/advance分段机制。不要把一块等同一个对象，也不要假定局部一定少于49对象。
3. 在现有Render material/body owner内扩展有界多项body coverage缓存，替代导航每副本反复raster相同本体。优先实现v4圆头GPU已合格路径的不可变本体coverage复用。key须覆盖完整body/config/seed/range、local-to-raster线性部分与采样phase、density/grid/crop/gutter/filter及context/resource/target要求；整数平移别名只有逐项证明采样一致才可用。不同亚像素phase、旋转/缩放不允许误命中；无法共享pixels时仍复用几何。现有prepared body不是新覆盖算法。
4. body不可变；永久mask应用到独立结果scratch，不能污染共享body。masked result另以完整ordered mask身份、transform/material与region basis索引；历史mask使用现有prepared history服务/游标，mask-only变更保留body，仅失效受影响结果区域。material/opacity/背景/图像/文字及z-order仍按既有路径逐对象合成。不得将整个混合截图统一挖洞或合并半透明对象一次画。
5. 子块必须完成其全部相关有序sources后才能发布。已完成块保留并加入coverage分区；后续相机/LOD改变不能冲掉仍有效完成块或尚适用游标。validity用有界子块位图/区域表示；表示耗尽只能失效或调度有界重建，不能把未画区域补成矩形有效。最多1 active +1 suspended raster job，分别保留frontier/refinement。新缺口优先于不相关quality，完成后恢复quality；单次仍只有原全局deadline/allowance。
6. 明确bootstrap：第一张新相机必须由既有合格retained/coarser来源 + 全部缺口的完整子块组成。缺一块不能提前present。使用旧源的更粗投影只在其真实valid范围/density/transform匹配时允许。可利用现有有界idle/prewarm机制预备可用粗块/共享body，不得延迟输入、暗中额外priming或新增独立budget。全视口所有必需块完成的成本才是100ms目标；每块完成快不等于通过。

## 有界资源与推进

固定64texel子块、2texelgutter先做本闭环，不能靠候选结果搜索块尺寸。合并/跨tile裁剪实现细节可在preflight决定，语义不变。每个粗tile16个块，bootstrap需求至多64；只有1active+1suspended raster，不为64块同时分配surface。新body/result像素缓存合计项数上限min(1024,options.maxEntries)，同时受现有共享budget/单项/optional limits约束；元数据、source leases、staging和pending release全部真实计账，不预分配。原tiles最大4096/已有需求工作集上限不增加。GPU临时压力等待退休，真实容量不足有界回收/降级到已合格来源；不得整帧同步CPU、busywait/fence或早退GPU lease。

全局navigation2000us；queries4扣outer已用；setup=max(1,passed remaining queries)；steps64；fine GPU128/coarse1024及已有ticket/publicationcaps均不加。mixed场景内单source的v4能力判断可收敛，不能把v5/v6误当v4，也不能每source重置全局GPU额度。复用命中照样计实际composition/copy成本，不以helper计数伪装没有GPU工作。

## 实施和验证节奏

先核对exact来源，移植query边界fix；adapt原fullRuntime Mixed49 RED测试（仅改新输出路径、保留输入/断言），形成真实Runtime→Scene→Render→Metal正向GREEN；再补局部复用、采样phase/mask/遮挡/失效/资源回归。测试可使用native同步完成机制检查正确性，不能将其当Web时延。常规实现/TDD迭代可持续做，旧包一次调整限制不适用新包；不得扩成第二renderer。

native GREEN后做早期真实Web诊断（有限4starts）确认该49对象当前相机真正有新画面。没有通过不要花时间重复大矩阵。随后freeze normal/diagnostic sources、matchingSDK、driver/oracle，public smoke DPR2/4，提供独立4340试用并主动通知控制会话；不等ACK继续8普通+4光学验证/归档。若正式guard失败，保留全部失败并终态回报；不挑最好一轮或偷偷改阈值。没有额外前置的Windows验证/1000测试/历史全套复跑。

本会话控制设计和独立review，coding会话修改/构建/验证；任务内preflight无语义/范围冲突时直接继续，不返回等常规ACK。主动通知本control：预览就绪一次，终态一次。结束回传精确result路径/SHA和source/SDK/各route结果后STOP。无提交推送，无primary production应用。

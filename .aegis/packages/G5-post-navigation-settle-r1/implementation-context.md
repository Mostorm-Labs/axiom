# 导航停止后的恢复：先查清尾部，再关闭剩余验收

用户已要求保存4340收益并开始下一步。准确检查点已提交并推送到 `codex/g5-exposure-region-checkpoint` 的230ee4c52ee36361abb26c527f29c86e9a8cebb7，annotated tag `g5-exposure-region-reuse-20261117`（nonrelease）。新任务B以该提交+冻结normal source/SDK为准，不继续使用primary未完成试验。性能原工作树codex/g5-cpu-worker、旧source/results/SDK/端口4336–4340全部保持。

## 已经完成、必须保留

固定64texel曝光块/2gutter、fullregion身份与完整viewport组合；canonical与navigation共用SkiaInkLayers完整support scalarN32白coverage；mask/result/material独立；精确phase/format/context校验；GPU租约完整。修正Query成本污染、旧fullquality任务AABB与hole相交导致错误占据frontier、WebsRGB provider与scalarcoverage格式不一致。原Mixed49最新45/48真实新present，Node6/native21/边界122检查点再次验证通过；normalDPR2/4wheel/touch最长GLcontent提交gap约34–50ms，旧4339约1133ms。

## 当前确切缺口

普通版停止后的stable中位数：DPR2wheel347.35/touch377.60ms仍在386.81/398.83限额内；DPR4wheel1081.55/touch859.70ms超过558.2175/573.11限额。raw中DPR4C1 wheel inputEnded7250.4→canonical onFrame8478.6→stable8530.1ms，表明长尾大部分已发生在canonicalframe之前，不能先说全是末尾3次poll或GPU等待。实际需分清CPU准备/重复工作、GPU排队退休和owner调度。

DPR2光学wheel100：inputEnd2662.6，33.4ms截止2696.0；下一实际present2710.3已包含input102，真实等待47.7ms。它发生在有读回的opticalroute，不能直接当ordinary同程度延迟；但冻结验证仍失败。最后360另被分析器过滤到inputEnded误报，实际7009.5（13.3ms）已有匹配导航present，不能拿7181.1更晚canonical当首次匹配。原失败与raw不重写，修新分析器并对同一raw离线复算。

剩余像素：DPR4光学未跑，D2只有7个B/C完全匹配参考对（14captures）聚合指标一致，34UNAVAILABLE。特定64gutter细线/shape接缝、完整body projection与半透明混合遮挡矩阵未闭合。不能将这些当通过，也不需要重做历史所有测试。

## 诊断路径与本包允许修复

1. 先离线看原raw/onFrame/RAF节奏和当前源码，建立stop时间线：最后input→100ms quiet交接→canonical FramePlan/几何/本体/历史mask/材质准备→GPU提交→匹配exactpresent→真实retirement→SDK停止调度→原stable谓词连续3次满足。计时要把实际计算与RAF空闲分开，不能把整elapsed称为CPU工作。
2. 当前交接入口：CanvasEditingRuntime::serviceNavigationPresentation在quiet>=100ms暂停feedback、将exact准备交给presentCanonicalFrame；后者maxGroups64/原8ms/4096coverage steps。SkiaRenderer::renderFrame的cache prepare/prepareWorkingSet/reset与共享body producer，导航与canonical重复不同density准备、mask重放、framebuffer复制/合成、target/metrics/Scene失效及预算/退休压力是需要核查的候选，不预定其为根因。
3. 用最少defaultOFF boundedrecords覆盖整个stop尾部，不用每primitive日志/clock/console或每帧完整Worker账本。记录既有owner的phase/cursor、body hit/miss及原因、真实targetGPU/CPU、字节/租约、实际deadline/yield和每次RAF/receipt的关联。每route最多768owner记录/尾部最多5s，精确64bit身份lossless；阶段耗时守恒，nested工作不能重复相加。
4. 先用有限DPR4旧B诊断定位并证明新C对应变化，再修现有owner的重复prepare/raster/mask/compose、已合格表示复用、进度失效/提前重启或交接等待。修复可贯穿cacheproducer/canonical/navigation/Scenequery；SDK仅在实际证明调度/receipt适配缺陷时改私有内部路径。禁止先调budget/quiet常数。若主要时间真是必要计算，可在原预算内分段重用，不能只让pending提早false。
5. 解决stop尾部后，按同一完整时序分析wheel100区域firstready/LOD切换；合法显示必须及时生成。光学readback会扰动后续opportunity，分别报告normaltiming与capturepose；不为通过而等未来present再冒充旧捕获。

## 验证与闭环

诊断池4starts总480s，建议D4B→因果C验证，D2window补充按实际需要，不重跑历史4+4。native先用actualRuntime49 fixture证明前后，沿原48calls>=42并执行noOperation/losttarget；增加必要RED/GREEN针对已定位phase/mapping/cache失效，复用原oracle。补64边界gutter细线/shape、透明自交、不同mask副本与image/text前后顺序、资源压力/context丢失；无新语义算法。

最终normalmatrix8：DPR2与4各B,C,C,B，两route原360events/6s/1431×1010。B=4340checkpoint，C=新candidate。保留原4339旧stable绝对限额386.8100/398.8275/558.2175/573.1100ms，且C不得比4340差。normal最大contentgap和方向边界<=100ms，原p95guard；GLop p95=0不是显示framep95，另有successfulpresent证据。paired计算要在数据齐全后立即执行，下一组采样前不得漏掉可算stableguard。

最终4optical：每DPR B/C同profile/successfulpresenthook；fixed12checkpoint立即读当前target+latestactualstamp+immutable请求姿态。实际33.4窗口含最后input之后33.4ms内的present，不能截断；capture仍保留事件返回瞬间的旧画面。精确reference二进制/fixture/resource/geometryedgeID固定，可离线为新actualpose生成missingCPU参考（上限128unique/每次60s/总1800s，最多1进程；缓存相同request）；不需要新browseroracle。所有mandatory6profile有限或已有独立fullmatchedpixel方法覆盖可辨识内容；finalsamepose用既有CPUreference非AA容差2、2px独立几何边缘及已知DPR4image差异单列。预先把旧optical-profile.py/参考边界方法内容寻址，参考位置与eligibility不能按candidate好看选。缺失是UNAVAILABLE，绝不零误差/静默放过。不要沿用“终态失败后不生成参考”作为本包oracle准备借口。

matchingnormalSDK公开功能DPR2/4通过后，新4350?不用预设远端：优先4341若空闲否则下个空闲port。必须独立于4340，禁止替换或重载用户tab。基础就绪主动send_message_to_thread回control URL/PID/sourceSDKhash并继续正式闭环无需ACK；正式失败保留实验候选和精确结果。成功/明确阻断最终通知control并STOP，不自动提交下一改动。

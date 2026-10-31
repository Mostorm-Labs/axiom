# INK-V2-02：固定材质算法与低存储纹理笔刷

入口：`.aegis/packages/INK-V2-02-v0.1/package.json`。依赖 01 的实际紧凑 carrier、
信号、config/readback 和生命周期能力；不能仅凭 01 文档或最终消息认定依赖完成。

## 任务步骤

1. 解析 01 的实际结果 manifest 与证据；同仓库 descendant 可以复用，核对 delta，
   不重做有效工作，不要求 HEAD 等于历史包锚点。
2. 先捕获当前 chalk-r4 同输入编码基线，并搭建新算法紧凑/诊断展开 writer 的独立
   字节统计；明确 Operation、单笔 Snapshot 增量与资源增量的计数范围。
3. RED：马克笔 headAngle/headWidth 改变后现有轮廓不变；chalk density不影响当前
   dab求值；旧记录包含全量dab；当前连续粉笔整笔平均动态。分别建立输出/像素观察。
4. 实现扁头扫掠与参数绑定。单笔 coverage均匀，透明度一次应用，跨笔显式叠色。
5. 实现粉笔连续覆盖+纸纹/颗粒固定场，保留每处有效压力；density真正改变覆盖。
6. 实现蜡笔独立沉积/阈值参数和视觉特征，与粉笔共用基础但不只是换色。
7. 新纹理写入全走 compact carrier。不存全量dab、raw副本、outline或每笔bitmap；
   染色/材质显示由固定版本算法产生，有界暂存与缓存淘汰不改变结果。
8. 拓展 config/preset/readback、真实材质预览、Demo 参数与 native/Chrome矩阵。
9. 跑真实字节压缩、长笔工作量和缓存预算检查；回归01圆头/激光与已有画板功能。

## 必需检查

- V-PERSIST、V-COMPACT、V-SIZE 两类比较、V-RESAMPLE、V-WORK、V-CACHE、V-MATERIAL、
  V-LIFECYCLE、V-CONSUMER、V-REGRESSION。
- 默认 simulated 覆盖三种新笔型；device入口沿01规则，未受影响时可绑定01证据。
- 独立测量保证没有把dab移进blob、cache、Undo inverse或资源清单来冒充节省空间。
- 仅减少点数但超几何/压力误差，或仅缩小PNG而规范数据仍膨胀，均不得完成。
- 不依赖水彩功能，不开发任意刷包导入或Brush Studio参数编辑器。

## 返回

交付三笔型可试用Demo、同轨迹材质矩阵、空间/工作量/缓存原始计数、旧数据回归、
数据与资源哈希。按closure完成后返回CONTROL_REVIEW，不发正式Gate结论或自动发布。


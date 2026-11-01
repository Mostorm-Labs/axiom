# INK-V2-03：轻量水彩与六笔型闭环

入口：`.aegis/packages/INK-V2-03-v0.1/package.json`。依赖01、02的实际结果。

## 任务步骤

1. 验证两个依赖包与结果，重用其适用证据；核对changed scope而非重跑所有历史工作。
2. 固定watercolor-lite材质版本：局部coverage、内部浓淡、纸纹、边缘颜料带、整笔
   透明度与跨笔SrcOver叠色。版本只依赖自身固化数据与资源，不采样已有画布颜色。
3. RED：当前主路径没有真正湿边/纸纹水彩能力，旧WaterColorLite枚举不算交付；
   实际公开preset、参数和像素oracle必须能观察新效果。
4. 从01紧凑中心线/压力派生coverage与边缘距离带，通过02共享材质基础渲染；同样
   不存全量dab/alpha-mask图片。局部tile与GPU/CPU派生缓存受共同预算控制。
5. 实现有效湿边/纹理/透明度参数与readback，模拟压力为默认。Demo展示同色/异色
   交叉、不同笔宽、白/深底和同笔自交；预览到提交保持材质一致。
6. 完成六笔型公开选择/设置/能力读回矩阵：无压力鼠标全部可操作；非法参数及过期
   token/source等拒绝；激光仍无文档副作用。
7. 完成持久笔型保存/恢复、远端应用、资源缺失与未知版本拒绝、撤销/重做、图片导出。
8. 对六笔型运行相关native/真实Chrome/TS/边界/descriptor/package检查，绑定精确
   assets/source manifest。列明真实Windows DLL/物理设备未验证部分。

## 必需检查

- V-WATER、V-PERSIST、V-COMPACT、V-RESAMPLE、V-WORK、V-CACHE、V-LIFECYCLE、
  V-CONSUMER（全部六种）、V-REGRESSION；V-SIZE执行水彩同算法<=40%比较。
- 02的旧chalk-r4<=30%比较可复用相同来源绑定；水彩没有旧对应实现，不伪造旧基线。
- 六笔型矩阵保留V-PEN/V-MATERIAL/V-LASER的公开行为smoke；未修改的内部证明可复用。
- 不加入扩散/干燥模拟、画布拾色混色、云协作presence或全画布依赖。

## 返回

交付六笔型Demo、视觉矩阵、水彩空间与性能记录、端到端结果和明确的平台资格边界。
现有历史失败重新核对并列明，新增失败不得藏在历史计数。用户后续启动执行时按包
连续完成，不把“多一个shader”或“单项测试通过”作为提前结束点。


本轮状态为 BLOCKED，候选版供手动观察，尚未达到控制面验收条件。

候选 Demo：http://127.0.0.1:4283/example.html
受控分支：codex/geometry-navigation-integration，基线14ce622161474e40d2f19ecfae66e9f31cf12933。保留所有既有 dirty 工作及4280/4281/4282；本轮未提交、推送或改变 SDK/锁/预算。

已实现的交接路径：partial-erase Down 只准入 trace，不同步 clear/acquire/present；精确命中独立恢复当前 Scene；History 在 semantic apply 前准备；active/sealed 反馈由 Runtime 持有，精确 replacement roster 在实际 submit 前冻结，绑定成功 CanonicalWork，仅在严格 completion 后退休其覆盖的 sealed identity；导航使用新建的当前 Scene query basis，不伪造旧 canonical 身份。

原生验证：26项受影响 CTest通过，其中8项新增交接测试最后再跑通过；包含真实 v4、busy acquire/present、History与Scene故障、DPR1/2/4遮罩/半透明/背景、错work/lifecycle、Undo/Redo及新手势保护。额外3项LiveErase/clipboard测试有2通过，1项旧断言仍要求Down清理未获合格回执的Brush预览；原断言保留，记录为待控制面协调的契约漂移。受影响Node13、Python2、120项边界检查、diff检查通过。额外旧Demo静态测试仍有继承的按钮结构正则失配。

两次固定Web候选验证：8个公共输入分支全部完成，无INPUT_REJECTED/DUPLICATE_CONTACT，最终contacts均为0。两组连续擦除中的6次nextDown检查全部准入，计划Up后4ms，实际派发约18–19ms；不能将计划时间写成实际时延。两次worker共计59个向上取整秒，累计44 starts/1433秒，进程族全部清理，无额外seed、smoke或retry。

未闭合项：

- C1预选拖动、C2预选拖动、C2擦除后缩放的Undo/Redo像素回环检查失败。其余5分支完成独立native cold pixel oracle。失败cold函数在传回像素前抛错，当前证据不足以判断是允许的AA边缘差异、内部像素错误还是呈现/采样时序；不将其改判通过。
- 6个即时cancel/pan/pinch输入阶段，相机虽已改变，却在约320–341ms期间未观察到默认framebuffer的内容写入。因此尚未证明密集擦除后的导航连续性已修复；相机状态成功不等于画布反馈成功。
- 旧测试的Down清理假设需要按新契约协调，不能为通过旧断言而恢复同步绘制或提前退休未获回执的预览。

下一步应先完善失败时原始像素/快照保全并分类差异，再定位密集场景的feedback demand与source准备。当前两次浏览器窗口已闭合；新采样由控制面另行授权。本轮不签发性能、完整G5或P34 PASS。

result.json绑定包、精确dirty源、正常JS/WASM/resources、全部日志及materialized archive；browser-analysis.json保留每个分支的输入、相机、提交活动与cold结果。214MB的完整未跟踪输入归档存于授权out目录，以SHA索引引用，避免作为巨型Git源文件引入。

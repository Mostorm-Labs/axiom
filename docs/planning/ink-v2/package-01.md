# INK-V2-01：信号、紧凑载体、圆头笔与激光

入口：`.aegis/packages/INK-V2-01-v0.1/package.json`。本包不依赖后两包功能。

## 任务步骤

1. 核对仓库身份、祖先锚点、当前 dirty delta；阅读包内 context/closure，再按需
   加载锁。保留手绘两批、Table、圆角、字体改动。为本包记录真实起始源清单和测试基线。
2. 定版 `CompactStrokeV1` 的载体、参数版本、capability 与纯数据 DTO：注册非冲突
   编号，新读取能力在 mutation 前验证。原分支不改义。定版记录后再修改生产代码。
3. RED 观察：当前模拟 lane 无真实时间信号；新紧凑 carrier 不存在；当前 append
   全历史扫描；默认产品预览为橙色。新断言应观察边界行为而非字符串匹配。
4. 扩展既有输入→Interaction→Ink 的时间/能力事实传递；实现默认 simulated 与显式
   device 的入口及错误行为。模型、预测状态和设备模式都在 begin 冻结。
5. 实现唯一中心线+有效压力写入；新圆头笔从紧凑数据求轮廓，规则/手绘切换；保留
   pinned perfect-freehand oracle 不改。pressure已解析时禁用二次模拟。
6. 增量 confirmed 状态与可替换 preview，取消不提交，seal一次完整求值；默认预览
   使用真实 captured paint 和材质，诊断色只留在显式 debug 能力。
7. 实现瞬态激光 owner、单调时钟驱动淡出和有界环形轨迹；不经过 AddStroke。
8. 接通公开 config/readback、Session、Web/types 与 Windows caller wrapper；匹配
   WASM/assets 的 Demo 能用无压力鼠标切换圆头模式和使用激光。
9. 验证编码/恢复/远端/Undo/取消/过载/旧数据读取、资源与打包边界；交付本包证据。

## 必需检查

- V-PRESSURE、V-PERSIST、V-COMPACT（圆头 carrier）、V-RESAMPLE、V-WORK、V-CACHE、
  V-PEN、V-LASER、V-LIFECYCLE、V-CONSUMER、V-REGRESSION。
- device 用合成真实能力事实 trace 验证可扩展入口；真实硬件资格为佐证，不阻断
  当前默认模拟输入交付，也不能反向宣称实际设备已经通过。
- 不要求 02 的纹理视觉或 03 的水彩才允许本包结束。

## 返回

返回实际本地结果和 `READY_FOR_CONTROL_REVIEW`，附精确来源 manifest、命令、退出码、
输入/资源/包哈希与 Demo。若无 commit，则报告 source manifest，不能伪造 result SHA。
不提交、推送、发布，不改历史 Gate。不在仅完成核心类或单元测试时提前结束。


# CPUW4：编辑后首个导航等待定位

本轮结果为 `ATTRIBUTED_NO_REPAIR`。已完成固定工程文档的 12 项普通版观测和 3 项资格诊断，尚未证明可在本包范围内安全修复的单一 Render 缺陷。没有生产代码改动、修复候选 Demo、提交或推送。4330、4328、4298 和用户文档均保留。

受控工作树是 `/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom`，分支 `codex/g5-cpu-worker`，HEAD 为 `53cd1d6625232c2112c8db272e61fa6da2a0910b`，包含已接受的 r3 未提交改动。CPUW4 任务包位于 `.aegis/packages/G5-CPUW4-edit-navigation-r1/package.json`，结果位于 `.aegis/results/G5-CPUW4-edit-navigation-r1/result.json`。

工程夹具由原 Fixed40、三条公共材料笔刷、六个混合对象及原 2.3MB 图片组成，共 49 对象。粉笔／蜡笔／水彩保留算法 6／5／6；没有删除材料或强制进入 Worker。所有有效观测均恢复同一实际 public author 快照和资源，以 DPR2、CSS 1431×1010、相机 scale 1.5、translation (-200,-120) 开始。

普通版使用原 r3 Release、DIAG_OFF、TIMING_OFF 候选。稳定后的放大约 1–12ms 即记录到默认 FBO 内容写入，缩小／平移约需 0.8–1.4 秒。立即导航时，放大同样可能等待，因此没有完整复现用户的“立即放大正常、缩小／平移顿”差异。这些数字是 GL 写入代理，不能称作物理显示或匹配相机的导航呈现时间。

诊断否定了本输入下“导航冷几何重编译是主因”的假设：新增编辑后的三个诊断中均未出现导航 geometry build，prepared body 已命中。另一个观察到的等待机制是旧画面的区域证明被拒绝，前沿没有在导航机会内补齐，最终由 canonical 收敛。新增笔画确实改变对象集合；局部擦除也确实修改算法 6 粉笔的遮罩，现有仅支持完整 v4／outline 的 mask proof 不能授权复用旧像素。不能放松身份和遮罩校验来减少等待。

抬笔附近还有共享 Scene 提交、canonical、preview 和旧擦除精确计算成本。Scene 提交约 32–37ms；代表性旧擦除 Up 约 176ms，包含约 125ms exact preparation。最终 canonical render 有较长的未细分排他时间，不能把它全称为 CPU 几何或 GPU 执行。后继范围应针对这些已测量阶段，或为合法区域复用及前沿完成建立可证伪的小范围归因。

本轮真实浏览器共 4 次开发启动，计费 105 秒，最终验收窗口使用 0 次；前三次驱动失败均保留，最后一窗完整完成。独立原 OperationEngine 重放夹具 12 个操作，以及 append／erase 各一个操作，三个最终快照均逐字节一致；同一编辑的六种导航时序也产生相同 Operation 和快照。未开展新的冷像素或 Undo/Redo 验收，未声明整体 G5、P34 或性能 PASS。

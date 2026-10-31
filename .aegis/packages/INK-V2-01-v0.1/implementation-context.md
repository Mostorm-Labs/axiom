# INK-V2-01-v0.1 实施上下文

交付：信号、紧凑笔画基础、圆头笔和瞬态激光。
当前只准备包；收到后续实施指令前不修改产品代码。

## 当前路径

collector -> CanvasEditingRuntime -> BrushSession -> BrushCommitAdapter -> AddStroke /
OperationEngine -> canonical record -> Scene/Skia。现有R01为已集成的C++轮廓参考；
本包不删除或改写其golden。

先把版本化紧凑载体与模拟压力正式定版，再从现有输入/BrushSession接通新圆头与瞬态激光。不要实现纹理材质才结束本包。

## 首个动作

依次读package.json、本context、execution-contract.json和task-plan.md。
按需读取authority/verification/evidence locks。核对仓库provider/name、包hash、
祖先锚点和真实dirty delta。参考baseline.lock.json，不能checkout/reset抹掉在研工作。
随后完成ImplementationDesignPreflight与新carrier版本绑定；不要凭旧枚举声称笔型已实现。

## 固定需求

默认simulated；device显式、保持真实能力语义。新笔画保存紧凑中心线/有效压力/
版本/seed/资源绑定，不保存逐dab或整笔bitmap。临时激光不入文档。
设计细节在design-proposal.md，分类是技术提案，不是历史schema已被更改。

## 已知缺口

BrushSample缺time；append全历史求值；marker角度/宽度未进入主轮廓；
chalk density未进入当前求值，连续chalk整笔平均；preview默认orange；
旧texture记录包含samples+全dab。以上是实现审计事实，不等于未运行的测试通过。

## 返回

以本包verification.lock中profile scope为准，未来包不是本包完成条件。
连续完成全部可执行要求后返回READY_FOR_CONTROL_REVIEW和精确本地结果证据。
不自动提交/推送/发布，不宣称正式Gate或未实测的Windows/硬件资格。

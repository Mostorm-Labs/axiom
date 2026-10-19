# FC-02 Runtime 输入合同原型：执行结果

日期：2026-10-19。输入源码/设计基准 `79aa19c15dc60df633ad5a9eab7aaeb7a46866ed`。
工作树 `C:\Users\NearHub\.codex\worktrees\p32-windows-touch\axiom`。
类型与 FakeIME 源码在 [prototype](prototype/text_input_contract.hpp)；主合同见
[Runtime input contract](../FC02_RUNTIME_INPUT_CONTRACT.md)。

## 执行边界与命令

```powershell
& docs/planning/fc02/prototype/run_text_input_probe.ps1
```

使用已安装 VS2026 BuildTools 18.10.2 / MSVC19.51.36260 x64，
`/std:c++20 /EHsc /W4 /WX /MT /O2`。只编译协议原型及既有 foundation 类型头；
没有编译/重建 Semantic SDK、protobuf、Skia，也没有接入产品二进制。

首次编译因 `optional<uint32_t> == int` 触发 C4389 `/WX` 失败，日志保存在
`out/fc02-input-contract/compile-red.log`；将比较值改为 `2U` 后严格编译通过。
没有降低警告等级或修改任何旧测试。

原型 exe：`out/fc02-input-contract/fake_ime_contract_probe.exe`。
SHA-256 在最终编译后记录如下；普通重编可能因 PE 时间戳不同而改变。
`a5337ab9790f6f0aef1a197f5838428736d805725a8037f86a227f24e180462c`。
编译/执行日志：`out/fc02-input-contract/compile.log`、`probe.log`。
这是一次同步 CLI 进程执行，非长期运行的 Minimal Host/浏览器会话。

本次编译输入 SHA-256：

- `fake_ime_contract_probe.cpp`：`23c1fb683085b9fb0aa3d2d8876b3963c221c42bd6a637e56211aa2857611a2c`。
- `text_input_contract.hpp`：`97a990ae0a8b1ef8789d1dbdcdefc57a1a281ab980d45232552f635f9c88cd3e`。

## 实际观察的协议场景

以下 PASS 仅指协议模型断言通过。每项均使用 TextIngress + FakeIME 输入，提交端为明确的
FakeCommitPort；没有真实 canonical Object/Operation/digest/frame/retry 读回。

| 场景 | expected | actual | 结果 |
| --- | --- | --- | --- |
| 多次 composition update | 不请求提交；最终确认后才等待 apply | 3 次 update，attempts0；确认 Pending；fake apply 后 accepted/history 各1 | PROTOCOL_PASS |
| 重复 terminal、payload collision | 原 key 回原 receipt；新序号重复终止不重交；同 key 改内容拒绝 | fake accepted1，原 receipt 可读回，collision 被拒绝 | PROTOCOL_PASS |
| UTF-16 / scalar | emoji 不拆 surrogate；孤立 surrogate 拒绝 | A😀B offset3→scalar2，offset2/孤立 high surrogate 拒绝 | PROTOCOL_PASS |
| 工具切换 | Finish effect→确认→apply→退出 | apply 前 completedExits0；接受后1，重复意图不产生新 effect | PROTOCOL_PASS |
| 离页提交被拒绝 | 历史不增加、不退出，可显式重试；原拒绝回执不可重写 | 拒绝后 history0/exits0；重试接受后各1；原 input 重放仍为 Rejected | PROTOCOL_PASS |
| Debug focus 完成失败/错 effect | 不退出、不提交 | 错 effect 拒绝；失败恢复 composing，attempts0 | PROTOCOL_PASS |
| resize / geometry | 会话保持，旧几何失效 | ProjectionToken 不变，旧 geometry 拒绝，后续输入 Pending | PROTOCOL_PASS |
| 旧 attachment / full128 ID | 迟到事件和高64位不同的对象均拒绝 | 两者 Stale，attempts0 | PROTOCOL_PASS |
| 远端相关/无关变化 | 无关变化继续；目标变化使旧投影失效 | Update 继续，旧 Commit Stale，attempts0 | PROTOCOL_PASS |
| 顺序与有限 journal | 缺口拒绝；背压不丢 terminal；ack 后可重试 | gap 拒绝，满8项时 Backpressure；ack 后相同 terminal Pending；过期/未结束淘汰被拒绝 | PROTOCOL_PASS |
| Cancel / Enter | IME Enter 不结束；Cancel 不提交；普通 Enter 结束 | 一致；Ctrl+Enter 仅证明未误结束，换段解析不在模型中 | PROTOCOL_PASS（有限范围） |
| detach 在 apply 前后 | 未执行工作失效；已接受修改保留 | apply 前 accepted0；apply 后 accepted/history 各1 | PROTOCOL_PASS |
| 空草稿/相同替换 | 零提交、零历史 | 两种情况 attempts0 | PROTOCOL_PASS |
| Pending 时新输入 | Busy，不消耗待重试序号 | apply 完成/重同步 token 后同序号可接受 | PROTOCOL_PASS |
| composition / exit 身份复用 | 同 exit ID 不同目标拒绝；失败 effect 迟到不提交；新 composition 可开始 | collision/late finish 拒绝，旧 composition ID 拒绝，新 id 接受 | PROTOCOL_PASS |
| 预编辑选区坐标 | 在 preedit 自身的 UTF-16 范围验证，不改 confirmed 投影 | surrogate 中间拒绝；合法 caret 接受；resize 保留预编辑内容/选区，attempts0 | PROTOCOL_PASS |
| 未知事件 kind | 拒绝，不进入提交口 | Rejected，attempts0 | PROTOCOL_PASS |

最终输出必须包含：

```text
PROTOCOL_MODEL_PASS 17/17; production_runtime=NOT_CONNECTED; semantic_v2=NOT_IMPLEMENTED
```

## 未覆盖、仍需真实接线的项

| 项目 | 状态与原因 |
| --- | --- |
| 真实 Semantic delta2/InsertObjects/Undo 原子性、Object/Operation 数 | IMPLEMENTATION_PENDING；原型只观察 fake commit 调用，不伪造文档 |
| ParagraphId、跨段 replacement、全文投影窗口、grapheme 与 Ctrl+Enter 换段 | IMPLEMENTATION_PENDING；模型只执行 UTF-16 单缓冲区范围检查 |
| caret/selection/hit geometry、字体与 preview/canonical/frame retry | IMPLEMENTATION_PENDING；没有布局或渲染替代实现 |
| 生产 Facade 的统一排序、按目标屏障和 focus/pointer 路由 | IMPLEMENTATION_PENDING；本轮是合同原型，未修改生产 queue |
| 真实 Windows/Web IME、Debug UI 焦点、resize 时输入法行为 | PHYSICAL_PENDING；未启动产品或进行人工观察 |
| Snapshot3 能力组合、strict required_ids、digest 投影差异 | 原语义证据中的 FAIL 保持，未修改为 PENDING 或 PASS |

原型 journal 容量8只是让背压反例可达，不是生产容量或新性能目标。原型中的异步 complete
代表“排队之后尚未 apply”的 owner 时序切分，不代表远端服务器或实际 Semantic 异步实现。

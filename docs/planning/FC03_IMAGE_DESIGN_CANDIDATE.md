# FC-03 Image 设计候选（P10–P20 输入）

状态：**候选 / 待 P10–P20 审定**。本文不发布 Current Authority，不授权 P31/P32，也不改变既有 ImageContent Authority、G4.7 证据或历史 Gate。

## 1. 范围与已确认产品决策

FC-03 只负责资源驱动的 Image 对象。Image 不包含文本；文字仍由 FC-02 RichText 或其他已授权文本容器负责。

首版范围：

- Windows/Web 插入和替换 PNG、JPEG、WebP；
- 普通图片态的选择、移动、四角缩放、工具条和既有对象操作；
- 普通缩放默认保持原图比例，按修饰键允许自由拉伸；
- Crop 临时编辑态：拖动底图改变内容位置，拖动裁剪框改变可见范围，顶部滑条改变图片内容缩放；
- `Done` 一次性提交裁剪范围和裁后显示宽高，形成一次可 Undo/Redo 的逻辑编辑；
- `Esc`/`Cancel` 放弃临时状态，不写 canonical、不产生 History。

不在 FC-03：文本标签、图片内文本编辑、EXIF/ICC 产品策略、像素 alpha 命中、网络同步协议、G5 缓存预算、全局多对象布局。

## 2. 现有语义复用与权威边界

仓库已有 `ImageContent`：`resource_id`、intrinsic width/height、可选 `source_rect`、`content_mode`、独立 display `width/height`。已有 `SetImageContent` whole-content Operation、`SetObjectSize` Image 分支、Image Resource Resolver、Scene bounds、Direct Reference 和 Skia source-rect 绘制。

FC-03 不新增 ObjectKind、Operation kind 或 Proto 字段。导入时由产品层根据解码结果构造完整 ImageContent，明确 `content_mode=FIT`、intrinsic 尺寸和初始 display box；语义层继续执行完整值校验和原子拒绝。

现有仓库镜像 `docs/notion/authority/04-semantic-schema/01-object-schema/image-content-v1-release-v0.1.md` 标为 `frozen-by-v1-final-gate`，而对应 Notion 页面仍显示 `Proposed Freeze`。该状态差异必须在 P21/P22 处理；本候选稿只能消费已存在字段，不能据此发布新的语义结论。

ResourceManifest 是唯一资源元数据来源。Host 只提供不可变 bytes/文件事实；ResourceId、revision、hash、byte length、decode metadata 来自 Document ResourceManifest。缺少 manifest、hash 不匹配、版本过期或晚到结果都保持 Missing/Unverified/Failed，不得修改 ImageContent 或伪造 Ready。

## 3. P10 对象模型

Image canonical record：

```text
ImageContent
├─ resource_id                 稳定资源身份
├─ intrinsic_width/height     source plane 元数据
├─ source_rect?                absent = full source；精确 full rect 归一化为 absent
├─ content_mode                FIT / FILL / STRETCH，必须显式存在
└─ width/height                object-local display box，严格 > 0
```

坐标顺序固定为：`source plane → source_rect crop → contentMode mapping → local display box clip → Transform2D`。

Image 的 geometry bounds 是 display box，不随 decoded bitmap、缓存或 GPU 纹理改变。Resource 不可用时仍可计算 bounds、选择和变换，并显示缺失占位/诊断。

资源替换默认保留现有 display box、transform、placement 和 selection；如产品要改变盒子，必须在同一受控编辑中显式提交 `SetObjectSize`。

## 4. P11 Crop 交互会话

Crop 是一个拥有临时状态的 `ImageCropSession`，不是第二个 Document：

```text
Idle → EnterCrop(snapshot) → AdjustContent / AdjustCrop / AdjustScale
                         ├─ Done → one SetImageContent + optional SetObjectSize, one History entry
                         └─ Esc/Cancel/lost source/close → discard, zero canonical mutation
```

临时会话保存：进入时的完整 ImageContent、进入时的 display box、临时 source rect、content offset/scale、pointer identity/generation、view/surface revision 和失败诊断。每次更新只改变 preview/derived state。

首版交互默认：裁剪框为矩形且自由比例；裁剪框四边不能越过 source plane，最小有效范围为正且达到 renderer 可表示的最小面积；内容拖动不能让裁剪结果落到空 source 区域；缩放滑条使用产品层定义的正数范围，语义层只接收最终归一化 source rect。若产品需要固定比例、旋转或 focal point，应在 P11 冻结前另行决定，不能由平台自行解释。

`Done` 的语义提交必须是完整 ImageContent：保留 resource/intrinsic/mode，写入 normalized source rect，并同时写入裁后 display width/height。两项必须在同一个逻辑 History entry 中成功或全部拒绝；不能先写 crop 再写 size。`Cancel`、输入源丢失、Surface lost、关闭请求不生成 Operation。

## 5. P12/P13 语义与操作约束

- `InsertObjects`/`RestoreObjects`：校验 Image kind、完整 ImageContent、资源引用格式和 manifest 语义；bytes 可暂缺。
- `SetImageContent`：whole-content replacement；任何字段非法时整项 reject；不得 partial apply。
- `SetObjectSize`：对 Image 只更新 local display width/height，不改 intrinsic、source rect、mode 或 transform。
- 导入替换：产品层先完成 bytes 入 ResourceProvider/Manifest 的受控生命周期，再提交 Image Operation；Resource readiness 变化本身不产生 Operation/Undo。
- `Undo/Redo`：Image insert、replace、crop、resize 均使用现有 canonical Operation + History；Crop Cancel 没有 entry。
- 资源删除：若仍有 ImageContent 引用，manifest remove 失败；已移除资源的读回不得返回旧 Ready/BytesValidated receipt。
- 所有资源完成、通知、decode 和失效动作在 Runtime owner safe point 执行，外部线程只能入队事实，不能直接改 Runtime 容器。

## 6. 架构与数据流

```text
Windows file picker / Web File input
    → Host bytes + MIME/file fact
    → RuntimeFacade import intent
    → ResourceManifest/ResourceProvider owner boundary
    → ImageResourceResolver (SkCodec; PNG/JPEG/WebP)
    → Scene image geometry + damage/hit refresh
    → Direct Reference / Skia production render
```

Host 不构造 canonical ImageContent，不直接调用 SemanticDocument，也不持有 Crop 的第二份提交状态。RuntimeFacade 提供导入、替换、进入/更新/完成/取消 Crop、读回资源状态和 frame demand 的 typed commands；Composition 只负责 owner ordering、resource binding、derived invalidation 和 frame opportunity；平台执行返回的绘制任务并回传呈现事实。

导入失败必须区分：unsupported format、file read failure、manifest validation failure、hash/length mismatch、decode failure、resource binding stale、queue full、surface unavailable。失败不得留下 Image object 或 History entry。

Scene 需要将 `source_rect`、`content_mode` 和 display box 纳入 content digest/impact，确保 crop/mode 变化刷新 bounds/hit/damage；资源 Missing→Ready、Ready→Failed 只刷新 derived/render，不推进 SemanticGeneration。

## 7. Windows/Web 平台合同

Windows：文件选择器产生一次不可变 bytes 事实；Runtime owner 线程登记资源并安排 decode；D3D12/Skia 只消费 Composition 返回的 Image draw task。窗口 resize、DPI、Surface loss 不得改变 canonical ImageContent。

Web：`<input type=file accept="image/png,image/jpeg,image/webp">` 或等价用户动作产生 bytes；浏览器 Blob/ArrayBuffer 的生命周期由 host 管理，完成后通过 typed RuntimeFacade 交给 owner。WebGL/Skia 只接收同一 source/content mapping，不使用 CSS image 作为第二渲染路径。页面销毁/上下文丢失取消未完成导入，不能承诺异步资源在销毁后提交。

两端必须共用相同 ResourceManifest hash/metadata、ImageContent mapping 和 Crop commit 语义；差异只存在于文件采集、surface 和 GPU provider。

## 8. P20 验证矩阵候选

| ID | 失败模式 / 不变量 | 证据与判定 |
| --- | --- | --- |
| IMG-01 | PNG/JPEG/WebP 导入得到稳定 ResourceId、manifest entry 和 ImageContent | Windows/Web host contract + typed facade trace；三格式各一 fixture |
| IMG-02 | bytes 缺失/错误/不支持不会创建对象或 History | negative import oracle；object count/semantic generation/history unchanged |
| IMG-03 | sourceRect absent/full 等价；partial crop 在 unit square 内 | semantic golden + canonical bytes |
| IMG-04 | FIT/FILL/STRETCH mapping 顺序一致 | 400×200→300×300 geometry oracle，跨 C++/WASM |
| IMG-05 | Crop update 只有 preview；Done 原子提交；Cancel 零 mutation | session trace + before/after snapshot + Undo/Redo |
| IMG-06 | 普通缩放保持比例，modifier 才允许自由拉伸 | pointer sequence + final ImageContent/box |
| IMG-07 | resource Missing→Ready/Failed 只改 derived pixels | semantic generation/operation ledger unchanged；frame demand + damage updated |
| IMG-08 | stale/late/duplicate provider result 不覆盖当前绑定 | binding token oracle；old generation rejected |
| IMG-09 | crop/mode 影响 Scene digest、bounds、hit、damage | independent Scene projection and hit oracle |
| IMG-10 | Image selection/move/resize/Undo/Redo 与 Shape 既有路径一致 | mixed-object replay；Image-specific provider/readback |
| IMG-11 | Windows/Web 使用同一 semantic mapping | C++/WASM differential projection and render-geometry digest |
| IMG-12 | manifest remove with live reference fails closed | operation state validator + no partial mutation |

自动证据可关闭语义/Runtime/host contract；真实文件选择器、DPI、Web context loss 和 GPU 显示仍按现有 FC-13/平台物理边界记录，不由 CTest 冒充物理 PASS。

## 9. 实施前置与待决项

进入 P30/P31 前必须：

1. 解决仓库 `.aegis/authorities.json` 的 FC execution v0.2 与 Notion Current v0.3 对齐，并独立审查；
2. 确认 ImageContent Notion 页 Proposed Freeze 与仓库 `frozen-by-v1-final-gate` 的状态，以一份有效 Authority 为准；
3. P11 冻结 Crop 框比例、最小面积、边界、滑条范围、资源失败 UI 和拖动越界反馈；
4. P17 冻结 Windows/Web file bytes 生命周期、取消、context loss 和 provider 线程边界；
5. P20 冻结 `IMG-01..12` 的精确 fixture、oracle、命令和 blocking/corroborative 分类；
6. 之后再生成 FC03 P30/P31 package；在 package 被独立接受前不进入实现。

当前结论：**FC03 需求与现有 Runtime/Scene/Render/Resource 框架可实现，主要是既有边界接线和 Crop 会话，不需要新的 ObjectKind 或大架构模块；但仍是 DESIGN_CANDIDATE，未授权实现。**

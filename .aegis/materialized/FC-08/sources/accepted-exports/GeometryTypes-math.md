# 2. Canonical Numeric Type

<callout icon="🔒" color="green_bg">
<b>Proposed Freeze G-02：本页定义的 canonical geometry primitives 使用 IEEE-754 binary64 / </b><b><code>f64</code></b><b>。</b> <code>NaN</code>、<code>+Infinity</code>、<code>-Infinity</code> 非法；<code>-0.0</code> 在 validation/normalization 时转换为 <code>+0.0</code>。
</callout>

这继承 Common Wire Rules：

- committed scalar 使用 normalized exact equality；
- canonical equality 不使用 epsilon；
- interaction/runtime 可以使用 tolerance，但 tolerance 不得改变 committed semantic equality；
- 不做隐式 quantization、snap-to-grid、path simplify 或 matrix decompose/recompose。

专项 schema 若未来需要压缩/量化表示，必须定义<b>自己的 versioned representation</b>，不能悄悄把本页的 <code>f64</code> type 改成平台依赖的 <code>float</code>。

# 3. <code>Vec2</code>

<code>Vec2</code> 是两个 <code>f64</code> scalar 的基础二维 pair：

```proto
message Vec2 {
  double x = 1;
  double y = 2;
}
```

语义规则：

- <code>x</code>、<code>y</code> 必须 finite；
- <code>-0.0 → +0.0</code>；
- <code>Vec2</code> 本身不携带 coordinate-space tag；<b>使用它的字段必须声明 coordinate space</b>；
- 同一个 wire type 可以表达 point、offset、control point 或 normalized hint，但字段语义决定其角色；
- 不在 wire 中重复编码 <code>space = object_local/world/...</code>，避免把一个本来由 schema 决定的事实变成双真相。

示例：

```txt
Connector.FreePoint.point      → Connector object-local
AutoPerimeterAnchor.hint       → normalized domain
VectorPath control point       → VectorPath object-local
```

# 4. <code>Rect2</code>

<code>Rect2</code> 用于需要 canonical axis-aligned rectangle 的 leaf schema。它不是 Runtime Bounds cache。

```proto
message Rect2 {
  double x = 1;
  double y = 2;
  double width = 3;
  double height = 4;
}
```

<callout icon="🔒" color="green_bg">
<b>Proposed Freeze G-03：</b><b><code>Rect2</code></b><b> 使用 origin + extent 表达；</b><b><code>x/y</code></b><b> 是 min-origin，</b><b><code>width/height >= 0</code></b><b>。负 width/height 非法，不做自动翻转或 canonical repair。</b>
</callout>

规则：

- 四个 scalar 全部 finite；
- width/height 可为 0，除非使用该 Rect 的字段另行要求 <code>> 0</code>；
- <code>Rect2</code> 不自动排序两个角；
- 不允许同时存在 <code>(left,top,right,bottom)</code> 与 <code>(x,y,width,height)</code> 两套 canonical representation；
- <code>Rect2</code> 不等于 <code>GeometryBounds / VisualBounds / WorldBounds</code>；后者是 Runtime derived data。

# 5. <code>NormalizedRect</code>

保留 Reference IDL 当前定义：

```proto
message NormalizedRect {
  double x = 1;
  double y = 2;
  double width = 3;
  double height = 4;
}
```

<callout icon="🔒" color="green_bg">
<b>Proposed Freeze G-04：NormalizedRect 的合法域是 unit square：</b><b><code>0 <= x,y,width,height <= 1</code></b><b> 且 </b><b><code>x + width <= 1</code></b><b>、</b><b><code>y + height <= 1</code></b><b>。非法值 reject，不 clamp。</b>
</callout>

说明：

- generic <code>NormalizedRect</code> 允许 zero extent；
- <code>ImageContent.sourceRect</code> 若启用，可在 Image schema 再要求 <code>width > 0 && height > 0</code>；
- absent <code>sourceRect</code> 与 present full-rect <code>[0,0,1,1]</code> 是否语义等价，属于 ImageContent presence policy，不由本页自行折叠。

# 6. <code>Transform2D</code>

保留 Reference IDL 字段：

```proto
message Transform2D {
  double a = 1;
  double b = 2;
  double c = 3;
  double d = 4;
  double tx = 5;
  double ty = 6;
}
```

<callout icon="🔒" color="green_bg">
<b>Proposed Freeze G-05：</b><b><code>Transform2D</code></b><b> 使用 column-vector affine convention。</b> 对 object-local point <code>(x,y)</code>：<code>x' = a*x + c*y + tx</code>，<code>y' = b*x + d*y + ty</code>。
</callout>

等价矩阵：

```txt
| a  c  tx |
| b  d  ty |
| 0  0   1 |
```

输入 point 使用 column vector：

```txt
| x |
| y |
| 1 |
```

因此：

```txt
P_parent = T_local_to_parent × P_local
```

## 6.1 Identity

唯一 canonical identity：

```txt
a=1, b=0, c=0, d=1, tx=0, ty=0
```

所有 zero 在 canonical normalization 后均为 <code>+0.0</code>。

## 6.2 Composition Order

<callout icon="🔒" color="green_bg">
<b>Proposed Freeze G-06：层级 transform composition 为 </b><b><code>T_world = T_parent_world × T_local</code></b><b>。也就是 point 先应用 local transform，再应用 ancestor transform。</b>
</callout>

对于：

```txt
Document Root
  └─ Parent  (T_parent_world)
      └─ Child (T_local)
```

有：

```txt
P_world = T_parent_world × T_local × P_child_local
```

矩阵乘法不能通过“按字段逐项相加/相乘”替代。

## 6.3 Translation / Scale / Rotation

Translation：

```txt
T(tx,ty) = [1,0,0,1,tx,ty]
```

Scale：

```txt
S(sx,sy) = [sx,0,0,sy,0,0]
```

若某上层 API 使用 angle 构造 rotation，则 angle canonical unit 为 <b>radians</b>：

```txt
a = cos(theta)
b = sin(theta)
c = -sin(theta)
d = cos(theta)
```

由于 Canvas 坐标 Y 向下，<code>theta > 0</code> 在屏幕视觉上表现为 clockwise rotation。

本页不要求 canonical state 额外保存 rotation/scale/skew decomposition；<b>matrix 六参数是 authority</b>。

# 7. Singular Transform Policy

Transform 的 determinant：

```txt
det = a*d - b*c
```

<callout icon="🔒" color="green_bg">
<b>Proposed Freeze G-07：V1 canonical </b><b><code>Transform2D</code></b><b> 要求 </b><b><code>det != 0</code></b><b>；exact singular transform 非法。</b> 不使用 epsilon 作为 semantic rejection threshold。
</callout>

理由：Axiom 的多项 canonical/runtime 行为需要稳定的 parent ↔ object-local 映射，例如 object-local erase mask、precise hit-test、connector/local geometry editing。允许 exact singular transform 会让 inverse 从语义上不存在。

规则：

- <code>det == 0</code> → reject Operation / ObjectRecord；
- <code>det != 0</code> → semantic 上合法；
- extremely ill-conditioned 但非零的 matrix 可以被 runtime diagnostics 标记为风险，但不能因平台不同 epsilon 而一端接受、一端拒绝；
- Product/Editor 可以使用更严格的 soft limits 阻止用户制造极端 transform，但那不是 wire legality。

# 8. Exact Inverse Formula

当 <code>det != 0</code> 时：

```txt
inv.a  =  d / det
inv.b  = -b / det
inv.c  = -c / det
inv.d  =  a / det
inv.tx = (c*ty - d*tx) / det
inv.ty = (b*tx - a*ty) / det
```

语义：

```txt
P_local = inverse(T_local_to_parent) × P_parent
```

Reference implementation 应提供同一组 math helpers / golden vectors，避免 C++ / TS 各自猜矩阵布局。

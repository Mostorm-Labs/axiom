# UR1c1 Web 实际消费者补证 r1

本次是验证驱动修复，完成后回到控制审查；不进入 UR1c2。

既有产品链已接通：Web deferred pointer -> dirty -> SDK RAF/frame -> composition 编辑反馈 -> canonical SkiaRenderer -> shared PreparedGeometryCache -> SkiaInkLayers。只有真正执行叶子 get 的计数才证明消费者使用；两个 service 地址相同本身不够。

原 S0/S2 固定手势只有一个 SDK RAF，冷帧分别约600.6/553.1ms，超过32×16.67ms规划。until 已过期时不再发生计时器让出，后续32Move作为微任务追赶，然后Up结束erase，无法观察第二个局部帧。首帧完整1600×1000、16compact的optional层静态准入估算237.25MiB不符合32MiB；不得把这理解为实际96MiB分配失败。原冷帧性能观察必须保留。

在现有 verifier seam 修复有界事件循环/显示机会保证。保持同一输入点、顺序、逻辑时间戳、fixture、DPR、目标尺寸与生产SDK。只重跑S0/S2 erase-navigation，共资格/普通两个worker；其余原生、canonical、drag/cancel及独立28像素/8回环证明可保留，因为14份产品/原生测试源码和原SDK产物不变。

先用假时钟/显示循环证明旧600ms超期机制会把后续Move和Up挤入同一微任务轮次；新机制在Up之前给实际后续帧机会。不要只改timeout、加日志或直接调用cache来伪造成功。资格记录必须看到navigation后同一服务真实retained-ink get、不新增body build、有identity hit/完整比较；普通版不输出诊断。

原独立CPU oracle和AA规则继续有效。同完整snapshot字节、完整render request、oracle和资源身份已存在的独立reference可以精确复用；不同输入仍运行原oracle一次，保持10000ms界限。先保存全部新RGBA和snapshot，再判断。不要把缺失或失败像素变成PASS。

受控分支HEAD e69f2377；采样候选的历史全树0a5d8113不必与新增归档/任务/验证器后的全树相等。需记录新的 verifier/result identity，同时逐文件维持14份原产品hash和原normal/qualification SDK hash。原两窗口关闭；新窗口只允许UR1C1-R1-Q1/N1各一次、串行、180秒、3GiB，历史48次1770秒不变，未来按实际ceil计费。

实现会话是唯一源码/index写入者；控制会话仅写本任务包并独立审查。无需为已解决的内部细节再次询问授权；遇到真实源/authority/环境/证据阻断则保存原始失败并主动回报控制线程。没有commit、push、SDK重建或新平台发布授权。


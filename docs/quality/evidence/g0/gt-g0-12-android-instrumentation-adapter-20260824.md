# GT-G0-12 Android Instrumentation Adapter Evidence

> 任务：`GT-G0-12`（Notion locator：`WP-G0-12 / IH-12`）  
> Gate：G0；R 里程碑贡献：R1 / Verification Foundation  
> 当前状态：`Validating`

## 1. 来源与边界

本任务采用 Android `Activity/View/JNI` 薄适配器。Kotlin/Java 只负责 Activity、SurfaceView、MotionEvent history 和 instrumentation 生命周期；C++ 适配器负责 generation、生命周期和 `PointerSampleBatch` 归一化。Shared Runner 继续拥有 scenario `expected`、比较和 PASS/FAIL 权限。适配器不复制 28 条场景，不读取 `expected`，不把 observation 直接标成 PASS。

Surface 生命周期与 Runtime/Document 生命周期分离；surface lost/rebind 推进 generation，background 和 surface 丢失不删除 semantic Document；Arc preview 只记录 capability/ownership facts，不能改变 canonical semantics。macOS host 不参与 Android 任务，也不查找或安装 MSVC runtime。

## 2. Task Matrix 对账

| 项目 | 结果 |
| --- | --- |
| Reuse | GT-G0-08 native hooks/common host、GT-G0-09 共享 28 场景和 fixtures、既有 platform schemas/runner protocol |
| Modify | verification CMake、conformance CLI、workspace lock、Android package、instrumentation host 和 Evidence generator |
| Missing（本轮补齐） | Activity/View/JNI host、MotionEvent history→JNI→PointerSampleBatch、surface/device generation、facts-only 28-case adapter Evidence |
| Conflict（已避免） | 不把 Android adapter 变成产品 Runtime；不在 Kotlin/Java 复制场景期望；不把 emulator 结果写成 physical device PASS |
| Blocked | Hosted Android emulator workflow 尚未在远端运行；GitHub Actions artifact 待 workflow 所在提交进入远端后取得 |

## 3. 实现交付物

- `verification/native/platform/android/`：C++ profile、生命周期/surface/device adapter、输入批处理桥、host tests 和 JNI target；
- `verification/native/platform/android/instrumentation/`：可构建的 Android app/test APK，Activity/View/JNI instrumentation probe；
- `verification/packages/platform-harness-android/`：共享场景 facts-only TypeScript adapter；
- `verification/tools/run_android_instrumentation.py`：选择唯一在线设备、安装 APK、执行 instrumentation、记录脱敏设备快照；
- `verification/tools/generate_android_adapter_evidence.mjs`：生成 28 observations、28 open results、profile、applicability、summary、可校验 manifest；
- `.github/workflows/g0-android-instrumentation-adapter.yml`：Ubuntu hosted emulator、API 35、NDK 27.2.12479018、Gradle 8.11.1 的可重复验证工作流。

## 4. 当前验证结果

| 检查 | 结果 |
| --- | --- |
| C++ Android logical adapter build | Pass；AppleClang host 构建通过 |
| CTest | Pass；7/7 verification tests，包括 Android adapter test |
| Android TypeScript package | Pass；2/2 tests |
| Shared CLI | Pass；Android profile 与 28 场景 facts-only run 测试通过 |
| Evidence generator | Pass；28 applicable、无 expected/PASS 越权字段、manifest SHA-256 完整 |
| workspace/schema/diff checks | Pass；workspace valid、13 schemas/fixtures valid、`git diff --check` clean |
| Android instrumentation APK build | Pass；Gradle 8.11.1 + Temurin 17 + NDK 27.2.12479018；clean build 后 `libaxiom_verification_android_jni.so` 的三个 ELF `LOAD` 段均为 `0x4000`，app/test APK 的 `zipalign -P 16` 均通过；app APK SHA-256 为 `102f42e97ac32934df58f70df7b49ed8521cf32b578365fc0f20cf17d543de21` |
| Hosted Android emulator | Pending；workflow 已加入 16 KB ELF 检查，需 GitHub Actions 实际运行并取得 artifact |
| Android physical device | Pass（instrumentation seam）；Pixel 7 / Android 17 / arm64-v8a，`HARNESS_STARTED`，`emulator=false`，28 applicable scenarios；修复包从设备回拉后 ELF/APK 对齐复验通过，冷启动未再次出现 `PageSizeMismatchDialog`；独立 physical Evidence 已生成 |

## 5. Evidence 与状态

结构化 Evidence：[`verification/evidence/g0/gt-g0-12/`](../../../../verification/evidence/g0/gt-g0-12/)。当前工作区 Evidence 使用 `sourceCommit: WORKTREE`，不能作为最终 commit-bound Evidence。真实 Android instrumentation 结果通过 `AXIOM_ANDROID_INSTRUMENTATION_RESULT` 注入；只有结果状态为 `HARNESS_STARTED` 且 physical 模式明确 `device.emulator: false` 时，才可记录物理设备证据。16 KB ELF 校验脚本为 `verification/tools/check_android_16kb_elf.py`，APK 与 ELF 对齐必须同时满足。当前 Pixel 7 内核页大小为 4 KB；它证明兼容包可安装、可加载且不再报警，但 16 KB 兼容结论仍由 ELF `0x4000`、APK `zipalign -P 16` 和后续 hosted CI 静态门禁共同约束，不能把这台 4 KB 设备写成 16 KB 物理设备 Evidence。

GT-G0-12 当前状态：

- Design：`Pass`
- Implementation：`Validating`
- Validation：`Validating`
- Final：`Validating`

G0 与 R1 继续保持 `Validating`；本轮不进入 `GT-G0-13`。

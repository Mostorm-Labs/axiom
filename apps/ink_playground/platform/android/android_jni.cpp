#include <jni.h>
#include <android/native_window_jni.h>

#include <cstdint>
#include <vector>

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height);
void axiom_ink_android_destroy_host(void* handle);
int axiom_ink_android_platform_batch(void* handle, std::uint64_t pointerId,
                            const std::uint64_t* sequences,
                            const std::uint64_t* timestampNs, const float* x,
                            const float* y, const float* pressure,
                            const int* phases, std::size_t count, int family);
int axiom_ink_android_attach_surface(void* handle, ANativeWindow* window,
                                     int preview, std::uint32_t width,
                                     std::uint32_t height);
void axiom_ink_android_detach_surface(void* handle, int preview);
int axiom_ink_android_present_preview(void* handle);
int axiom_ink_android_resize(void* handle, std::uint32_t width, std::uint32_t height);
int axiom_ink_android_surface_lost(void* handle);
int axiom_ink_android_cancel_all(void* handle);
int axiom_ink_android_viewport_claimed(void* handle);
int axiom_ink_android_set_multi_contact_policy(void* handle, int policy);
int axiom_ink_android_multi_contact_policy(void* handle);
float axiom_ink_android_viewport_scale(void* handle);
float axiom_ink_android_viewport_center_x(void* handle);
float axiom_ink_android_viewport_center_y(void* handle);
float axiom_ink_android_viewport_translation_x(void* handle);
float axiom_ink_android_viewport_translation_y(void* handle);
std::uint64_t axiom_ink_android_brush_digest(void* handle);
std::uint64_t axiom_ink_android_brush_primitive_count(void* handle);
std::uint64_t axiom_ink_android_brush_family(void* handle);
int axiom_ink_android_brush_canonical_mutation(void* handle);
std::uint64_t axiom_ink_android_render_readback_count(void* handle);
std::uint64_t axiom_ink_android_render_cpu_copy_count(void* handle);
std::uint64_t axiom_ink_android_render_present_count(void* handle);
void axiom_ink_android_record_jni_copy(void* handle);
void axiom_ink_android_record_present(void* handle);
const char* axiom_ink_android_baseline_observation(void* handle, const char* reality,
                                                   const char* artifactIdentity);
int axiom_ink_android_brush_render(void* handle, std::uint32_t width,
                                   std::uint32_t height, std::uint8_t* rgba,
                                   std::uint32_t stride);
int axiom_ink_android_preview_render(void* handle, std::uint32_t width,
                                     std::uint32_t height, std::uint8_t* rgba,
                                     std::uint32_t stride);
float axiom_ink_android_brush_size(void* handle, std::uint64_t pointerId);
float axiom_ink_android_brush_opacity(void* handle, std::uint64_t pointerId);
int axiom_ink_android_brush_representation(void* handle, std::uint64_t pointerId);


JNIEXPORT jlong JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeCreate(
    JNIEnv*, jclass, jint width, jint height) {
  return reinterpret_cast<jlong>(axiom_ink_android_create_host(
      static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)));
}

JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeDestroy(
    JNIEnv*, jclass, jlong handle) {
  axiom_ink_android_destroy_host(reinterpret_cast<void*>(handle));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativePlatformBatch(
    JNIEnv* env, jclass, jlong handle, jint pointerId, jlongArray sequences,
    jlongArray times, jfloatArray xs, jfloatArray ys, jfloatArray pressures,
    jintArray phases, jint family) {
  const auto count = sequences == nullptr ? 0 : env->GetArrayLength(sequences);
  if (count <= 0 || times == nullptr || xs == nullptr || ys == nullptr ||
      pressures == nullptr || phases == nullptr || env->GetArrayLength(times) != count ||
      env->GetArrayLength(xs) != count || env->GetArrayLength(ys) != count ||
      env->GetArrayLength(pressures) != count || env->GetArrayLength(phases) != count) return 0;
  std::vector<jlong> sequenceValues(static_cast<std::size_t>(count));
  std::vector<jlong> timeValues(static_cast<std::size_t>(count));
  std::vector<jfloat> xValues(static_cast<std::size_t>(count));
  std::vector<jfloat> yValues(static_cast<std::size_t>(count));
  std::vector<jfloat> pressureValues(static_cast<std::size_t>(count));
  std::vector<jint> phaseValues(static_cast<std::size_t>(count));
  env->GetLongArrayRegion(sequences, 0, count, sequenceValues.data());
  env->GetLongArrayRegion(times, 0, count, timeValues.data());
  env->GetFloatArrayRegion(xs, 0, count, xValues.data());
  env->GetFloatArrayRegion(ys, 0, count, yValues.data());
  env->GetFloatArrayRegion(pressures, 0, count, pressureValues.data());
  env->GetIntArrayRegion(phases, 0, count, phaseValues.data());
  std::vector<std::uint64_t> nativeSequences(sequenceValues.begin(), sequenceValues.end());
  std::vector<std::uint64_t> nativeTimes(timeValues.begin(), timeValues.end());
  std::vector<float> nativeXs(xValues.begin(), xValues.end());
  std::vector<float> nativeYs(yValues.begin(), yValues.end());
  std::vector<float> nativePressures(pressureValues.begin(), pressureValues.end());
  std::vector<int> nativePhases(phaseValues.begin(), phaseValues.end());
  return axiom_ink_android_platform_batch(
      reinterpret_cast<void*>(handle), static_cast<std::uint64_t>(pointerId),
      nativeSequences.data(), nativeTimes.data(), nativeXs.data(), nativeYs.data(),
      nativePressures.data(), nativePhases.data(), static_cast<std::size_t>(count), family);
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeAttachSurface(
    JNIEnv* env, jclass, jlong handle, jobject surface, jboolean preview,
    jint width, jint height) {
  if (surface == nullptr || width <= 0 || height <= 0) return 0;
  auto* window = ANativeWindow_fromSurface(env, surface);
  if (window == nullptr) return 0;
  const auto attached = axiom_ink_android_attach_surface(
      reinterpret_cast<void*>(handle), window, preview ? 1 : 0,
      static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height));
  if (attached == 0) ANativeWindow_release(window);
  return attached;
}

JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeDetachSurface(
    JNIEnv*, jclass, jlong handle, jboolean preview) {
  axiom_ink_android_detach_surface(reinterpret_cast<void*>(handle), preview ? 1 : 0);
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativePresentPreview(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_present_preview(reinterpret_cast<void*>(handle));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeResize(
    JNIEnv*, jclass, jlong handle, jint width, jint height) {
  return axiom_ink_android_resize(reinterpret_cast<void*>(handle),
                                  static_cast<std::uint32_t>(width),
                                  static_cast<std::uint32_t>(height));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeSurfaceLost(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_surface_lost(reinterpret_cast<void*>(handle));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeCancelAll(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_cancel_all(reinterpret_cast<void*>(handle));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeViewportClaimed(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_viewport_claimed(reinterpret_cast<void*>(handle));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeSetMultiContactPolicy(
    JNIEnv*, jclass, jlong handle, jint policy) {
  return axiom_ink_android_set_multi_contact_policy(
      reinterpret_cast<void*>(handle), policy);
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeMultiContactPolicy(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_multi_contact_policy(reinterpret_cast<void*>(handle));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeViewportScale(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_viewport_scale(reinterpret_cast<void*>(handle));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeViewportCenterX(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_viewport_center_x(reinterpret_cast<void*>(handle));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeViewportCenterY(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_viewport_center_y(reinterpret_cast<void*>(handle));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeViewportTranslationX(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_viewport_translation_x(reinterpret_cast<void*>(handle));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeViewportTranslationY(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_viewport_translation_y(reinterpret_cast<void*>(handle));
}

JNIEXPORT jlong JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushDigest(
    JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(axiom_ink_android_brush_digest(
      reinterpret_cast<void*>(handle)));
}

JNIEXPORT jlong JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushPrimitiveCount(
    JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(axiom_ink_android_brush_primitive_count(
      reinterpret_cast<void*>(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushFamily(
    JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(axiom_ink_android_brush_family(
      reinterpret_cast<void*>(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushCanonicalMutation(
    JNIEnv*, jclass, jlong handle) {
  return axiom_ink_android_brush_canonical_mutation(
      reinterpret_cast<void*>(handle));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushSize(
    JNIEnv*, jclass, jlong handle, jint pointerId) {
  return axiom_ink_android_brush_size(reinterpret_cast<void*>(handle),
                                       static_cast<std::uint64_t>(pointerId));
}

JNIEXPORT jfloat JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushOpacity(
    JNIEnv*, jclass, jlong handle, jint pointerId) {
  return axiom_ink_android_brush_opacity(reinterpret_cast<void*>(handle),
                                          static_cast<std::uint64_t>(pointerId));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushRepresentation(
    JNIEnv*, jclass, jlong handle, jint pointerId) {
  return axiom_ink_android_brush_representation(reinterpret_cast<void*>(handle),
                                                static_cast<std::uint64_t>(pointerId));
}

JNIEXPORT jbyteArray JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushRgba(
    JNIEnv* env, jclass, jlong handle, jint width, jint height) {
  if (width <= 0 || height <= 0) return nullptr;
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4U);
  if (axiom_ink_android_brush_render(reinterpret_cast<void*>(handle),
                                     static_cast<std::uint32_t>(width),
                                     static_cast<std::uint32_t>(height), rgba.data(),
                                     static_cast<std::uint32_t>(width) * 4U) == 0) return nullptr;
  jbyteArray result = env->NewByteArray(static_cast<jsize>(rgba.size()));
  if (result == nullptr) return nullptr;
  env->SetByteArrayRegion(result, 0, static_cast<jsize>(rgba.size()),
                          reinterpret_cast<const jbyte*>(rgba.data()));
  axiom_ink_android_record_jni_copy(reinterpret_cast<void*>(handle));
  return result;
}

JNIEXPORT jbyteArray JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativePreviewRgba(
    JNIEnv* env, jclass, jlong handle, jint width, jint height) {
  if (width <= 0 || height <= 0) return nullptr;
  std::vector<std::uint8_t> rgba(static_cast<std::size_t>(width) * height * 4U);
  if (axiom_ink_android_preview_render(reinterpret_cast<void*>(handle),
                                       static_cast<std::uint32_t>(width),
                                       static_cast<std::uint32_t>(height), rgba.data(),
                                       static_cast<std::uint32_t>(width) * 4U) == 0) return nullptr;
  auto result = env->NewByteArray(static_cast<jsize>(rgba.size()));
  if (result != nullptr) env->SetByteArrayRegion(result, 0, static_cast<jsize>(rgba.size()),
                                                  reinterpret_cast<const jbyte*>(rgba.data()));
  return result;
}

JNIEXPORT jlong JNICALL Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeRenderReadbackCount(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(axiom_ink_android_render_readback_count(reinterpret_cast<void*>(handle)));
}
JNIEXPORT jlong JNICALL Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeRenderCpuCopyCount(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(axiom_ink_android_render_cpu_copy_count(reinterpret_cast<void*>(handle)));
}
JNIEXPORT jlong JNICALL Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeRenderPresentCount(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(axiom_ink_android_render_present_count(reinterpret_cast<void*>(handle)));
}
JNIEXPORT void JNICALL Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeRecordPresent(JNIEnv*, jclass, jlong handle) {
  axiom_ink_android_record_present(reinterpret_cast<void*>(handle));
}
JNIEXPORT jstring JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBaselineObservation(
    JNIEnv* env, jclass, jlong handle, jstring reality, jstring artifactIdentity) {
  if (reality == nullptr || artifactIdentity == nullptr) return nullptr;
  const char* realityChars = env->GetStringUTFChars(reality, nullptr);
  const char* artifactChars = env->GetStringUTFChars(artifactIdentity, nullptr);
  if (realityChars == nullptr || artifactChars == nullptr) {
    if (realityChars != nullptr) env->ReleaseStringUTFChars(reality, realityChars);
    if (artifactChars != nullptr) env->ReleaseStringUTFChars(artifactIdentity, artifactChars);
    return nullptr;
  }
  const char* json = axiom_ink_android_baseline_observation(
      reinterpret_cast<void*>(handle), realityChars, artifactChars);
  env->ReleaseStringUTFChars(reality, realityChars);
  env->ReleaseStringUTFChars(artifactIdentity, artifactChars);
  return json == nullptr ? nullptr : env->NewStringUTF(json);
}
}

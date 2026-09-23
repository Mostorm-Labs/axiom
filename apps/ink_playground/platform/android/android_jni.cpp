#include <jni.h>

#include <cstdint>
#include <vector>

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height);
void axiom_ink_android_destroy_host(void* handle);
int axiom_ink_android_platform_batch(void* handle, std::uint64_t pointerId,
                            std::uint64_t sequence, std::uint64_t timestampNs,
                            float x, float y, float pressure, int phase, int family,
                            float contentX, float contentY);
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
int axiom_ink_android_brush_render(void* handle, std::uint32_t width,
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
    JNIEnv*, jclass, jlong handle, jint pointerId, jlong sequence, jlong timeNs,
    jfloat x, jfloat y, jfloat pressure, jint phase, jint family,
    jfloat contentX, jfloat contentY) {
  return axiom_ink_android_platform_batch(reinterpret_cast<void*>(handle),
      static_cast<std::uint64_t>(pointerId), static_cast<std::uint64_t>(sequence),
      static_cast<std::uint64_t>(timeNs), x, y, pressure, phase, family, contentX, contentY);
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
  return result;
}
}

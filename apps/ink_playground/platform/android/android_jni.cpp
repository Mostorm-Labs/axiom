#include <jni.h>

#include <cstdint>

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height);
void axiom_ink_android_destroy_host(void* handle);
int axiom_ink_android_begin(void* handle, std::uint64_t pointerId, std::uint64_t strokeId);
int axiom_ink_android_sample(void* handle, std::uint64_t pointerId,
                            std::uint64_t sequence, std::uint64_t timestampNs,
                            float x, float y, float pressure, int phase, int tool);
int axiom_ink_android_commit(void* handle, std::uint64_t pointerId, std::uint64_t strokeId);
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
int axiom_ink_android_brush_begin(void* handle, std::uint64_t pointerId,
                                  std::uint64_t family);
int axiom_ink_android_brush_sample(void* handle, std::uint64_t pointerId,
                                   std::uint64_t sequence, float x, float y,
                                   float pressure);
int axiom_ink_android_brush_finish(void* handle, std::uint64_t pointerId);
std::uint64_t axiom_ink_android_brush_digest(void* handle);
std::uint64_t axiom_ink_android_brush_primitive_count(void* handle);
std::uint64_t axiom_ink_android_brush_family(void* handle);
int axiom_ink_android_brush_canonical_mutation(void* handle);
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
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBegin(
    JNIEnv*, jclass, jlong handle, jint pointerId, jlong strokeId) {
  return axiom_ink_android_begin(reinterpret_cast<void*>(handle),
                                 static_cast<std::uint64_t>(pointerId),
                                 static_cast<std::uint64_t>(strokeId));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeMotion(
    JNIEnv*, jclass, jlong handle, jint pointerId, jlong sequence,
    jlong timeNs, jfloat x, jfloat y, jfloat pressure, jint down, jint up,
    jint tool) {
  const int phase = down != 0 ? 1 : up != 0 ? 3 : 2;
  return axiom_ink_android_sample(
      reinterpret_cast<void*>(handle), static_cast<std::uint64_t>(pointerId),
      static_cast<std::uint64_t>(sequence), static_cast<std::uint64_t>(timeNs),
      x, y, pressure, phase, tool);
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeCommit(
    JNIEnv*, jclass, jlong handle, jint pointerId, jlong strokeId) {
  return axiom_ink_android_commit(reinterpret_cast<void*>(handle),
                                  static_cast<std::uint64_t>(pointerId),
                                  static_cast<std::uint64_t>(strokeId));
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
}JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushBegin(
    JNIEnv*, jclass, jlong handle, jint pointerId, jint family) {
  return axiom_ink_android_brush_begin(reinterpret_cast<void*>(handle),
                                       static_cast<std::uint64_t>(pointerId),
                                       static_cast<std::uint64_t>(family));
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushSample(
    JNIEnv*, jclass, jlong handle, jint pointerId, jlong sequence,
    jfloat x, jfloat y, jfloat pressure) {
  return axiom_ink_android_brush_sample(reinterpret_cast<void*>(handle),
                                         static_cast<std::uint64_t>(pointerId),
                                         static_cast<std::uint64_t>(sequence),
                                         x, y, pressure);
}

JNIEXPORT jint JNICALL
Java_dev_mostorm_axiom_inkplayground_InkPlaygroundView_nativeBrushFinish(
    JNIEnv*, jclass, jlong handle, jint pointerId) {
  return axiom_ink_android_brush_finish(reinterpret_cast<void*>(handle),
                                         static_cast<std::uint64_t>(pointerId));
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
}

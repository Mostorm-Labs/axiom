#include <jni.h>

#include <android/native_window_jni.h>

#include <cstdint>
#include <sstream>
#include <string>
#include <vector>

#include <axiom/verification/android_harness_adapter.hpp>

namespace {
using axiom::verification::platform::AndroidHarnessAdapter;
using axiom::verification::platform::AndroidMotionSample;

struct State {
  AndroidHarnessAdapter adapter;
  std::size_t pointer_batch_count{0};
  bool surface_seen{false};
};

State* state(JNIEnv* env, jobject object) {
  const jclass klass = env->GetObjectClass(object);
  const jfieldID field = env->GetFieldID(klass, "nativeScope", "J");
  return reinterpret_cast<State*>(static_cast<std::uintptr_t>(
      env->GetLongField(object, field)));
}

std::string snapshot(State& value) {
  std::ostringstream out;
  out << "{"
      << "\"format\":\"axiom-android-instrumentation-snapshot-v1\","
      << "\"surfaceGeneration\":" << value.adapter.surface_generation() << ","
      << "\"deviceGeneration\":" << value.adapter.device_generation() << ","
      << "\"metricsGeneration\":" << value.adapter.metrics_generation() << ","
      << "\"documentAttached\":"
      << (value.adapter.document_attached() ? "true" : "false") << ","
      << "\"pointerBatchCount\":" << value.pointer_batch_count << ","
      << "\"nativeWindowToken\":" << value.adapter.native_window_token()
      << "}";
  return out.str();
}
}  // namespace

extern "C" JNIEXPORT jlong JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeCreate(
    JNIEnv*, jobject) {
  return static_cast<jlong>(
      reinterpret_cast<std::uintptr_t>(new State()));
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeDestroy(
    JNIEnv* env, jobject object) {
  const jclass klass = env->GetObjectClass(object);
  const jfieldID field = env->GetFieldID(klass, "nativeScope", "J");
  auto* value = state(env, object);
  delete value;
  env->SetLongField(object, field, 0);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeCreateCanvas(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.create_canvas();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeAttachHost(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.attach_host();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeAttachDocument(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.attach_document();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeBackground(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.app_background();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeForeground(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.app_foreground();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeSurfaceAvailable(
    JNIEnv* env, jobject object, jobject surface, jint width, jint height) {
  auto* value = state(env, object);
  std::uintptr_t token = 1;
  if (surface != nullptr) {
    if (auto* window = ANativeWindow_fromSurface(env, surface)) {
      token = reinterpret_cast<std::uintptr_t>(window);
      ANativeWindow_release(window);
    }
  }
  if (value->adapter.surface_ready()) {
    return;
  }
  if (value->surface_seen) {
    value->adapter.rebind_surface(token);
  } else {
    value->adapter.surface_available(token);
    value->surface_seen = true;
  }
  value->adapter.update_metrics({
      static_cast<std::uint32_t>(width),
      static_cast<std::uint32_t>(height),
      static_cast<std::uint32_t>(width),
      static_cast<std::uint32_t>(height),
      1.0F,
      0,
      true,
      false,
  });
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeSurfaceLost(
    JNIEnv* env, jobject object) {
  auto* value = state(env, object);
  value->adapter.surface_lost(value->adapter.surface_generation());
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeMetricsChanged(
    JNIEnv* env, jobject object, jint width, jint height, jfloat density,
    jint orientation) {
  state(env, object)->adapter.update_metrics({
      static_cast<std::uint32_t>(width),
      static_cast<std::uint32_t>(height),
      static_cast<std::uint32_t>(width * density),
      static_cast<std::uint32_t>(height * density),
      density,
      static_cast<std::uint32_t>(orientation),
      true,
      false,
  });
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeDeviceLost(
    JNIEnv* env, jobject object) {
  auto* value = state(env, object);
  value->adapter.device_lost(value->adapter.device_generation());
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeDeviceRecover(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.device_recover();
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativePointerBatch(
    JNIEnv* env, jobject object, jfloatArray samples, jint count) {
  if (samples == nullptr || count <= 0) return;
  const jsize length = env->GetArrayLength(samples);
  if (length < count * 7) return;
  std::vector<AndroidMotionSample> normalized;
  normalized.reserve(static_cast<std::size_t>(count));
  std::vector<jfloat> values(static_cast<std::size_t>(length));
  env->GetFloatArrayRegion(samples, 0, length, values.data());
  for (jint index = 0; index < count; ++index) {
    const auto offset = static_cast<std::size_t>(index) * 7;
    normalized.push_back({
        values[offset], values[offset + 1], values[offset + 2],
        values[offset + 3], values[offset + 4],
        static_cast<std::uint64_t>(values[offset + 5]),
        static_cast<std::int32_t>(values[offset + 6]),
    });
  }
  auto* value = state(env, object);
  const auto current = normalized.back();
  normalized.pop_back();
  (void)axiom::verification::platform::normalize_android_motion_history(
      "android-motion-batch", normalized, current);
  value->pointer_batch_count = static_cast<std::size_t>(count);
}

extern "C" JNIEXPORT void JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeDestroyCanvas(
    JNIEnv* env, jobject object) {
  state(env, object)->adapter.destroy_canvas();
}

extern "C" JNIEXPORT jstring JNICALL
Java_dev_mostorm_axiom_verification_android_HarnessCanvasView_nativeSnapshotJson(
    JNIEnv* env, jobject object) {
  const auto value = snapshot(*state(env, object));
  return env->NewStringUTF(value.c_str());
}

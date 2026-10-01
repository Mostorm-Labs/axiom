#include "ink_playground_host.hpp"
#include "platform_brush_baseline_observation.hpp"
#include "android_pointer_identity.hpp"
#include "android_egl_surface_provider.hpp"
#include "canvas/render/skia_renderer.hpp"
#include "canvas/render/skia_surface_provider.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <android/native_window.h>

namespace {
using canvas::ink_playground::InkPlaygroundHost;
struct AndroidHost final {
  std::unique_ptr<InkPlaygroundHost> host = std::make_unique<InkPlaygroundHost>();
  std::uint64_t serial = 0, digest = 0, primitiveCount = 0, family = 1;
  bool canonicalMutation = false;
  std::uint64_t cpuCopyCount = 0;
  std::uint64_t presentCount = 0;
  std::unordered_set<std::uint64_t> activePointers;
  canvas::ink_playground::AndroidEglSkiaSurfaceProvider* canonicalGpu = nullptr;
  canvas::ink_playground::AndroidEglSkiaSurfaceProvider* previewGpu = nullptr;
#if defined(CANVAS_RENDER_HAS_SKIA)
  canvas::render::SkiaRenderer renderer;
  std::vector<std::uint8_t> rgba;
#endif
};
AndroidHost* asHost(void* value) { return static_cast<AndroidHost*>(value); }
bool initPrograms(AndroidHost&) { return true; }
bool submitBatch(AndroidHost& value, std::uint64_t pointerId,
                 const std::uint64_t* sequences, const std::uint64_t* timestampNs,
                 const float* x, const float* y, const float* pressure,
                 const int* phases, std::size_t count, int family) {
  if (sequences == nullptr || timestampNs == nullptr || x == nullptr || y == nullptr ||
      pressure == nullptr || phases == nullptr || count == 0U) return false;
  const auto identity = canvas::ink_playground::androidPointerIdentity(pointerId); if (!identity) return false;
  // Some adb/input implementations deliver a repeated ACTION_DOWN while a
  // contact is being synthesized. Keep the platform bridge's lifecycle
  // canonical: only the first down begins a generation; repeats are moves.
  const auto logicalPointer = *identity;
  canvas::input::PlatformPointerBatch batch;
  batch.samples.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    auto phase = phases[index];
    if (phase == 1 && value.activePointers.contains(logicalPointer)) phase = 2;
    batch.samples.push_back({1U, *identity, sequences[index], timestampNs[index],
      x[index], y[index], pressure[index], 0.0F, 0.0F, {}, {},
      canvas::input::SampleProvenance::kConfirmedCurrent,
      phase == 1 ? canvas::input::PointerPhase::kDown :
      phase == 3 ? canvas::input::PointerPhase::kUp : canvas::input::PointerPhase::kMove});
  }
  const bool accepted = value.host->acceptPlatformBatch(batch, timestampNs[count - 1U]);
  if (!accepted) return false;
  for (const auto& sample : batch.samples) {
    if (sample.phase == canvas::input::PointerPhase::kDown) value.activePointers.insert(logicalPointer);
    if (sample.phase == canvas::input::PointerPhase::kUp) value.activePointers.erase(logicalPointer);
  }
  value.family = static_cast<std::uint64_t>(family);
  if (batch.samples.back().phase == canvas::input::PointerPhase::kUp) {
    value.digest = value.host->brushDigest(); value.primitiveCount = value.host->brushPrimitiveCount();
    if (value.canonicalGpu != nullptr) {
      const auto frameId = value.host->canonicalFrameCount() + 1U;
      if (!value.host->presentCanonicalFrame(frameId, 0.0)) return false;
    }
  }
  return true;
}
}

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height) {
  if (width == 0U || height == 0U) return nullptr; auto value = std::make_unique<AndroidHost>();
  if (!initPrograms(*value) || !value->host->bindSurface(width, height)) return nullptr;
#if defined(CANVAS_RENDER_HAS_SKIA)
  value->rgba.resize(static_cast<std::size_t>(width) * height * 4U);
#endif
  return value.release();
}
void axiom_ink_android_destroy_host(void* handle) { delete asHost(handle); }
int axiom_ink_android_platform_batch(void* handle, std::uint64_t pointerId,
    const std::uint64_t* sequences, const std::uint64_t* timestampNs,
    const float* x, const float* y, const float* pressure, const int* phases,
    std::size_t count, int family) {
  auto* value = asHost(handle); return value != nullptr && submitBatch(
      *value, pointerId, sequences, timestampNs, x, y, pressure, phases, count, family) ? 1 : 0;
}

int axiom_ink_android_attach_surface(void* handle, ANativeWindow* window,
                                     int preview, std::uint32_t width,
                                     std::uint32_t height) {
  auto* value = asHost(handle);
  if (value == nullptr || window == nullptr || width == 0U || height == 0U) return 0;
  // SurfaceView may deliver surfaceChanged more than once for the same
  // native window. Rebind the already registered provider instead of trying
  // to register a duplicate profile (which would leave the lost old provider
  // as the active target).
  auto* existing = preview ? value->previewGpu : value->canonicalGpu;
  if (existing != nullptr) {
    if (!existing->attach(window, width, height)) return 0;
    return (preview ? value->host->rebindPreviewSurface()
                    : value->host->rebindSurface()) ? 1 : 0;
  }
  auto provider = std::make_unique<canvas::ink_playground::AndroidEglSkiaSurfaceProvider>();
  if (!provider->attach(window, width, height)) return 0;
  auto* raw = provider.get();
  const bool registered = preview
      ? value->host->registerPreviewSurfaceProvider("android-gles-preview", std::move(provider))
      : value->host->registerSurfaceProvider("android-gles-canonical", std::move(provider));
  if (!registered) return 0;
  if (preview) value->previewGpu = raw;
  else value->canonicalGpu = raw;
  return 1;
}

void axiom_ink_android_detach_surface(void* handle, int preview) {
  auto* value = asHost(handle);
  if (value == nullptr) return;
  auto*& provider = preview ? value->previewGpu : value->canonicalGpu;
  if (provider != nullptr) {
    (void)provider->lose();
  }
}

int axiom_ink_android_present_preview(void* handle) {
  auto* value = asHost(handle);
  return value != nullptr && value->host->presentBrushPreview() ? 1 : 0;
}
int axiom_ink_android_resize(void* handle, std::uint32_t width, std::uint32_t height) {
  auto* value = asHost(handle); if (value == nullptr || !value->host->resizeSurface(width, height)) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  value->rgba.resize(static_cast<std::size_t>(width) * height * 4U);
#endif
  return 1;
}
int axiom_ink_android_surface_lost(void* handle) { auto* value = asHost(handle); return value != nullptr && value->host->loseSurface(); }
int axiom_ink_android_cancel_all(void* handle) { auto* value = asHost(handle); if (value == nullptr) return 0; value->host->cancelAllPointers(); return 1; }
int axiom_ink_android_viewport_claimed(void* handle) { auto* value = asHost(handle); return value != nullptr && value->host->viewportGestureClaimed(); }
int axiom_ink_android_set_multi_contact_policy(void* handle, int policy) { auto* value = asHost(handle); return value != nullptr && policy >= 0 && policy <= 2 && value->host->setMultiContactPolicy(static_cast<canvas::interaction::MultiContactPolicy>(policy)); }
int axiom_ink_android_multi_contact_policy(void* handle) { auto* value = asHost(handle); return value == nullptr ? -1 : static_cast<int>(value->host->multiContactPolicy()); }
float axiom_ink_android_viewport_scale(void* handle) { auto* value = asHost(handle); return value == nullptr ? 1.0F : value->host->viewportGesture().scale; }
float axiom_ink_android_viewport_center_x(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0.0F : value->host->viewportGesture().centerX; }
float axiom_ink_android_viewport_center_y(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0.0F : value->host->viewportGesture().centerY; }
float axiom_ink_android_viewport_translation_x(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0.0F : value->host->viewportGesture().translationX; }
float axiom_ink_android_viewport_translation_y(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0.0F : value->host->viewportGesture().translationY; }
int axiom_ink_android_brush_render(void* handle, std::uint32_t width, std::uint32_t height, std::uint8_t* rgba, std::uint32_t stride) {
  auto* value = asHost(handle); if (value == nullptr || rgba == nullptr || stride < width * 4U) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto* provider = value->host->activeSurfaceProvider();
  if (provider == nullptr) return 0;
  const auto info = provider->describe();
  // A window-backed GLES provider has no safe readback path.  Evidence is
  // best-effort and runs off the UI thread; fail closed before touching the
  // EGL context so it cannot call canonical present concurrently with input.
  if (!info.capabilities.supportsReadback) return 0;
  if (info.metrics.physicalWidth != width || info.metrics.physicalHeight != height) {
    if (!value->host->resizeSurface(width, height)) return 0;
    value->rgba.resize(static_cast<std::size_t>(width) * height * 4U);
    provider = value->host->activeSurfaceProvider();
  }
  // The commit path may already have presented frame 1 before the first
  // platform readback. Use the runtime's canonical frame ordinal to avoid
  // resubmitting the same PresentationTracker frame identity.
  const auto frameId = value->host->canonicalFrameCount() + 1U;
  if (!value->host->presentCanonicalFrame(frameId, 0.0)) return 0;
  value->serial = frameId;
  if (provider->readbackRgba(value->rgba).code != canvas::render::BackendSubmissionCode::kAccepted) return 0;
  const auto pixels = std::span<const std::uint8_t>(value->rgba.data(), value->rgba.size());
  for (std::uint32_t y = 0; y < height; ++y) {
    std::copy_n(pixels.data() + static_cast<std::size_t>(y) * width * 4U,
                static_cast<std::size_t>(width) * 4U,
                rgba + static_cast<std::size_t>(y) * stride);
  }
  ++value->cpuCopyCount;
  return 1;
#else
  (void)width; (void)height; (void)stride; return 0;
#endif
}
int axiom_ink_android_preview_render(void* handle, std::uint32_t width, std::uint32_t height, std::uint8_t* rgba, std::uint32_t stride) {
  auto* value = asHost(handle); if (value == nullptr || rgba == nullptr || stride < width * 4U) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  auto* provider = value->host->previewSurfaceProvider();
  if (provider == nullptr) return 0;
  std::vector<std::uint8_t> pixels(static_cast<std::size_t>(width) * height * 4U);
  if (provider->readbackRgba(pixels).code != canvas::render::BackendSubmissionCode::kAccepted) return 0;
  for (std::uint32_t y = 0; y < height; ++y)
    std::copy_n(pixels.data() + static_cast<std::size_t>(y) * width * 4U,
                static_cast<std::size_t>(width) * 4U,
                rgba + static_cast<std::size_t>(y) * stride);
  return provider->overlayVisible() ? 1 : 0;
#else
  (void)width; (void)height; (void)stride; return 0;
#endif
}
std::uint64_t axiom_ink_android_render_readback_count(void* handle) {
  auto* value = asHost(handle);
#if defined(CANVAS_RENDER_HAS_SKIA)
  return value == nullptr || value->host->activeSurfaceProvider() == nullptr ? 0 : value->host->activeSurfaceProvider()->readbackCount();
#else
  return 0;
#endif
}
std::uint64_t axiom_ink_android_render_cpu_copy_count(void* handle) {
  auto* value = asHost(handle);
  if (value == nullptr) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  return value->cpuCopyCount + (value->host->activeSurfaceProvider() ? value->host->activeSurfaceProvider()->cpuCopyCount() : 0);
#else
  return value->cpuCopyCount;
#endif
}
std::uint64_t axiom_ink_android_render_present_count(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->presentCount; }
void axiom_ink_android_record_jni_copy(void* handle) { auto* value = asHost(handle); if (value != nullptr) ++value->cpuCopyCount; }
void axiom_ink_android_record_present(void* handle) { auto* value = asHost(handle); if (value != nullptr) ++value->presentCount; }
const char* axiom_ink_android_baseline_observation(void* handle, const char* reality,
                                                   const char* artifactIdentity) {
  static thread_local std::string json;
  auto* value = asHost(handle);
  if (value == nullptr || reality == nullptr || artifactIdentity == nullptr) return nullptr;
#if defined(CANVAS_RENDER_HAS_SKIA)
  const auto submissions = value->renderer.submissionCount();
  const auto readbacks = value->host->activeSurfaceProvider() ? value->host->activeSurfaceProvider()->readbackCount() : 0;
  const auto copies = value->cpuCopyCount +
      (value->host->activeSurfaceProvider() ? value->host->activeSurfaceProvider()->cpuCopyCount() : 0);
#else
  const std::uint64_t submissions = 0;
  const std::uint64_t readbacks = 0;
  const auto copies = value->cpuCopyCount;
#endif
  json = canvas::ink_playground::platformBrushBaselineObservationJson(
      "android", reality, artifactIdentity, *value->host,
      {"MotionEvent.history→JNI→PlatformPointerBatch", "C++ InkPlaygroundHost",
       "Skia raster", "CPU raster buffer→Java byte[]→Bitmap→Canvas", submissions,
       readbacks, copies, value->presentCount, "true"});
  return json.c_str();
}
float axiom_ink_android_brush_size(void* handle, std::uint64_t pointer) { (void)handle; (void)pointer; return 1.0F; }
float axiom_ink_android_brush_opacity(void* handle, std::uint64_t pointer) { (void)handle; (void)pointer; return 1.0F; }
int axiom_ink_android_brush_representation(void* handle, std::uint64_t pointer) { (void)handle; (void)pointer; return 1; }
std::uint64_t axiom_ink_android_brush_digest(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->digest; }
std::uint64_t axiom_ink_android_brush_primitive_count(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->primitiveCount; }
std::uint64_t axiom_ink_android_brush_family(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->family; }
int axiom_ink_android_brush_canonical_mutation(void* handle) { (void)handle; return 1; }
}

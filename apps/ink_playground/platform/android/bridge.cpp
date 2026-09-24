#include "ink_playground_host.hpp"
#include "android_pointer_identity.hpp"
#include "canvas/render/skia_ink_backend.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>
#include <vector>

namespace {
using canvas::ink_playground::InkPlaygroundHost;
struct AndroidHost final {
  std::unique_ptr<InkPlaygroundHost> host = std::make_unique<InkPlaygroundHost>();
  std::uint64_t serial = 0, digest = 0, primitiveCount = 0, family = 1;
  bool canonicalMutation = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaInkBackend> renderer = std::make_unique<canvas::render::SkiaInkBackend>();
#endif
};
AndroidHost* asHost(void* value) { return static_cast<AndroidHost*>(value); }
bool initPrograms(AndroidHost&) { return true; }
bool submitBatch(AndroidHost& value, std::uint64_t pointerId, std::uint64_t sequence,
                 std::uint64_t timestampNs, float x, float y, float pressure,
                 int phase, int family, float contentX, float contentY) {
  const auto identity = canvas::ink_playground::androidPointerIdentity(pointerId); if (!identity) return false;
  canvas::input::PlatformPointerBatch batch;
  batch.samples.push_back({1U, *identity, sequence, timestampNs, x, y, pressure, 0.0F, 0.0F, {}, {},
    canvas::input::SampleProvenance::kConfirmedCurrent,
    phase == 1 ? canvas::input::PointerPhase::kDown : phase == 3 ? canvas::input::PointerPhase::kUp : canvas::input::PointerPhase::kMove});
  if (!value.host->acceptPlatformBatch(batch, timestampNs)) return false;
  if (phase == 1) {
    if (!value.host->beginBrushSession(pointerId, static_cast<std::uint32_t>(family))) return false;
    value.family = static_cast<std::uint64_t>(family);
  } else if (!value.host->viewportGestureClaimed() &&
             !value.host->appendBrushSample(pointerId, contentX, contentY, pressure, sequence)) {
    return false;
  }
  if (phase == 3) {
    if (!value.host->finishBrushSession(pointerId)) return false;
    value.digest = value.host->brushDigest(); value.primitiveCount = value.host->brushPrimitiveCount();
  }
  return true;
}
}

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height) {
  if (width == 0U || height == 0U) return nullptr; auto value = std::make_unique<AndroidHost>();
  if (!initPrograms(*value) || !value->host->bindSurface(width, height)) return nullptr;
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value->renderer->resize(width, height).code != canvas::render::BackendSubmissionCode::kAccepted) return nullptr;
#endif
  return value.release();
}
void axiom_ink_android_destroy_host(void* handle) { delete asHost(handle); }
int axiom_ink_android_platform_batch(void* handle, std::uint64_t pointerId, std::uint64_t sequence, std::uint64_t timestampNs,
    float x, float y, float pressure, int phase, int family, float contentX, float contentY) {
  auto* value = asHost(handle); return value != nullptr && submitBatch(*value, pointerId, sequence, timestampNs, x, y, pressure, phase, family, contentX, contentY) ? 1 : 0;
}
int axiom_ink_android_resize(void* handle, std::uint32_t width, std::uint32_t height) {
  auto* value = asHost(handle); if (value == nullptr || !value->host->resizeSurface(width, height)) return 0;
#if defined(CANVAS_RENDER_HAS_SKIA)
  if (value->renderer->resize(width, height).code != canvas::render::BackendSubmissionCode::kAccepted) return 0;
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
  if (value->renderer->width() != width || value->renderer->height() != height) if (value->renderer->resize(width, height).code != canvas::render::BackendSubmissionCode::kAccepted) return 0;
  const auto points = value->host->brushRenderPoints();
  if (value->renderer->submitBrushPoints(points).code != canvas::render::BackendSubmissionCode::kAccepted) return 0; const auto pixels = value->renderer->rgba();
  for (std::uint32_t y = 0; y < height; ++y) std::copy_n(pixels.data() + static_cast<std::size_t>(y) * width * 4U, static_cast<std::size_t>(width) * 4U, rgba + static_cast<std::size_t>(y) * stride); return 1;
#else
  (void)width; (void)height; (void)stride; return 0;
#endif
}
float axiom_ink_android_brush_size(void* handle, std::uint64_t pointer) { (void)handle; (void)pointer; return 1.0F; }
float axiom_ink_android_brush_opacity(void* handle, std::uint64_t pointer) { (void)handle; (void)pointer; return 1.0F; }
int axiom_ink_android_brush_representation(void* handle, std::uint64_t pointer) { (void)handle; (void)pointer; return 1; }
std::uint64_t axiom_ink_android_brush_digest(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->digest; }
std::uint64_t axiom_ink_android_brush_primitive_count(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->primitiveCount; }
std::uint64_t axiom_ink_android_brush_family(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->family; }
int axiom_ink_android_brush_canonical_mutation(void* handle) { (void)handle; return 1; }
}

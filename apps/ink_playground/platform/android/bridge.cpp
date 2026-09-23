#include "ink_playground_host.hpp"
#include "android_pointer_identity.hpp"
#include "canvas/ink/programmable_brush.hpp"
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
  canvas::ink::ResourceCatalog resources;
  canvas::ink::BrushRuntime runtime{resources};
  std::unordered_map<std::uint64_t, std::shared_ptr<const canvas::ink::BrushProgram>> programs;
  std::unordered_map<std::uint64_t, std::uint64_t> sessions;
  std::unordered_map<std::uint64_t, canvas::ink::BrushPrimitive> primitives;
  std::vector<canvas::ink::BrushPrimitive> committed;
  std::uint64_t serial = 0, digest = 0, primitiveCount = 0, family = 1;
  bool canonicalMutation = false;
#if defined(CANVAS_RENDER_HAS_SKIA)
  std::unique_ptr<canvas::render::SkiaInkBackend> renderer = std::make_unique<canvas::render::SkiaInkBackend>();
#endif
};
AndroidHost* asHost(void* value) { return static_cast<AndroidHost*>(value); }
canvas::ink::BrushDefinition brushDefinition(std::uint64_t family) {
  canvas::ink::BrushDefinition value; value.definitionId = family;
  value.family = static_cast<canvas::ink::BrushFamily>(family);
  value.nominalSize = family == 6 ? 18.0F : 7.0F; value.opacity = family == 5 ? 0.35F : 0.8F;
  value.spacing = family >= 2 && family <= 5 ? 0.2F : 0.08F;
  if (family >= 2 && family <= 5) { value.shapeResource = {100U + family}; value.grainResource = {200U + family}; }
  return value;
}
bool initPrograms(AndroidHost& value) {
  for (std::uint64_t family = 2; family <= 5; ++family) {
    value.resources.add({100U + family}, canvas::ink::BrushResourceKind::kShape);
    value.resources.add({200U + family}, canvas::ink::BrushResourceKind::kGrain);
  }
  for (std::uint64_t family = 1; family <= 7; ++family) {
    const auto result = canvas::ink::BrushCompiler{}.compile(brushDefinition(family),
      {.pressure = false, .tilt = false, .shapeResource = true, .grainResource = true, .temporalTransient = true});
    if (!result) return false; value.programs.emplace(family, result.program);
  }
  return true;
}
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
    const auto program = value.programs.find(static_cast<std::uint64_t>(family)); if (program == value.programs.end()) return false;
    const auto session = ++value.serial; if (!value.runtime.begin({session}, *program->second, 0x4500ULL + family + session)) return false;
    value.sessions[pointerId] = session; value.family = static_cast<std::uint64_t>(family);
  }
  const auto session = value.sessions.find(pointerId);
  if (session != value.sessions.end() && !value.host->viewportGestureClaimed()) {
    const canvas::ink::BrushInputSample sample{contentX, contentY, pressure, 0.0F, 0.0F, sequence};
    const auto result = value.runtime.append({session->second}, std::span<const canvas::ink::BrushInputSample>(&sample, 1));
    if (!result) return false; if (!result.preview.primitives.empty()) value.primitives[pointerId] = result.preview.primitives.back();
  }
  if (phase == 3) {
    if (session == value.sessions.end()) return false; const auto result = value.runtime.finish({session->second}); if (!result) return false;
    value.digest = result.commit.digest; value.primitiveCount = result.commit.primitives.size(); value.canonicalMutation = result.commit.canonicalMutation;
    value.committed.insert(value.committed.end(), result.commit.primitives.begin(), result.commit.primitives.end()); value.sessions.erase(session);
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
int axiom_ink_android_cancel_all(void* handle) { auto* value = asHost(handle); if (value == nullptr) return 0; value->host->cancelAllPointers(); for (const auto& [p, s] : value->sessions) { (void)p; (void)value->runtime.cancel({s}); } value->sessions.clear(); value->primitives.clear(); return 1; }
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
  std::vector<canvas::ink::BrushPrimitive> primitives = value->committed; for (const auto& [p, primitive] : value->primitives) { (void)p; primitives.push_back(primitive); }
  if (value->renderer->submitPrimitives(primitives).code != canvas::render::BackendSubmissionCode::kAccepted) return 0; const auto pixels = value->renderer->rgba();
  for (std::uint32_t y = 0; y < height; ++y) std::copy_n(pixels.data() + static_cast<std::size_t>(y) * width * 4U, static_cast<std::size_t>(width) * 4U, rgba + static_cast<std::size_t>(y) * stride); return 1;
#else
  (void)width; (void)height; (void)stride; return 0;
#endif
}
float axiom_ink_android_brush_size(void* handle, std::uint64_t pointer) { auto* value = asHost(handle); if (value == nullptr) return 6.0F; const auto it = value->primitives.find(pointer); return it == value->primitives.end() ? 6.0F : it->second.size; }
float axiom_ink_android_brush_opacity(void* handle, std::uint64_t pointer) { auto* value = asHost(handle); if (value == nullptr) return 1.0F; const auto it = value->primitives.find(pointer); return it == value->primitives.end() ? 1.0F : it->second.opacity; }
int axiom_ink_android_brush_representation(void* handle, std::uint64_t pointer) { auto* value = asHost(handle); if (value == nullptr) return 1; const auto it = value->primitives.find(pointer); return it == value->primitives.end() ? 1 : static_cast<int>(it->second.representation); }
std::uint64_t axiom_ink_android_brush_digest(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->digest; }
std::uint64_t axiom_ink_android_brush_primitive_count(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->primitiveCount; }
std::uint64_t axiom_ink_android_brush_family(void* handle) { auto* value = asHost(handle); return value == nullptr ? 0 : value->family; }
int axiom_ink_android_brush_canonical_mutation(void* handle) { auto* value = asHost(handle); return value != nullptr && value->canonicalMutation; }
}

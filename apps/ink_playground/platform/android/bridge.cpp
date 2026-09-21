#include "ink_playground_host.hpp"
#include "android_pointer_identity.hpp"
#include "arc/arc.hpp"
#include "canvas/input/active_pointer_registry.hpp"
#include "canvas/ink/programmable_brush.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <unordered_map>

namespace {
using canvas::ink_playground::InkPlaygroundHost;

class AndroidSink final : public arc::PointerSampleSink {
 public:
  AndroidSink(InkPlaygroundHost& host,
              const std::unordered_map<std::uint64_t, canvas::input::PointerKey>& activeKeys)
      : host_(host), activeKeys_(activeKeys) {}
  arc::Status Push(const arc_pointer_sample_batch_v0& batch) override {
    canvas::input::PointerSampleBatch samples;
    samples.samples.reserve(batch.sample_count);
    const auto* raw = reinterpret_cast<const std::byte*>(batch.samples);
    for (std::uint32_t i = 0; i < batch.sample_count; ++i) {
      const auto* sample = reinterpret_cast<const arc_pointer_sample_v0*>(raw +
          static_cast<std::size_t>(i) * batch.sample_stride);
      const auto key = activeKeys_.find(sample->pointer_id);
      if (key == activeKeys_.end()) return arc::Status::kInvalidState;
      const auto phase = sample->phase == ARC_POINTER_PHASE_DOWN
          ? canvas::input::PointerPhase::kDown
          : sample->phase == ARC_POINTER_PHASE_UP ? canvas::input::PointerPhase::kUp
          : sample->phase == ARC_POINTER_PHASE_CANCEL ? canvas::input::PointerPhase::kCancel
          : canvas::input::PointerPhase::kMove;
      samples.samples.push_back({sample->sample_sequence, sample->timestamp_us * 1000U,
                                 sample->x, sample->y, sample->pressure,
                                 sample->provenance == ARC_SAMPLE_PLATFORM_PREDICTION_HINT,
                                 key->second, {}, {}, phase});
    }
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return host_.accept(samples, now) ? arc::Status::kOk : arc::Status::kInvalidState;
  }
  void SourceLost(std::uint64_t, arc::Status) override { (void)host_.loseSurface(); }
 private:
  InkPlaygroundHost& host_;
  const std::unordered_map<std::uint64_t, canvas::input::PointerKey>& activeKeys_;
};

struct AndroidHost final {
  std::unique_ptr<InkPlaygroundHost> host = std::make_unique<InkPlaygroundHost>();
  std::unique_ptr<arc::InputSource> input = arc::CreateAndroidInputSource();
  std::unique_ptr<AndroidSink> sink;
  canvas::ink::ResourceCatalog brushResources;
  canvas::ink::BrushRuntime brushRuntime{brushResources};
  std::unordered_map<std::uint64_t, std::shared_ptr<const canvas::ink::BrushProgram>> brushPrograms;
  std::unordered_map<std::uint64_t, std::uint64_t> brushSessions;
  std::uint64_t brushStroke = 0;
  std::uint64_t lastBrushDigest = 0;
  std::uint64_t lastBrushPrimitiveCount = 0;
  std::uint64_t lastBrushFamily = 0;
  bool lastBrushCanonicalMutation = false;
  std::unordered_map<std::uint64_t, canvas::ink::BrushPrimitive> brushPrimitives;
  std::uint64_t stroke = 0;
  canvas::input::ActivePointerRegistry pointerRegistry;
  std::unordered_map<std::uint64_t, canvas::input::PointerKey> activeKeys;
  std::unordered_map<std::uint64_t, std::uint64_t> pointerStrokes;
};
AndroidHost* asHost(void* value) { return static_cast<AndroidHost*>(value); }

canvas::ink::BrushDefinition brushDefinition(std::uint64_t family) {
  canvas::ink::BrushDefinition definition;
  definition.definitionId = family;
  definition.family = static_cast<canvas::ink::BrushFamily>(family);
  definition.nominalSize = family == 6 ? 18.0F : 7.0F;
  definition.opacity = family == 5 ? 0.35F : 0.8F;
  definition.spacing = (family >= 2 && family <= 5) ? 0.2F : 0.08F;
  if (family >= 2 && family <= 5) {
    definition.shapeResource = {100U + family};
    definition.grainResource = {200U + family};
  }
  return definition;
}

bool initializeBrushPrograms(AndroidHost& value) {
  for (std::uint64_t family = 1; family <= 7; ++family) {
    const auto definition = brushDefinition(family);
    const auto result = canvas::ink::BrushCompiler{}.compile(
        definition, canvas::ink::BrushCapabilityProfile{
            .pressure = false, .tilt = false, .shapeResource = true,
            .grainResource = true, .temporalTransient = true});
    if (!result) return false;
    value.brushPrograms.emplace(family, result.program);
  }
  return true;
}

bool submitSample(AndroidHost& value, std::uint64_t pointerId, std::uint64_t sequence,
                  std::uint64_t timestampNs, float x, float y, float pressure,
                  std::uint32_t phase, int tool) {
  arc_pointer_sample_v0 sample{};
  sample.pointer_id = pointerId; sample.sample_sequence = sequence;
  sample.timestamp_us = timestampNs / 1000U; sample.x = x; sample.y = y;
  sample.pressure = pressure; sample.phase = phase; sample.provenance = ARC_SAMPLE_CONFIRMED_CURRENT;
  arc_pointer_sample_batch_v0 batch{}; batch.struct_size = sizeof(batch);
  batch.abi_version = ARC_ABI_VERSION; batch.schema_version = ARC_PROTOCOL_SCHEMA_VERSION;
  batch.coordinate_space = ARC_COORDINATE_SPACE_VIEW_LOGICAL; batch.view_id = 1;
  batch.viewport_revision = 1; batch.device_id = pointerId;
  batch.input_capabilities = ARC_INPUT_CAPABILITY_HISTORY;
  batch.tool = (tool == 2 || tool == 4) ? ARC_INPUT_TOOL_PEN : ARC_INPUT_TOOL_TOUCH;
  if (batch.tool == ARC_INPUT_TOOL_PEN)
    batch.input_capabilities |= ARC_INPUT_CAPABILITY_PRESSURE;
  batch.samples = &sample; batch.sample_count = 1;
  batch.sample_stride = sizeof(sample);
  return value.input->SubmitBatch(batch) == arc::Status::kOk;
}

bool releasePointer(AndroidHost& value, std::uint64_t pointerId) noexcept {
  const auto key = value.activeKeys.find(pointerId);
  if (key == value.activeKeys.end()) return false;
  const bool ended = value.pointerRegistry.end(key->second);
  value.activeKeys.erase(key);
  value.pointerStrokes.erase(pointerId);
  return ended;
}
}  // namespace

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height) {
  if (width == 0U || height == 0U) return nullptr;
  auto value = std::make_unique<AndroidHost>();
  for (std::uint64_t family = 2; family <= 5; ++family) {
    value->brushResources.add({100U + family}, canvas::ink::BrushResourceKind::kShape);
    value->brushResources.add({200U + family}, canvas::ink::BrushResourceKind::kGrain);
  }
  if (!initializeBrushPrograms(*value)) return nullptr;
  if (value->input == nullptr || !value->host->bindSurface(width, height)) return nullptr;
  value->sink = std::make_unique<AndroidSink>(*value->host, value->activeKeys);
  if (value->input->Start(*value->sink) != arc::Status::kOk) return nullptr;
  return value.release();
}
void axiom_ink_android_destroy_host(void* handle) {
  auto* value = asHost(handle);
  if (value != nullptr && value->input != nullptr) value->input->Stop();
  delete value;
}
int axiom_ink_android_motion(void* handle, std::uint64_t pointerId,
                             std::uint64_t sequence, std::uint64_t timestampNs,
                             float x, float y, float pressure, int down, int up) {
  auto* value = asHost(handle);
  if (value == nullptr || value->input == nullptr) return 0;
  if (down != 0) {
    const auto key = value->pointerRegistry.begin(1, pointerId);
    if (!key.valid()) return 0;
    value->activeKeys[pointerId] = key;
    ++value->stroke;
    value->pointerStrokes[pointerId] = value->stroke;
    if (!value->host->beginStroke(key, value->stroke)) return 0;
  }
  const auto phase = down != 0 ? ARC_POINTER_PHASE_DOWN : (up != 0 ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE);
  if (!submitSample(*value, pointerId, sequence, timestampNs, x, y, pressure,
                    phase, ARC_INPUT_TOOL_PEN)) return 0;
  if (up != 0) {
    const auto key = value->activeKeys.find(pointerId);
    const auto stroke = value->pointerStrokes.find(pointerId);
    if (key == value->activeKeys.end() || stroke == value->pointerStrokes.end() ||
        !value->host->commitStroke(key->second, stroke->second, stroke->second)) return 0;
    (void)value->pointerRegistry.end(key->second);
    value->activeKeys.erase(key);
    value->pointerStrokes.erase(stroke);
  }
  return 1;
}
int axiom_ink_android_begin(void* handle, std::uint64_t pointerId, std::uint64_t strokeId) {
  auto* value = asHost(handle);
  if (value == nullptr || strokeId == 0U) return 0;
  const auto identity = canvas::ink_playground::androidPointerIdentity(pointerId);
  if (!identity.has_value()) return 0;
  const auto key = value->pointerRegistry.begin(1, *identity);
  if (!key.valid()) return 0;
  value->activeKeys[*identity] = key;
  value->pointerStrokes[*identity] = strokeId;
  value->stroke = strokeId;
  return value->host->beginStroke(key, strokeId);
}
int axiom_ink_android_sample(void* handle, std::uint64_t pointerId,
                            std::uint64_t sequence, std::uint64_t timestampNs,
                            float x, float y, float pressure, int phase, int tool) {
  auto* value = asHost(handle);
  if (value == nullptr || value->input == nullptr || sequence == 0U) return 0;
  const auto identity = canvas::ink_playground::androidPointerIdentity(pointerId);
  if (!identity.has_value()) return 0;
  const std::uint32_t arcPhase = phase == 1 ? ARC_POINTER_PHASE_DOWN
      : phase == 3 ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE;
  return submitSample(*value, *identity, sequence, timestampNs, x, y, pressure,
                      arcPhase, tool);
}
int axiom_ink_android_commit(void* handle, std::uint64_t pointerId, std::uint64_t strokeId) {
  auto* value = asHost(handle);
  if (value == nullptr) return 0;
  const auto identity = canvas::ink_playground::androidPointerIdentity(pointerId);
  if (!identity.has_value()) return 0;
  const auto key = value->activeKeys.find(*identity);
  if (key == value->activeKeys.end()) return 0;
  const bool committed = value->host->commitStroke(key->second, strokeId, strokeId);
  const bool released = releasePointer(*value, *identity);
  return committed && released;
}
int axiom_ink_android_resize(void* handle, std::uint32_t width, std::uint32_t height) {
  auto* value = asHost(handle); return value != nullptr && value->host->resizeSurface(width, height);
}
int axiom_ink_android_surface_lost(void* handle) {
  auto* value = asHost(handle);
  if (value == nullptr) return 0;
  for (const auto& [pointerId, session] : value->brushSessions) {
    (void)pointerId;
    (void)value->brushRuntime.cancel({session});
  }
  value->brushSessions.clear();
  value->brushPrimitives.clear();
  return value->host->loseSurface();
}
int axiom_ink_android_cancel_all(void* handle) {
  auto* value = asHost(handle);
  if (value == nullptr) return 0;
  value->host->cancelAllPointers();
  for (const auto& [pointerId, session] : value->brushSessions) {
    (void)pointerId;
    (void)value->brushRuntime.cancel({session});
  }
  value->brushSessions.clear();
  value->brushPrimitives.clear();
  value->activeKeys.clear();
  value->pointerStrokes.clear();
  return 1;
}
int axiom_ink_android_viewport_claimed(void* handle) {
  auto* value = asHost(handle);
  return value != nullptr && value->host->viewportGestureClaimed();
}
int axiom_ink_android_set_multi_contact_policy(void* handle, int policy) {
  auto* value = asHost(handle);
  if (value == nullptr || policy < 0 || policy > 2) return 0;
  const auto selected = static_cast<canvas::interaction::MultiContactPolicy>(policy);
  return value->host->setMultiContactPolicy(selected);
}
int axiom_ink_android_multi_contact_policy(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? -1 : static_cast<int>(value->host->multiContactPolicy());
}
float axiom_ink_android_viewport_scale(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 1.0F : value->host->viewportGesture().scale;
}
float axiom_ink_android_viewport_center_x(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0.0F : value->host->viewportGesture().centerX;
}
float axiom_ink_android_viewport_center_y(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0.0F : value->host->viewportGesture().centerY;
}
float axiom_ink_android_viewport_translation_x(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0.0F : value->host->viewportGesture().translationX;
}
float axiom_ink_android_viewport_translation_y(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0.0F : value->host->viewportGesture().translationY;
}
int axiom_ink_android_brush_begin(void* handle, std::uint64_t pointerId,
                                  std::uint64_t family) {
  auto* value = asHost(handle);
  if (value == nullptr || family < 1U || family > 7U ||
      value->brushPrograms.find(family) == value->brushPrograms.end()) return 0;
  const auto session = ++value->brushStroke;
  if (!value->brushRuntime.begin({session}, *value->brushPrograms.at(family),
                                 0x4500ULL + family + session)) return 0;
  value->brushSessions[pointerId] = session;
  value->lastBrushFamily = family;
  return 1;
}
int axiom_ink_android_brush_sample(void* handle, std::uint64_t pointerId,
                                   std::uint64_t sequence, float x, float y,
                                   float pressure) {
  auto* value = asHost(handle);
  if (value == nullptr) return 0;
  const auto found = value->brushSessions.find(pointerId);
  if (found == value->brushSessions.end()) return 0;
  const canvas::ink::BrushInputSample sample{x, y, pressure, 0.0F, 0.0F, sequence};
  const auto result = value->brushRuntime.append(
      {found->second}, std::span<const canvas::ink::BrushInputSample>(&sample, 1));
  if (result && !result.preview.primitives.empty()) {
    value->brushPrimitives[pointerId] = result.preview.primitives.back();
  }
  return result ? 1 : 0;
}
int axiom_ink_android_brush_finish(void* handle, std::uint64_t pointerId) {
  auto* value = asHost(handle);
  if (value == nullptr) return 0;
  const auto found = value->brushSessions.find(pointerId);
  if (found == value->brushSessions.end()) return 0;
  const auto result = value->brushRuntime.finish({found->second});
  if (!result) return 0;
  value->lastBrushDigest = result.commit.digest;
  value->lastBrushPrimitiveCount = result.preview.primitives.size();
  value->lastBrushCanonicalMutation = result.commit.canonicalMutation;
  value->brushSessions.erase(found);
  return 1;
}
float axiom_ink_android_brush_size(void* handle, std::uint64_t pointerId) {
  auto* value = asHost(handle);
  if (value == nullptr) return 6.0F;
  const auto found = value->brushPrimitives.find(pointerId);
  return found == value->brushPrimitives.end() ? 6.0F : found->second.size;
}
float axiom_ink_android_brush_opacity(void* handle, std::uint64_t pointerId) {
  auto* value = asHost(handle);
  if (value == nullptr) return 1.0F;
  const auto found = value->brushPrimitives.find(pointerId);
  return found == value->brushPrimitives.end() ? 1.0F : found->second.opacity;
}
int axiom_ink_android_brush_representation(void* handle, std::uint64_t pointerId) {
  auto* value = asHost(handle);
  if (value == nullptr) return 1;
  const auto found = value->brushPrimitives.find(pointerId);
  return found == value->brushPrimitives.end() ? 1 : static_cast<int>(found->second.representation);
}
std::uint64_t axiom_ink_android_brush_digest(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0 : value->lastBrushDigest;
}
std::uint64_t axiom_ink_android_brush_primitive_count(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0 : value->lastBrushPrimitiveCount;
}
std::uint64_t axiom_ink_android_brush_family(void* handle) {
  auto* value = asHost(handle);
  return value == nullptr ? 0 : value->lastBrushFamily;
}
int axiom_ink_android_brush_canonical_mutation(void* handle) {
  auto* value = asHost(handle);
  return value != nullptr && value->lastBrushCanonicalMutation ? 1 : 0;
}
void* axiom_ink_android_create_input_source() {
  return arc::CreateAndroidInputSource().release();
}
void axiom_ink_android_destroy_input_source(void* source) {
  delete static_cast<arc::InputSource*>(source);
}
}

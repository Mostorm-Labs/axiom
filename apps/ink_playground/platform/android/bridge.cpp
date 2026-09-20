#include "ink_playground_host.hpp"
#include "arc/arc.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace {
using canvas::ink_playground::InkPlaygroundHost;

class AndroidSink final : public arc::PointerSampleSink {
 public:
  explicit AndroidSink(InkPlaygroundHost& host) : host_(host) {}
  arc::Status Push(const arc_pointer_sample_batch_v0& batch) override {
    canvas::input::PointerSampleBatch samples;
    samples.samples.reserve(batch.sample_count);
    const auto* raw = reinterpret_cast<const std::byte*>(batch.samples);
    for (std::uint32_t i = 0; i < batch.sample_count; ++i) {
      const auto* sample = reinterpret_cast<const arc_pointer_sample_v0*>(raw +
          static_cast<std::size_t>(i) * batch.sample_stride);
      samples.samples.push_back({sample->sample_sequence, sample->timestamp_us * 1000U,
                                 sample->x, sample->y, sample->pressure,
                                 sample->provenance == ARC_SAMPLE_PLATFORM_PREDICTION_HINT});
    }
    const auto now = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    return host_.accept(samples, now) ? arc::Status::kOk : arc::Status::kInvalidState;
  }
  void SourceLost(std::uint64_t, arc::Status) override { (void)host_.loseSurface(); }
 private:
  InkPlaygroundHost& host_;
};

struct AndroidHost final {
  std::unique_ptr<InkPlaygroundHost> host = std::make_unique<InkPlaygroundHost>();
  std::unique_ptr<arc::InputSource> input = arc::CreateAndroidInputSource();
  std::unique_ptr<AndroidSink> sink;
  std::uint64_t stroke = 0;
};
AndroidHost* asHost(void* value) { return static_cast<AndroidHost*>(value); }

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
}  // namespace

extern "C" {
void* axiom_ink_android_create_host(std::uint32_t width, std::uint32_t height) {
  if (width == 0U || height == 0U) return nullptr;
  auto value = std::make_unique<AndroidHost>();
  if (value->input == nullptr || !value->host->bindSurface(width, height)) return nullptr;
  value->sink = std::make_unique<AndroidSink>(*value->host);
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
  if (down != 0) { ++value->stroke; if (!value->host->beginStroke(value->stroke)) return 0; }
  const auto phase = down != 0 ? ARC_POINTER_PHASE_DOWN : (up != 0 ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE);
  if (!submitSample(*value, pointerId, sequence, timestampNs, x, y, pressure,
                    phase, ARC_INPUT_TOOL_PEN)) return 0;
  if (up != 0 && !value->host->commitStroke(value->stroke, value->stroke)) return 0;
  return 1;
}
int axiom_ink_android_begin(void* handle, std::uint64_t strokeId) {
  auto* value = asHost(handle);
  if (value == nullptr || strokeId == 0U) return 0;
  value->stroke = strokeId;
  return value->host->beginStroke(strokeId);
}
int axiom_ink_android_sample(void* handle, std::uint64_t pointerId,
                            std::uint64_t sequence, std::uint64_t timestampNs,
                            float x, float y, float pressure, int phase, int tool) {
  auto* value = asHost(handle);
  if (value == nullptr || value->input == nullptr || sequence == 0U) return 0;
  const std::uint32_t arcPhase = phase == 1 ? ARC_POINTER_PHASE_DOWN
      : phase == 3 ? ARC_POINTER_PHASE_UP : ARC_POINTER_PHASE_MOVE;
  return submitSample(*value, pointerId, sequence, timestampNs, x, y, pressure,
                      arcPhase, tool);
}
int axiom_ink_android_commit(void* handle, std::uint64_t strokeId) {
  auto* value = asHost(handle);
  return value != nullptr && value->host->commitStroke(strokeId, strokeId);
}
int axiom_ink_android_resize(void* handle, std::uint32_t width, std::uint32_t height) {
  auto* value = asHost(handle); return value != nullptr && value->host->resizeSurface(width, height);
}
int axiom_ink_android_surface_lost(void* handle) {
  auto* value = asHost(handle); return value != nullptr && value->host->loseSurface();
}
void* axiom_ink_android_create_input_source() {
  return arc::CreateAndroidInputSource().release();
}
void axiom_ink_android_destroy_input_source(void* source) {
  delete static_cast<arc::InputSource*>(source);
}
}

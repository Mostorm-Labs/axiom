#include "minimal_runtime_host.hpp"

namespace canvas::minimal_runtime_host {

bool MinimalRuntimeHost::registerSource(input::InputSourceId source) noexcept {
  return source != 0U && sources_.insert(source).second;
}

bool MinimalRuntimeHost::submit(const input::PlatformPointerBatch& batch) noexcept {
  if (batch.samples.empty() || !sources_.contains(batch.samples.front().source)) return false;
  for (const auto& sample : batch.samples) {
    if (sample.source != batch.samples.front().source) return false;
  }
  const auto routed = ingress_.submit(batch);
  if (!routed.accepted && !routed.terminalCancel) return false;
  for (const auto& sample : routed.normalized.samples) {
    if (!dispatch_.submit(sample)) return false;
  }
  return true;
}

std::size_t MinimalRuntimeHost::eventCount(interaction::DispatchEvent event) const noexcept {
  const auto index = static_cast<std::size_t>(event);
  return index < counts_.size() ? counts_[index] : 0U;
}

bool MinimalRuntimeHost::record(interaction::DispatchEvent event) noexcept {
  ++counts_[static_cast<std::size_t>(event)];
  return true;
}

bool MinimalRuntimeHost::selection(const input::PointerSample&) noexcept {
  return record(interaction::DispatchEvent::kSelection);
}
bool MinimalRuntimeHost::creation(const input::PointerSample&) noexcept {
  return record(interaction::DispatchEvent::kCreation);
}
bool MinimalRuntimeHost::pan(const input::PointerSample&) noexcept {
  return record(interaction::DispatchEvent::kPan);
}
bool MinimalRuntimeHost::brush(const input::PointerSample&) noexcept {
  return record(interaction::DispatchEvent::kBrush);
}
bool MinimalRuntimeHost::eraser(const input::PointerSample&) noexcept {
  return record(interaction::DispatchEvent::kEraser);
}

}  // namespace canvas::minimal_runtime_host

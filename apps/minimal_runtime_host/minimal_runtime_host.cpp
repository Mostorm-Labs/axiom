#include "minimal_runtime_host.hpp"

namespace canvas::minimal_runtime_host {
MinimalRuntimeHost::MinimalRuntimeHost() noexcept { (void)composition_.open(); }
bool MinimalRuntimeHost::registerSource(input::InputSourceId source) noexcept {
  return composition_.registerSource(source);
}
bool MinimalRuntimeHost::unregisterSource(input::InputSourceId source,
                                          std::uint64_t sequence,
                                          std::uint64_t timestampNs) noexcept {
  return composition_.unregisterSource(source, sequence, timestampNs);
}
bool MinimalRuntimeHost::submit(const input::PlatformPointerBatch& batch) noexcept {
  if (batch.samples.empty()) return false;
  return composition().submit(batch);
}
}  // namespace canvas::minimal_runtime_host

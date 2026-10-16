#include "minimal_runtime_host.hpp"

namespace canvas::minimal_runtime_host {
bool MinimalRuntimeHost::registerSource(input::InputSourceId source) noexcept {
  return runtime_.registerInputSource(source, "minimal");
}
bool MinimalRuntimeHost::unregisterSource(input::InputSourceId source,
                                          std::uint64_t sequence,
                                          std::uint64_t timestampNs) noexcept {
  return runtime_.unregisterInputSource(source, sequence, timestampNs);
}
bool MinimalRuntimeHost::submit(const input::PlatformPointerBatch& batch) noexcept {
  if (batch.samples.empty()) return false;
  return runtime_.acceptPlatformBatch(batch, batch.samples.back().timestampNs);
}
}  // namespace canvas::minimal_runtime_host

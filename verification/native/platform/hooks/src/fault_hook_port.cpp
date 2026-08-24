#include <axiom/verification/platform_hooks.h>

namespace axiom::verification::platform {

PlatformHooks::PlatformHooks(NormalizedPlatformPort& port, SourceLeaseProbe& leases,
                             SourceAttemptProbe& attempts, EventTap& events)
    : port_(port), leases_(leases), attempts_(attempts), events_(events) {}

FaultState PlatformHooks::apply_fault(const FaultRequest& request) {
  if (request.fault_id.empty()) return FaultState::Failed;
  if (request.type == FaultType::PresentCompletionHeld) {
    if (request.mode == FaultMode::Activate) {
      hold_present_completions_ = true;
      return FaultState::Active;
    }
    if (request.mode != FaultMode::Clear) return FaultState::Failed;
    hold_present_completions_ = false;
    release_held_completions();
    return FaultState::Cleared;
  }
  if (request.mode != FaultMode::Pulse) return FaultState::Failed;
  if (request.type == FaultType::SurfaceLost) port_.surface_lost(request.generation);
  if (request.type == FaultType::DeviceLost) port_.device_lost(request.generation);
  return FaultState::Cleared;
}

bool PlatformHooks::capture_present_completion(
    std::string attempt_id, std::string source_lease_id,
    GenerationKind generation_kind, std::uint64_t generation,
    std::function<void()> completion) {
  if (!leases_.is_open(source_lease_id) || !completion) return false;
  if (hold_present_completions_) {
    held_.push_back({std::move(attempt_id), std::move(source_lease_id),
                     generation_kind, generation, std::move(completion)});
    return true;
  }
  completion();
  probe_generation(std::move(attempt_id), std::move(source_lease_id),
                   generation_kind, generation);
  return true;
}

PublishDisposition PlatformHooks::probe_generation(
    std::string attempt_id, std::string source_lease_id, GenerationKind kind,
    std::uint64_t generation) {
  if (!leases_.is_open(source_lease_id)) {
    const auto disposition = PublishDisposition::DroppedStaleScope;
    attempts_.record({std::move(attempt_id), std::move(source_lease_id), kind,
                      generation, disposition});
    return disposition;
  }
  const auto disposition = port_.probe_publish(kind, generation);
  attempts_.record({std::move(attempt_id), source_lease_id, kind, generation,
                    disposition});
  if (disposition == PublishDisposition::Forwarded) {
    events_.record({std::move(source_lease_id), kind, generation});
  }
  return disposition;
}

void PlatformHooks::release_held_completions() {
  auto pending = std::move(held_);
  held_.clear();
  for (auto& held : pending) {
    held.completion();
    probe_generation(std::move(held.attempt_id), std::move(held.source_lease_id),
                     held.generation_kind, held.generation);
  }
}

}  // namespace axiom::verification::platform

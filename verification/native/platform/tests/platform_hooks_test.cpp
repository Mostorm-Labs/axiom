#include <axiom/verification/platform_hooks.h>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace avp = axiom::verification::platform;

namespace {

void require(bool condition) {
  if (!condition) {
    std::abort();
  }
}

std::string_view disposition_name(avp::PublishDisposition disposition) {
  switch (disposition) {
    case avp::PublishDisposition::Forwarded:
      return "FORWARDED";
    case avp::PublishDisposition::DroppedStaleScope:
      return "DROPPED_STALE_SCOPE";
    case avp::PublishDisposition::DroppedStaleGeneration:
      return "DROPPED_STALE_GENERATION";
    case avp::PublishDisposition::Cancelled:
      return "CANCELLED";
  }
  std::abort();
}

void write_trace(const std::string& path, const std::vector<int>& release_order,
                 const avp::SourceAttemptProbe& attempts,
                 const avp::EventTap& events, bool late_lease_rejected,
                 bool all_leases_closed) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  require(output.good());
  output << "{\n"
         << "  \"schema_version\": 1,\n"
         << "  \"trace_kind\": \"verification-native-hooks\",\n"
         << "  \"normalized_loss\": [\n"
         << "    {\"fault_id\":\"fault:surface\",\"generation\":7,\"state\":\"CLEARED\",\"type\":\"SURFACE_LOST\"},\n"
         << "    {\"fault_id\":\"fault:device\",\"generation\":8,\"state\":\"CLEARED\",\"type\":\"DEVICE_LOST\"}\n"
         << "  ],\n"
         << "  \"present_hold\": {\"captured\":[\"attempt:held:1\",\"attempt:held:2\"],\"release_order\":[";
  for (std::size_t index = 0; index < release_order.size(); ++index) {
    if (index != 0) output << ',';
    output << release_order[index];
  }
  output << "],\"release_dispositions\":[\""
         << disposition_name(attempts.records()[0].disposition) << "\",\""
         << disposition_name(attempts.records()[1].disposition) << "\"]},\n"
         << "  \"source_attempts\": [\n";
  for (std::size_t index = 0; index < attempts.records().size(); ++index) {
    const auto& attempt = attempts.records()[index];
    output << "    {\"attempt_id\":\"" << attempt.attempt_id
           << "\",\"disposition\":\"" << disposition_name(attempt.disposition)
           << "\",\"generation\":" << attempt.generation << '}';
    output << (index + 1 == attempts.records().size() ? "\n" : ",\n");
  }
  output << "  ],\n  \"event_tap\": [";
  for (std::size_t index = 0; index < events.records().size(); ++index) {
    const auto& event = events.records()[index];
    if (index != 0) output << ',';
    output << "{\"generation\":" << event.generation
           << ",\"source_lease_id\":\"" << event.source_lease_id << "\"}";
  }
  output << "],\n  \"closed_scope\": {\"all_leases_closed\":"
         << (all_leases_closed ? "true" : "false")
         << ",\"late_lease_rejected\":"
         << (late_lease_rejected ? "true" : "false") << "}\n}\n";
  require(output.good());
}

class RecordingPort final : public avp::NormalizedPlatformPort {
 public:
  void surface_lost(std::uint64_t generation) override {
    calls.push_back("surface:" + std::to_string(generation));
  }

  void device_lost(std::uint64_t generation) override {
    calls.push_back("device:" + std::to_string(generation));
  }

  avp::PublishDisposition probe_publish(avp::GenerationKind kind,
                                        std::uint64_t generation) override {
    probes.push_back({kind, generation});
    return generation < current_generation
               ? avp::PublishDisposition::DroppedStaleGeneration
               : avp::PublishDisposition::Forwarded;
  }

  std::uint64_t current_generation{4};
  std::vector<std::string> calls;
  std::vector<std::pair<avp::GenerationKind, std::uint64_t>> probes;
};

}  // namespace

int main(int argc, char** argv) {
  RecordingPort port;
  avp::SourceLeaseProbe leases;
  avp::SourceAttemptProbe attempts;
  avp::EventTap events;
  avp::PlatformHooks hooks(port, leases, attempts, events);

  require(hooks.apply_fault({"fault:surface", avp::FaultMode::Pulse,
                             avp::FaultType::SurfaceLost, 7}) ==
          avp::FaultState::Cleared);
  require(hooks.apply_fault({"fault:device", avp::FaultMode::Pulse,
                             avp::FaultType::DeviceLost, 8}) ==
          avp::FaultState::Cleared);
  require((port.calls == std::vector<std::string>{"surface:7", "device:8"}));

  require(leases.open({"source:present:1", avp::ScopeKind::Canvas,
                       "canvas:1", avp::SourceKind::SurfacePresentation}));
  require(hooks.apply_fault({"fault:hold", avp::FaultMode::Activate,
                             avp::FaultType::PresentCompletionHeld, 0}) ==
          avp::FaultState::Active);
  require(hooks.apply_fault({"fault:hold-invalid", avp::FaultMode::Pulse,
                             avp::FaultType::PresentCompletionHeld, 0}) ==
          avp::FaultState::Failed);

  std::vector<int> release_order;
  require(hooks.capture_present_completion(
      "attempt:held:1", "source:present:1", avp::GenerationKind::Surface, 3,
      [&] { release_order.push_back(1); }));
  require(hooks.capture_present_completion(
      "attempt:held:2", "source:present:1", avp::GenerationKind::Surface, 3,
      [&] { release_order.push_back(2); }));
  require(release_order.empty());
  require(hooks.apply_fault({"fault:hold", avp::FaultMode::Clear,
                             avp::FaultType::PresentCompletionHeld, 0}) ==
          avp::FaultState::Cleared);
  require((release_order == std::vector<int>{1, 2}));
  require(attempts.records().size() == 2);
  require(attempts.records()[0].attempt_id == "attempt:held:1");
  require(attempts.records()[1].attempt_id == "attempt:held:2");
  require(events.records().empty());

  const auto stale = hooks.probe_generation(
      "attempt:stale", "source:present:1", avp::GenerationKind::Surface, 3);
  require(stale == avp::PublishDisposition::DroppedStaleGeneration);
  require(attempts.records().size() == 3);
  require(attempts.records().back().disposition ==
          avp::PublishDisposition::DroppedStaleGeneration);
  require(events.records().empty());

  const auto current = hooks.probe_generation(
      "attempt:current", "source:present:1", avp::GenerationKind::Surface, 4);
  require(current == avp::PublishDisposition::Forwarded);
  require(events.records().size() == 1);

  bool immediate_completion = false;
  require(hooks.capture_present_completion(
      "attempt:immediate", "source:present:1",
      avp::GenerationKind::Surface, 4,
      [&] { immediate_completion = true; }));
  require(immediate_completion);
  require(attempts.records().size() == 5);
  require(attempts.records().back().attempt_id == "attempt:immediate");
  require(attempts.records().back().disposition ==
          avp::PublishDisposition::Forwarded);
  require(events.records().size() == 2);

  leases.close_scope(avp::ScopeKind::Canvas, "canvas:1");
  const bool late_lease_rejected =
      !leases.open({"source:late", avp::ScopeKind::Canvas, "canvas:1",
                    avp::SourceKind::SurfacePresentation});
  require(late_lease_rejected);
  require(leases.close("source:present:1"));
  const bool all_leases_closed =
      leases.all_closed(avp::ScopeKind::Canvas, "canvas:1");
  require(all_leases_closed);
  const auto stale_scope = hooks.probe_generation(
      "attempt:stale-scope", "source:present:1",
      avp::GenerationKind::Surface, 4);
  require(stale_scope == avp::PublishDisposition::DroppedStaleScope);
  require(attempts.records().size() == 6);
  require(attempts.records().back().attempt_id == "attempt:stale-scope");
  require(events.records().size() == 2);
  if (argc == 3 && std::string_view(argv[1]) == "--trace") {
    write_trace(argv[2], release_order, attempts, events, late_lease_rejected,
                all_leases_closed);
  } else {
    require(argc == 1);
  }
  return 0;
}

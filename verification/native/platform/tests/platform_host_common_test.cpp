#include <axiom/verification/platform_host_common.hpp>

#include <cstdlib>
#include <fstream>
#include <stdexcept>
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

template <typename Function>
bool rejected(Function&& function) {
  try {
    function();
  } catch (const std::logic_error&) {
    return true;
  }
  return false;
}

std::string_view fact_kind_name(avp::HostFactKind kind) {
  switch (kind) {
    case avp::HostFactKind::SessionOpened:
      return "SESSION_OPENED";
    case avp::HostFactKind::SessionClosed:
      return "SESSION_CLOSED";
    case avp::HostFactKind::ActionRegistered:
      return "ACTION_REGISTERED";
    case avp::HostFactKind::CompletionBound:
      return "COMPLETION_BOUND";
    case avp::HostFactKind::CompletionObserved:
      return "COMPLETION_OBSERVED";
    case avp::HostFactKind::ProfileReported:
      return "PROFILE_REPORTED";
  }
  std::abort();
}

void write_trace(const std::string& path,
                 const avp::VerificationBuildManifest& manifest,
                 const avp::ProfileCapabilities& profile,
                 const std::vector<avp::HostFact>& facts,
                 const std::vector<std::string>& rejections) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  require(output.good());
  output << "{\n  \"schema_version\":1,\n"
         << "  \"trace_kind\":\"verification-platform-host-common\",\n"
         << "  \"build_manifest\":{\"product_public_abi\":"
         << (manifest.product_public_abi ? "true" : "false")
         << ",\"protocol_version\":" << manifest.protocol_version
         << ",\"verification_only\":"
         << (manifest.verification_only ? "true" : "false") << "},\n"
         << "  \"profile\":{\"capabilities\":[";
  for (std::size_t index = 0; index < profile.capabilities.size(); ++index) {
    if (index != 0) output << ',';
    output << '\"' << profile.capabilities[index] << '\"';
  }
  output << "],\"platform_family\":\"" << profile.platform_family
         << "\",\"profile_id\":\"" << profile.profile_id << "\"},\n"
         << "  \"facts\":[";
  for (std::size_t index = 0; index < facts.size(); ++index) {
    const auto& fact = facts[index];
    if (index != 0) output << ',';
    output << "{\"kind\":\"" << fact_kind_name(fact.kind)
           << "\",\"primary_id\":\"" << fact.primary_id
           << "\",\"related_id\":\"" << fact.related_id << "\"}";
  }
  output << "],\n  \"rejections\":[";
  for (std::size_t index = 0; index < rejections.size(); ++index) {
    if (index != 0) output << ',';
    output << '\"' << rejections[index] << '\"';
  }
  output << "]\n}\n";
  require(output.good());
}

}  // namespace

int main(int argc, char** argv) {
  const auto manifest = avp::verification_build_manifest();
  require(manifest.verification_only);
  require(manifest.protocol_version == 1);
  require(manifest.product_public_abi == false);

  avp::ProtocolBridge bridge;
  bridge.open_session("session:1", 1, "adapter:test:1");
  bridge.register_action("action:1");
  bridge.bind_completion("action:1", "completion:1");
  bridge.complete("completion:1");
  std::vector<std::string> rejections;
  require(rejected([&] { bridge.complete("completion:1"); }));
  rejections.push_back("DUPLICATE_COMPLETION");
  require(rejected([&] { bridge.bind_completion("missing", "completion:2"); }));
  rejections.push_back("UNKNOWN_ACTION_BINDING");
  bridge.close_session();
  require(rejected([&] { bridge.register_action("action:late"); }));
  rejections.push_back("ACTION_AFTER_CLOSE");
  require(rejected([&] { bridge.open_session("session:2", 1, "adapter:test:1"); }));
  rejections.push_back("REUSED_SESSION_EPOCH");
  bridge.open_session("session:2", 2, "adapter:test:1");

  const avp::ProfileCapabilities profile{
      "headless-reference-v0.1", "HEADLESS",
      {"fault.surface.loss", "fault.device.loss", "harness.source_attempt"}};
  bridge.report_profile(profile);
  require(rejected([&] { bridge.report_profile(profile); }));
  require(profile.capabilities.size() == 3);

  const auto* reported_profile = bridge.reported_profile();
  require(reported_profile != nullptr);
  require(reported_profile->profile_id == profile.profile_id);
  require(reported_profile->platform_family == profile.platform_family);
  require(reported_profile->capabilities == profile.capabilities);

  const auto facts = bridge.facts();
  require(!facts.empty());
  require(facts.front().kind == avp::HostFactKind::SessionOpened);
  if (argc == 3 && std::string_view(argv[1]) == "--trace") {
    write_trace(argv[2], manifest, profile, facts, rejections);
  } else {
    require(argc == 1);
  }
  return 0;
}

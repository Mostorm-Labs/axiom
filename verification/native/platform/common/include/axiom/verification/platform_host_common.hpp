#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace axiom::verification::platform {

struct VerificationBuildManifest {
  bool verification_only;
  std::uint32_t protocol_version;
  bool product_public_abi;
};

VerificationBuildManifest verification_build_manifest() noexcept;

enum class HostFactKind {
  SessionOpened,
  SessionClosed,
  ActionRegistered,
  CompletionBound,
  CompletionObserved,
  ProfileReported,
};

struct HostFact {
  HostFactKind kind;
  std::string primary_id;
  std::string related_id;
};

struct ProfileCapabilities {
  std::string profile_id;
  std::string platform_family;
  std::vector<std::string> capabilities;
};

class ProtocolBridge {
 public:
  void open_session(std::string session_id, std::uint64_t epoch,
                    std::string adapter_instance_id);
  void close_session();
  void register_action(std::string action_id);
  void bind_completion(std::string_view action_id, std::string token_id);
  void complete(std::string_view token_id);
  void report_profile(ProfileCapabilities profile);
  const ProfileCapabilities* reported_profile() const noexcept;
  const std::vector<HostFact>& facts() const noexcept;

 private:
  struct Action {
    std::string action_id;
    std::string token_id;
    bool complete{false};
  };

  bool open_{false};
  std::uint64_t last_epoch_{0};
  std::string session_id_;
  std::string adapter_instance_id_;
  std::optional<ProfileCapabilities> profile_;
  std::vector<Action> actions_;
  std::vector<HostFact> facts_;
};

}  // namespace axiom::verification::platform

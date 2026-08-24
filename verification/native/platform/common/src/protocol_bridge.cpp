#include <axiom/verification/platform_host_common.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace axiom::verification::platform {

void ProtocolBridge::open_session(std::string session_id, std::uint64_t epoch,
                                  std::string adapter_instance_id) {
  if (open_ || epoch == 0 || epoch <= last_epoch_ || session_id.empty() ||
      adapter_instance_id.empty()) {
    throw std::logic_error("invalid verification session transition");
  }
  open_ = true;
  last_epoch_ = epoch;
  session_id_ = std::move(session_id);
  adapter_instance_id_ = std::move(adapter_instance_id);
  profile_.reset();
  actions_.clear();
  facts_.push_back({HostFactKind::SessionOpened, session_id_,
                    adapter_instance_id_});
}

void ProtocolBridge::close_session() {
  if (!open_ || std::ranges::any_of(actions_, [](const auto& action) {
        return !action.token_id.empty() && !action.complete;
      })) {
    throw std::logic_error("session cannot close with unresolved completion");
  }
  facts_.push_back({HostFactKind::SessionClosed, session_id_, {}});
  open_ = false;
}

void ProtocolBridge::register_action(std::string action_id) {
  if (!open_ || action_id.empty() ||
      std::ranges::any_of(actions_, [&](const auto& action) {
        return action.action_id == action_id;
      })) {
    throw std::logic_error("invalid action registration");
  }
  facts_.push_back({HostFactKind::ActionRegistered, action_id, {}});
  actions_.push_back({std::move(action_id), {}, false});
}

void ProtocolBridge::bind_completion(std::string_view action_id,
                                     std::string token_id) {
  const auto action = std::ranges::find_if(actions_, [&](const auto& candidate) {
    return candidate.action_id == action_id;
  });
  const auto duplicate = std::ranges::any_of(actions_, [&](const auto& candidate) {
    return candidate.token_id == token_id;
  });
  if (!open_ || action == actions_.end() || token_id.empty() || duplicate ||
      !action->token_id.empty()) {
    throw std::logic_error("invalid completion binding");
  }
  action->token_id = std::move(token_id);
  facts_.push_back(
      {HostFactKind::CompletionBound, action->action_id, action->token_id});
}

void ProtocolBridge::complete(std::string_view token_id) {
  const auto action = std::ranges::find_if(actions_, [&](const auto& candidate) {
    return candidate.token_id == token_id;
  });
  if (!open_ || action == actions_.end() || action->complete) {
    throw std::logic_error("unknown or duplicate completion");
  }
  action->complete = true;
  facts_.push_back(
      {HostFactKind::CompletionObserved, action->token_id, action->action_id});
}

void ProtocolBridge::report_profile(ProfileCapabilities profile) {
  if (!open_ || profile_ || profile.profile_id.empty() ||
      profile.platform_family.empty() || profile.capabilities.empty()) {
    throw std::logic_error("invalid profile report");
  }
  profile_ = std::move(profile);
  facts_.push_back({HostFactKind::ProfileReported, profile_->profile_id, {}});
}

const ProfileCapabilities* ProtocolBridge::reported_profile() const noexcept {
  return profile_ ? &*profile_ : nullptr;
}

const std::vector<HostFact>& ProtocolBridge::facts() const noexcept {
  return facts_;
}

}  // namespace axiom::verification::platform

#pragma once

#include "canvas/interaction/multi_contact_policy.hpp"
#include "canvas/input/pointer_sample.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>

namespace canvas::interaction {

enum class ContactDisposition : std::uint8_t {
  kPending, kInk, kViewportGesture, kIgnored, kTerminal
};

class MultiContactCoordinator final {
 public:
  explicit MultiContactCoordinator(
      MultiContactPolicy policy = MultiContactPolicy::kAutoIntent) noexcept
      : policy_(policy) {}

  [[nodiscard]] MultiContactPolicy policy() const noexcept { return policy_; }
  [[nodiscard]] ContactDisposition update(const input::PointerSample& sample) noexcept;
  [[nodiscard]] ContactDisposition disposition(const input::PointerKey& key) const noexcept;
  [[nodiscard]] bool viewportClaimed() const noexcept { return viewportClaimed_; }
  [[nodiscard]] bool canonicalMutation() const noexcept { return canonicalMutation_; }
  [[nodiscard]] std::size_t activeCount() const noexcept { return contacts_.size(); }
  [[nodiscard]] bool setPolicy(MultiContactPolicy policy) noexcept {
    if (!contacts_.empty()) return false;
    policy_ = policy;
    viewportClaimed_ = false;
    canonicalMutation_ = false;
    return true;
  }
  void reset() noexcept;

 private:
  struct Contact final {
    ContactDisposition disposition = ContactDisposition::kPending;
    float startX = 0.0F;
    float startY = 0.0F;
    float lastX = 0.0F;
    float lastY = 0.0F;
    float path = 0.0F;
  };
  [[nodiscard]] bool activatesInk(Contact& contact, float x, float y) noexcept;
  [[nodiscard]] bool tryViewportClaim() noexcept;
  MultiContactPolicy policy_;
  std::unordered_map<input::PointerKey, Contact, input::PointerKeyHash> contacts_;
  bool viewportClaimed_ = false;
  bool canonicalMutation_ = false;
};

}  // namespace canvas::interaction

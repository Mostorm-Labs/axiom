#pragma once

#include "canvas/interaction/multi_contact_coordinator.hpp"

#include <unordered_map>
#include <vector>

namespace canvas::interaction {

class ViewportInteractionController;

struct InteractionRoutingResult final {
  enum class Route : std::uint8_t { kInk, kViewport, kPending, kIgnored };
  ContactDisposition disposition = ContactDisposition::kIgnored;
  Route route = Route::kIgnored;
  bool routesToInk = false;
  bool cancelsInk = false;
  bool viewportClaimed = false;
  bool becameViewport = false;
  bool endedViewport = false;
  bool canonicalMutation = false;
  input::PointerSample routedSample{};
  std::vector<input::PointerKey> cancelPointers;
};

using InteractionRoute = InteractionRoutingResult::Route;

class CanvasInteractionCoordinator final {
 public:
  explicit CanvasInteractionCoordinator(
      MultiContactPolicy policy = MultiContactPolicy::kAutoIntent) noexcept
      : contacts_(policy) {}

  [[nodiscard]] InteractionRoutingResult route(
      const input::PointerSample& sample) noexcept;
  [[nodiscard]] InteractionRoutingResult route(
      const input::PointerSample& sample,
      const ViewportInteractionController& viewport) noexcept;
  [[nodiscard]] ContactDisposition disposition(
      const input::PointerKey& key) const noexcept {
    return contacts_.disposition(key);
  }
  [[nodiscard]] bool hasContact(const input::PointerKey& key) const noexcept {
    return samples_.contains(key);
  }
  [[nodiscard]] bool viewportClaimed() const noexcept {
    return contacts_.viewportClaimed();
  }
  [[nodiscard]] bool canonicalMutation() const noexcept {
    return contacts_.canonicalMutation();
  }
  [[nodiscard]] MultiContactPolicy policy() const noexcept { return contacts_.policy(); }
  [[nodiscard]] bool setPolicy(MultiContactPolicy policy) noexcept {
    return contacts_.setPolicy(policy);
  }
  [[nodiscard]] std::vector<input::PointerSample> viewportSamples() const;
  void reset() noexcept;

 private:
  MultiContactCoordinator contacts_;
  std::unordered_map<input::PointerKey, input::PointerSample,
                     input::PointerKeyHash> samples_;
};

}  // namespace canvas::interaction

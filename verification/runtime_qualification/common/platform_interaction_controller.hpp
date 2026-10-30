#pragma once

#include "canvas/input/platform_interaction_ingress.hpp"

#include <optional>

namespace canvas::ink_playground {

class PlatformInteractionController final {
 public:
  [[nodiscard]] input::PlatformInteractionIngressResult submit(
      const input::PlatformPointerBatch& batch) noexcept {
    return ingress_.submit(batch);
  }
  [[nodiscard]] input::PlatformInteractionIngressResult sourceLost(
      input::InputSourceId source, std::uint64_t sequence,
      std::uint64_t timestampNs) noexcept {
    return ingress_.sourceLost(source, sequence, timestampNs);
  }
  [[nodiscard]] std::size_t activeCount() const noexcept { return ingress_.activeCount(); }
  [[nodiscard]] std::optional<input::PointerKey> keyFor(
      input::InputSourceId source, input::PointerId pointer) const noexcept {
    return ingress_.keyFor(source, pointer);
  }

 private:
  input::PlatformInteractionIngress ingress_;
};

}  // namespace canvas::ink_playground

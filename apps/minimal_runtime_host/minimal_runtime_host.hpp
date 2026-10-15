#pragma once

#include "canvas/input/platform_interaction_ingress.hpp"
#include "canvas/interaction/interaction_dispatch_runtime.hpp"

#include <array>
#include <cstddef>
#include <unordered_set>

namespace canvas::minimal_runtime_host {

class MinimalRuntimeHost final : private interaction::InteractionDispatchPort {
 public:
  bool registerSource(input::InputSourceId source) noexcept;
  void setMode(interaction::DispatchMode mode) noexcept { mode_ = mode; }
  [[nodiscard]] bool submit(const input::PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] std::size_t eventCount(interaction::DispatchEvent event) const noexcept;

 private:
  [[nodiscard]] interaction::DispatchMode dispatchMode() const noexcept override { return mode_; }
  [[nodiscard]] bool selection(const input::PointerSample&) noexcept override;
  [[nodiscard]] bool creation(const input::PointerSample&) noexcept override;
  [[nodiscard]] bool pan(const input::PointerSample&) noexcept override;
  [[nodiscard]] bool brush(const input::PointerSample&) noexcept override;
  [[nodiscard]] bool eraser(const input::PointerSample&) noexcept override;
  [[nodiscard]] bool record(interaction::DispatchEvent event) noexcept;

  input::PlatformInteractionIngress ingress_;
  interaction::InteractionDispatchRuntime dispatch_{*this};
  interaction::DispatchMode mode_ = interaction::DispatchMode::kBrush;
  std::unordered_set<input::InputSourceId> sources_;
  std::array<std::size_t, 5> counts_{};
};

}  // namespace canvas::minimal_runtime_host

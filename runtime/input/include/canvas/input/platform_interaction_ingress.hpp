#pragma once

#include "canvas/input/active_pointer_registry.hpp"
#include "canvas/input/input_capture_gate.hpp"
#include "canvas/input/platform_input_contract.hpp"
#include "canvas/input/pointer_sample_batch.hpp"

#include <cstdint>
#include <unordered_map>
#include <optional>

namespace canvas::input {

struct PlatformInteractionIngressResult final {
  PointerSampleBatch normalized;
  bool accepted = false;
  bool terminalCancel = false;
  std::size_t began = 0;
  std::size_t ended = 0;
};

class PlatformInteractionIngress final {
 public:
  [[nodiscard]] PlatformInteractionIngressResult submit(
      const PlatformPointerBatch& batch) noexcept;
  [[nodiscard]] PlatformInteractionIngressResult sourceLost(
      InputSourceId source, std::uint64_t sequence, std::uint64_t timestampNs) noexcept;
  [[nodiscard]] bool accepts(const PointerKey& key) const noexcept {
    return registry_.accepts(key);
  }
  [[nodiscard]] std::size_t activeCount() const noexcept { return active_.size(); }
  [[nodiscard]] std::optional<PointerKey> keyFor(
      InputSourceId source, PointerId pointer) const noexcept {
    const auto it = active_.find(Identity{source, pointer});
    return it == active_.end() ? std::nullopt : std::optional<PointerKey>(it->second);
  }
  [[nodiscard]] const PassThroughInputCaptureGate& captureGate() const noexcept {
    return captureGate_;
  }

 private:
  struct Identity final {
    InputSourceId source = 0;
    PointerId pointer = 0;
    friend bool operator==(const Identity&, const Identity&) = default;
  };
  struct IdentityHash final {
    std::size_t operator()(const Identity& value) const noexcept {
      auto hash = std::hash<std::uint64_t>{}(value.source);
      hash ^= std::hash<std::uint64_t>{}(value.pointer) + (hash << 6U) + (hash >> 2U);
      return hash;
    }
  };

  [[nodiscard]] PointerSample normalize(const PlatformPointerSample& sample,
                                        const PointerKey& key) const noexcept;
  void cancelAll(PlatformInteractionIngressResult& result) noexcept;

  ActivePointerRegistry registry_;
  std::unordered_map<Identity, PointerKey, IdentityHash> active_;
  PassThroughInputCaptureGate captureGate_;
};

}  // namespace canvas::input

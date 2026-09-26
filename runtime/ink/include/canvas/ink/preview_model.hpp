#pragma once

#include "canvas/ink/ink_engine.hpp"

#include <cstdint>
#include <span>
#include <unordered_map>
#include <vector>

namespace canvas::ink {

struct PreviewSnapshot final {
  std::uint64_t strokeId = 0;
  std::uint64_t revision = 0;
  std::vector<StrokePoint> confirmed;
  std::vector<StrokePoint> predicted;
};

class PreviewModel final {
 public:
  bool begin(std::uint64_t strokeId) noexcept;
  bool beginKeyed(std::uint64_t strokeId) noexcept;
  bool updateKeyed(std::uint64_t strokeId,
                   std::span<const StrokePoint> confirmedAppend,
                   std::span<const StrokePoint> predictedTail);
  void cancelKeyed(std::uint64_t strokeId) noexcept;
  [[nodiscard]] const PreviewSnapshot* snapshot(std::uint64_t strokeId) const noexcept;
  [[nodiscard]] std::vector<PreviewSnapshot> keyedSnapshots() const;
  bool update(std::span<const StrokePoint> confirmedAppend,
              std::span<const StrokePoint> predictedTail);
  void cancel() noexcept;
  [[nodiscard]] const PreviewSnapshot& snapshot() const noexcept { return state_; }

 private:
  PreviewSnapshot state_;
  bool active_ = false;
  std::unordered_map<std::uint64_t, PreviewSnapshot> keyed_;
};

}  // namespace canvas::ink

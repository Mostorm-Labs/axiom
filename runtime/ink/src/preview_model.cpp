#include "canvas/ink/preview_model.hpp"

namespace canvas::ink {

bool PreviewModel::begin(std::uint64_t strokeId) noexcept {
  if (active_ || strokeId == 0) return false;
  state_ = PreviewSnapshot{};
  state_.strokeId = strokeId;
  active_ = true;
  return true;
}

bool PreviewModel::update(std::span<const StrokePoint> confirmedAppend,
                          std::span<const StrokePoint> predictedTail) {
  if (!active_ || (confirmedAppend.empty() && predictedTail.empty())) return false;
  state_.confirmed.insert(state_.confirmed.end(), confirmedAppend.begin(), confirmedAppend.end());
  state_.predicted.assign(predictedTail.begin(), predictedTail.end());
  ++state_.revision;
  return true;
}

void PreviewModel::cancel() noexcept {
  state_ = PreviewSnapshot{};
  active_ = false;
}

}  // namespace canvas::ink

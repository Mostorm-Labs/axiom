#include "canvas/ink/preview_model.hpp"

namespace canvas::ink {

bool PreviewModel::begin(std::uint64_t strokeId) noexcept {
  if (active_ || strokeId == 0) return false;
  state_ = PreviewSnapshot{};
  state_.strokeId = strokeId;
  active_ = true;
  return true;
}

bool PreviewModel::beginKeyed(std::uint64_t strokeId) noexcept {
  if (strokeId == 0 || keyed_.contains(strokeId)) return false;
  keyed_.emplace(strokeId, PreviewSnapshot{strokeId, 0, {}, {}});
  return true;
}

bool PreviewModel::updateKeyed(std::uint64_t strokeId,
                               std::span<const StrokePoint> confirmedAppend,
                               std::span<const StrokePoint> predictedTail) {
  auto it = keyed_.find(strokeId);
  if (it == keyed_.end() || (confirmedAppend.empty() && predictedTail.empty())) return false;
  it->second.confirmed.insert(it->second.confirmed.end(), confirmedAppend.begin(), confirmedAppend.end());
  it->second.predicted.assign(predictedTail.begin(), predictedTail.end());
  ++it->second.revision;
  return true;
}

void PreviewModel::cancelKeyed(std::uint64_t strokeId) noexcept { keyed_.erase(strokeId); }

const PreviewSnapshot* PreviewModel::snapshot(std::uint64_t strokeId) const noexcept {
  const auto it = keyed_.find(strokeId);
  return it == keyed_.end() ? nullptr : &it->second;
}

std::vector<PreviewSnapshot> PreviewModel::keyedSnapshots() const {
  std::vector<PreviewSnapshot> snapshots;
  snapshots.reserve(keyed_.size());
  for (const auto& [strokeId, snapshot] : keyed_) {
    (void)strokeId;
    snapshots.push_back(snapshot);
  }
  return snapshots;
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

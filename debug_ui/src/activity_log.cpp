#include "canvas/debug_ui/activity_log.hpp"

#include <algorithm>

namespace canvas::debug_ui {

DebugActivityLog::DebugActivityLog(std::size_t capacity)
    : capacity_(capacity == 0 ? 1 : capacity) {}

void DebugActivityLog::record(DebugActivityEntry entry) {
  std::lock_guard lock(mutex_);
  if (entry.sequence == 0) entry.sequence = nextSequence_++;
  else nextSequence_ = (std::max)(nextSequence_, entry.sequence + 1);
  if (entries_.size() >= capacity_) entries_.pop_front();
  entries_.push_back(std::move(entry));
}

DebugActivitySnapshot DebugActivityLog::snapshot() const {
  std::lock_guard lock(mutex_);
  DebugActivitySnapshot result{};
  result.entries.assign(entries_.begin(), entries_.end());
  for (const auto& entry : entries_) {
    if (entry.productReceipt.has_value()) result.productControl = entry.productReceipt;
    if (entry.surfaceReceipt.has_value()) result.surfaceControl = entry.surfaceReceipt;
  }
  return result;
}

DebugActivitySnapshot DebugActivityLog::readActivity() const noexcept { return snapshot(); }

std::size_t DebugActivityLog::size() const noexcept {
  std::lock_guard lock(mutex_);
  return entries_.size();
}

}  // namespace canvas::debug_ui

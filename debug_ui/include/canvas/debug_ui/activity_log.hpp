#pragma once

#include "canvas/debug_ui/activity.hpp"

#include <cstddef>
#include <deque>
#include <mutex>

namespace canvas::debug_ui {

class DebugActivityLog final : public DebugActivitySource {
 public:
  explicit DebugActivityLog(std::size_t capacity = 64);
  void record(DebugActivityEntry entry);
  [[nodiscard]] DebugActivitySnapshot snapshot() const;
  [[nodiscard]] DebugActivitySnapshot readActivity() const noexcept override;
  [[nodiscard]] std::size_t size() const noexcept;

 private:
  mutable std::mutex mutex_;
  std::size_t capacity_ = 64;
  std::uint64_t nextSequence_ = 1;
  std::deque<DebugActivityEntry> entries_;
};

}  // namespace canvas::debug_ui

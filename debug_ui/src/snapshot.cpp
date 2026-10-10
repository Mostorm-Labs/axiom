#include "canvas/debug_ui/snapshot.hpp"

namespace canvas::debug_ui {
MutexCopySnapshotChannel::MutexCopySnapshotChannel() : snapshot_{} {}
void MutexCopySnapshotChannel::publish(DebugSnapshot snapshot) {
  std::lock_guard lock(mutex_);
  snapshot_ = std::move(snapshot);
}
DebugSnapshot MutexCopySnapshotChannel::read() const {
  std::lock_guard lock(mutex_);
  return snapshot_;
}
DebugSnapshotStamp MutexCopySnapshotChannel::stamp() const {
  std::lock_guard lock(mutex_);
  return snapshot_.stamp;
}
}  // namespace canvas::debug_ui

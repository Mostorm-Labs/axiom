#include "canvas/debug_ui/snapshot.hpp"

namespace canvas::debug_ui {
MutexCopySnapshotChannel::MutexCopySnapshotChannel()
    : snapshot_(std::make_shared<const DebugSnapshot>()) {}
void MutexCopySnapshotChannel::publish(DebugSnapshot snapshot) {
    auto copy = std::make_shared<const DebugSnapshot>(std::move(snapshot));
    std::lock_guard lock(mutex_);
    snapshot_ = std::move(copy);
}
DebugSnapshot MutexCopySnapshotChannel::read() const {
    std::lock_guard lock(mutex_);
    return *snapshot_;
}
DebugSnapshotStamp MutexCopySnapshotChannel::stamp() const {
    std::lock_guard lock(mutex_);
    return snapshot_->stamp;
}
}  // namespace canvas::debug_ui

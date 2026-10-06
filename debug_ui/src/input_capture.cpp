#include "canvas/debug_ui/input_capture.hpp"
namespace canvas::debug_ui {
DebugInputOwner InputCaptureGate::begin(DebugInputSequence sequence, DebugInputOwner requested) {
    std::lock_guard lock(mutex_);
    const auto [it, inserted] = owners_.emplace(sequence, requested);
    return inserted ? it->second : it->second;
}
std::optional<DebugInputOwner> InputCaptureGate::owner(DebugInputSequence sequence) const {
    std::lock_guard lock(mutex_); const auto it = owners_.find(sequence);
    return it == owners_.end() ? std::nullopt : std::optional<DebugInputOwner>(it->second);
}
std::optional<DebugInputOwner> InputCaptureGate::route(DebugInputSequence sequence) const { return owner(sequence); }
bool InputCaptureGate::terminal(DebugInputSequence sequence) {
    std::lock_guard lock(mutex_); return owners_.erase(sequence) != 0;
}
std::size_t InputCaptureGate::clear() { std::lock_guard lock(mutex_); const auto count = owners_.size(); owners_.clear(); return count; }
}  // namespace canvas::debug_ui

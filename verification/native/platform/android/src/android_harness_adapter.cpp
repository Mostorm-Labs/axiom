#include <axiom/verification/android_harness_adapter.hpp>

namespace axiom::verification::platform {

void AndroidHarnessAdapter::create_canvas() {
  if (state_ == State::New) state_ = State::Created;
}

void AndroidHarnessAdapter::destroy_canvas() {
  if (state_ == State::Destroyed) return;
  state_ = State::Destroyed;
  surface_ready_ = false;
  surface_lost_ = true;
  document_attached_ = false;
  native_window_token_ = 0;
}

void AndroidHarnessAdapter::attach_host() {
  if (state_ == State::Created) state_ = State::HostAttached;
}

void AndroidHarnessAdapter::detach_host() {
  if (state_ == State::Destroyed) return;
  if (state_ == State::DocumentAttached) state_ = State::Created;
  if (state_ == State::HostAttached) state_ = State::Created;
  surface_ready_ = false;
  surface_lost_ = true;
  native_window_token_ = 0;
}

void AndroidHarnessAdapter::attach_document() {
  if (state_ == State::HostAttached && surface_ready_) {
    state_ = State::DocumentAttached;
    document_attached_ = true;
  }
}

void AndroidHarnessAdapter::app_background() {
  if (state_ == State::Destroyed) return;
  backgrounded_ = true;
}

void AndroidHarnessAdapter::app_foreground() {
  if (state_ == State::Destroyed) return;
  backgrounded_ = false;
}

void AndroidHarnessAdapter::update_metrics(const AndroidMetrics&) {
  if (state_ != State::Destroyed) ++metrics_generation_;
}

void AndroidHarnessAdapter::surface_available(std::uintptr_t native_window_token) {
  if (state_ == State::Destroyed || native_window_token == 0) return;
  surface_ready_ = true;
  surface_lost_ = false;
  native_window_token_ = native_window_token;
}

void AndroidHarnessAdapter::surface_lost(std::uint64_t generation) {
  if (state_ == State::Destroyed || generation != surface_generation_) return;
  surface_ready_ = false;
  surface_lost_ = true;
  native_window_token_ = 0;
}

void AndroidHarnessAdapter::rebind_surface(std::uintptr_t native_window_token) {
  if (state_ == State::Destroyed || !surface_lost_ || native_window_token == 0) return;
  ++surface_generation_;
  surface_ready_ = true;
  surface_lost_ = false;
  native_window_token_ = native_window_token;
}

void AndroidHarnessAdapter::device_lost(std::uint64_t generation) {
  if (state_ == State::Destroyed || generation != device_generation_) return;
  device_lost_ = true;
}

void AndroidHarnessAdapter::device_recover() {
  if (state_ == State::Destroyed || !device_lost_) return;
  device_lost_ = false;
  ++device_generation_;
}

void AndroidHarnessAdapter::hold_present_completions() {
  if (state_ != State::Destroyed) present_held_ = true;
}

std::uint64_t AndroidHarnessAdapter::semantic_revision() const noexcept {
  return semantic_revision_;
}
std::uint64_t AndroidHarnessAdapter::metrics_generation() const noexcept {
  return metrics_generation_;
}
std::uint64_t AndroidHarnessAdapter::surface_generation() const noexcept {
  return surface_generation_;
}
std::uint64_t AndroidHarnessAdapter::device_generation() const noexcept {
  return device_generation_;
}
bool AndroidHarnessAdapter::document_attached() const noexcept {
  return document_attached_;
}
bool AndroidHarnessAdapter::backgrounded() const noexcept {
  return backgrounded_;
}
bool AndroidHarnessAdapter::surface_ready() const noexcept {
  return surface_ready_;
}
std::uintptr_t AndroidHarnessAdapter::native_window_token() const noexcept {
  return native_window_token_;
}
AndroidTargetOwnership AndroidHarnessAdapter::target_ownership() const {
  return {"AXIOM", "ARC", true};
}
PublishDisposition AndroidHarnessAdapter::release_held_after_destroy() {
  if (!present_held_ || state_ != State::Destroyed) return PublishDisposition::Cancelled;
  present_held_ = false;
  return PublishDisposition::DroppedStaleScope;
}
std::size_t AndroidHarnessAdapter::events_after_destroy() const noexcept {
  return events_after_destroy_;
}

}  // namespace axiom::verification::platform

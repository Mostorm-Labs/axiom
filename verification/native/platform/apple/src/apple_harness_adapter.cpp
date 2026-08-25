#include <axiom/verification/apple_harness_adapter.hpp>

namespace axiom::verification::platform {

AppleHarnessAdapter::AppleHarnessAdapter(AppleDeviceClass device_class)
    : profile_(apple_verification_profile(device_class)) {}

void AppleHarnessAdapter::create_canvas() { if (state_ == State::New) state_ = State::Created; }
void AppleHarnessAdapter::destroy_canvas() {
  if (state_ == State::Destroyed) return;
  state_ = State::Destroyed;
  surface_ready_ = false;
  surface_lost_ = true;
  document_attached_ = false;
  drawable_token_ = 0;
}
void AppleHarnessAdapter::attach_host() { if (state_ == State::Created) state_ = State::HostAttached; }
void AppleHarnessAdapter::detach_host() {
  if (state_ == State::Destroyed) return;
  if (state_ == State::DocumentAttached || state_ == State::HostAttached) state_ = State::Created;
  surface_ready_ = false;
  surface_lost_ = true;
  drawable_token_ = 0;
}
void AppleHarnessAdapter::attach_document() {
  if (state_ == State::HostAttached && surface_ready_) {
    state_ = State::DocumentAttached;
    document_attached_ = true;
  }
}
void AppleHarnessAdapter::app_background() { if (state_ != State::Destroyed) backgrounded_ = true; }
void AppleHarnessAdapter::app_foreground() { if (state_ != State::Destroyed) backgrounded_ = false; }
void AppleHarnessAdapter::update_metrics(const AppleMetrics&) { if (state_ != State::Destroyed) ++metrics_generation_; }
void AppleHarnessAdapter::surface_available(std::uintptr_t drawable_token) {
  if (state_ == State::Destroyed || drawable_token == 0) return;
  surface_ready_ = true;
  surface_lost_ = false;
  drawable_token_ = drawable_token;
}
void AppleHarnessAdapter::surface_lost(std::uint64_t generation) {
  if (state_ == State::Destroyed || generation != surface_generation_) return;
  surface_ready_ = false;
  surface_lost_ = true;
  drawable_token_ = 0;
}
void AppleHarnessAdapter::rebind_surface(std::uintptr_t drawable_token) {
  if (state_ == State::Destroyed || !surface_lost_ || drawable_token == 0) return;
  ++surface_generation_;
  surface_ready_ = true;
  surface_lost_ = false;
  drawable_token_ = drawable_token;
}
void AppleHarnessAdapter::device_lost(std::uint64_t generation) {
  if (state_ == State::Destroyed || generation != device_generation_) return;
  device_lost_ = true;
}
void AppleHarnessAdapter::device_recover() {
  if (state_ == State::Destroyed || !device_lost_) return;
  device_lost_ = false;
  ++device_generation_;
}
void AppleHarnessAdapter::hold_present_completions() { if (state_ != State::Destroyed) present_held_ = true; }

const AppleVerificationProfile& AppleHarnessAdapter::profile() const noexcept { return profile_; }
std::uint64_t AppleHarnessAdapter::semantic_revision() const noexcept { return semantic_revision_; }
std::uint64_t AppleHarnessAdapter::metrics_generation() const noexcept { return metrics_generation_; }
std::uint64_t AppleHarnessAdapter::surface_generation() const noexcept { return surface_generation_; }
std::uint64_t AppleHarnessAdapter::device_generation() const noexcept { return device_generation_; }
bool AppleHarnessAdapter::document_attached() const noexcept { return document_attached_; }
bool AppleHarnessAdapter::backgrounded() const noexcept { return backgrounded_; }
bool AppleHarnessAdapter::surface_ready() const noexcept { return surface_ready_; }
std::uintptr_t AppleHarnessAdapter::drawable_token() const noexcept { return drawable_token_; }
AppleTargetOwnership AppleHarnessAdapter::target_ownership() const { return {"AXIOM", "ARC", true}; }
PublishDisposition AppleHarnessAdapter::release_held_after_destroy() {
  if (!present_held_ || state_ != State::Destroyed) return PublishDisposition::Cancelled;
  present_held_ = false;
  return PublishDisposition::DroppedStaleScope;
}

}  // namespace axiom::verification::platform

#include "canvas/interaction/gesture_arbiter.hpp"

namespace canvas::interaction {

bool GestureArbiter::claim(const input::PointerKey& key, GestureClaim claimValue) noexcept {
  if (!key.valid() || claimValue == GestureClaim::kNone) return false;
  const auto [it, inserted] = claims_.emplace(key, claimValue);
  return inserted || it->second == claimValue;
}

bool GestureArbiter::release(const input::PointerKey& key, GestureClaim claimValue) noexcept {
  const auto it = claims_.find(key);
  if (it == claims_.end() || it->second != claimValue) return false;
  claims_.erase(it);
  return true;
}

bool GestureArbiter::owns(const input::PointerKey& key, GestureClaim claimValue) const noexcept {
  return owner(key) == claimValue;
}

GestureClaim GestureArbiter::owner(const input::PointerKey& key) const noexcept {
  const auto it = claims_.find(key);
  return it == claims_.end() ? GestureClaim::kNone : it->second;
}

void GestureArbiter::cancel(const input::PointerKey& key) noexcept { claims_.erase(key); }

}  // namespace canvas::interaction

#include "canvas/interaction/multi_contact_coordinator.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::interaction {
namespace {
constexpr float kRadialSlop = 8.0F;
constexpr float kPathSlop = 12.0F;
}

bool MultiContactCoordinator::activatesInk(Contact& contact, float x, float y) noexcept {
  const float dx = x - contact.startX;
  const float dy = y - contact.startY;
  const float radial = std::sqrt(dx * dx + dy * dy);
  const float stepDx = x - contact.lastX;
  const float stepDy = y - contact.lastY;
  contact.path += std::sqrt(stepDx * stepDx + stepDy * stepDy);
  contact.lastX = x;
  contact.lastY = y;
  return radial >= kRadialSlop || contact.path >= kPathSlop;
}

bool MultiContactCoordinator::tryViewportClaim() noexcept {
  if (contacts_.size() < 2U || viewportClaimed_) return false;
  for (const auto& [key, contact] : contacts_) {
    static_cast<void>(key);
    if (contact.disposition != ContactDisposition::kPending) return false;
  }
  viewportClaimed_ = true;
  for (auto& [key, contact] : contacts_) {
    static_cast<void>(key);
    contact.disposition = ContactDisposition::kViewportGesture;
  }
  return true;
}

ContactDisposition MultiContactCoordinator::update(const input::PointerSample& sample) noexcept {
  if (!sample.key.valid()) return ContactDisposition::kIgnored;
  if (sample.predicted) {
    const auto existing = contacts_.find(sample.key);
    return existing == contacts_.end() ? ContactDisposition::kIgnored
                                        : existing->second.disposition;
  }
  if (sample.phase == input::PointerPhase::kDown) {
    if (const auto existing = contacts_.find(sample.key); existing != contacts_.end()) {
      return existing->second.disposition;
    }
    Contact contact;
    contact.startX = contact.lastX = sample.x;
    contact.startY = contact.lastY = sample.y;
    if (policy_ == MultiContactPolicy::kMultiInk ||
        (policy_ == MultiContactPolicy::kAutoIntent && !contacts_.empty() &&
         std::any_of(contacts_.begin(), contacts_.end(), [](const auto& entry) {
           return entry.second.disposition == ContactDisposition::kInk;
         }))) {
      contact.disposition = ContactDisposition::kInk;
      canonicalMutation_ = true;
    } else if (policy_ == MultiContactPolicy::kGesturePriority && !contacts_.empty()) {
      const bool inkLocked = std::any_of(contacts_.begin(), contacts_.end(), [](const auto& entry) {
        return entry.second.disposition == ContactDisposition::kInk;
      });
      if (inkLocked) contact.disposition = ContactDisposition::kIgnored;
    }
    const auto [it, inserted] = contacts_.emplace(sample.key, contact);
    if (!inserted) return ContactDisposition::kIgnored;
    if ((policy_ == MultiContactPolicy::kAutoIntent ||
         policy_ == MultiContactPolicy::kGesturePriority) && contacts_.size() >= 2U) {
      static_cast<void>(tryViewportClaim());
    }
    return it->second.disposition;
  }
  const auto it = contacts_.find(sample.key);
  if (it == contacts_.end()) return ContactDisposition::kIgnored;
  if (sample.phase == input::PointerPhase::kMove &&
      it->second.disposition == ContactDisposition::kPending) {
    if (policy_ == MultiContactPolicy::kGesturePriority ||
        policy_ == MultiContactPolicy::kMultiInk) {
      if (activatesInk(it->second, sample.x, sample.y) &&
          policy_ == MultiContactPolicy::kGesturePriority) {
        it->second.disposition = ContactDisposition::kInk;
        canonicalMutation_ = true;
      }
    } else if (activatesInk(it->second, sample.x, sample.y)) {
      it->second.disposition = ContactDisposition::kInk;
      canonicalMutation_ = true;
    }
  }
  const auto result = it->second.disposition;
  if (sample.phase == input::PointerPhase::kUp || sample.phase == input::PointerPhase::kCancel) {
    it->second.disposition = ContactDisposition::kTerminal;
    contacts_.erase(it);
    if (contacts_.empty()) viewportClaimed_ = false;
  }
  return result;
}

ContactDisposition MultiContactCoordinator::disposition(const input::PointerKey& key) const noexcept {
  const auto it = contacts_.find(key);
  return it == contacts_.end() ? ContactDisposition::kTerminal : it->second.disposition;
}

void MultiContactCoordinator::reset() noexcept {
  contacts_.clear();
  viewportClaimed_ = false;
  canonicalMutation_ = false;
}

}  // namespace canvas::interaction

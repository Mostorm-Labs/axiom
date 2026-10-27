#pragma once

#if !defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#error "FC08 checkpoints are private to the explicit qualification build"
#endif

#include "canvas/semantic/operation.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

namespace canvas::qualification {

enum class Point : std::uint8_t {
  kSemStaged, kSemReserved, kSemPublish, kHistBeforeReserve,
  kHistReady, kCmdAdmitted, kSemDone, kHistDone
};
inline constexpr std::array<std::string_view, 8> pointNames{
  "Q-SEM-STAGED", "Q-SEM-RESERVED", "Q-SEM-PUBLISH", "Q-HIST-BEFORE-RESERVE",
  "Q-HIST-READY", "Q-CMD-ADMITTED", "Q-SEM-DONE", "Q-HIST-DONE"};

enum PreparedResource : std::uint32_t {
  kApplyPlan = 1U, kChangeSet = 2U, kHistoryEntry = 4U, kPublished = 8U
};

struct Event final {
  Point point{};
  const void* owner = nullptr;
  std::array<std::uint8_t, 16> ownerIdentity{};
  std::uint64_t ownerEpoch = 0;
  semantic::OperationId operation{};
  semantic::DocumentId document{};
  std::optional<std::uint64_t> semanticGeneration;
  std::optional<std::uint64_t> commitOrdinal;
  std::optional<std::size_t> historySize;
  std::optional<std::size_t> historyCursor;
  std::uint32_t preparedResources = 0;
  bool rejectionSelected = false;
};

class Observer {
 public:
  virtual ~Observer() = default;
  // Trusted qualification clients only: bounded, noexcept, read-only and
  // non-reentrant. No store, submit port or mutable owner is supplied.
  virtual void observe(const Event&) noexcept = 0;
};

struct State final {
  const void* owner = nullptr;
  std::array<std::uint8_t, 16> identity{};
  std::uint64_t epoch = 0;
  std::shared_ptr<Observer> observer;
  std::optional<Point> armed;
  std::optional<semantic::OperationId> armedOperation;
  std::array<std::weak_ptr<State>, 2> related;
  bool active = true;
  bool observing = false;
};

inline bool postcommit(Point point) noexcept {
  return point == Point::kSemDone || point == Point::kHistDone;
}
inline void clearFault(State* state) noexcept {
  if (state) { state->armed.reset(); state->armedOperation.reset(); }
}
inline void clearGroup(State* state) noexcept {
  if (!state) return;
  clearFault(state);
  for (auto& weak : state->related) if (auto child = weak.lock()) clearFault(child.get());
}
inline void deactivate(State* state) noexcept {
  if (!state) return;
  clearFault(state);
  state->active = false;
  state->observer.reset();
}
inline void deactivateGroup(State* state) noexcept {
  if (!state) return;
  for (auto& weak : state->related) if (auto child = weak.lock()) deactivate(child.get());
  deactivate(state);
}
inline bool observing(const std::shared_ptr<State>& state) noexcept {
  return state && state->observing;
}

struct FaultScope final {
  State* state = nullptr;
  ~FaultScope() { clearGroup(state); }
};

struct ApplyContext final {
  State* state = nullptr;
  const void* owner = nullptr;
  std::uint64_t generation = 0;
  std::uint64_t ordinal = 0;
  std::uint32_t resources = kApplyPlan;
};

inline bool notify(State* state, Event event) noexcept {
  if (!state || !state->active || state->owner != event.owner || !state->observer) return false;
  if (state->observing) return !postcommit(event.point);
  event.ownerIdentity = state->identity;
  event.ownerEpoch = state->epoch;
  const bool reject = !postcommit(event.point) && state->armed == event.point &&
      (!state->armedOperation || *state->armedOperation == event.operation);
  event.rejectionSelected = reject;
  if (reject) clearFault(state);
  state->observing = true;
  const auto observer = state->observer;
  observer->observe(event);
  state->observing = false;
  return reject;
}
inline bool notify(const ApplyContext* context, Point point,
                   const semantic::Operation& operation) noexcept {
  if (!context) return false;
  Event event{};
  event.point = point; event.owner = context->owner;
  event.operation = operation.id; event.document = operation.document_id;
  event.semanticGeneration = context->generation; event.commitOrdinal = context->ordinal;
  event.preparedResources = context->resources;
  return notify(context->state, event);
}

// A handle never borrows the owner or observer. Destruction/disarm is safe even
// after the owner dies; all registration allocation occurs outside apply.
class Handle final {
 public:
  explicit Handle(const std::shared_ptr<State>& state) : state_(state) {}
  Handle(Handle&&) noexcept = default;
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  ~Handle() { if (auto state = state_.lock()) deactivateGroup(state.get()); }
  [[nodiscard]] bool arm(Point point, std::optional<semantic::OperationId> operation = {}) noexcept {
    const auto state = state_.lock();
    if (!state || !state->active || state->observing || postcommit(point)) return false;
    state->armed = point; state->armedOperation = operation; return true;
  }
  [[nodiscard]] bool armed() const noexcept {
    const auto state = state_.lock(); return state && state->active && state->armed.has_value();
  }
 private:
  std::weak_ptr<State> state_;
};

class Access;
} // namespace canvas::qualification

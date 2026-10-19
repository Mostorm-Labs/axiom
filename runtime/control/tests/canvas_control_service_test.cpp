#include "canvas/control/canvas_control_service.hpp"

#include <cassert>
#include <cstdint>
#include <string>

using namespace canvas::runtime;

namespace {
class Port final : public CanvasControlPort {
 public:
  CanvasTargetKey current{7, 11, 3, 9};
  bool isTargetCurrent(const CanvasTargetKey& target) const noexcept override { return target == current; }
  CanvasControlApplyResult apply(const CanvasControlRequest& request) override {
    ++applied;
    if (request.payload.kind == CanvasControlPayloadKind::kUndo && undoApplied) {
      return {CanvasControlError::kInternalFailure, "duplicate undo"};
    }
    if (request.payload.kind == CanvasControlPayloadKind::kUndo) undoApplied = true;
    return {};
  }
  CanvasControlSnapshot snapshot(const CanvasTargetKey& target) const override {
    CanvasControlSnapshot out{};
    out.target = target;
    out.controlRevision = revision;
    out.canUndo = !undoApplied;
    return out;
  }
  std::uint32_t applied = 0;
  std::uint64_t revision = 4;
  bool undoApplied = false;
};

CanvasControlRequest request(std::uint64_t client, std::uint64_t id,
                             CanvasTargetKey target, CanvasControlPayloadKind kind) {
  CanvasControlRequest out{};
  out.clientId = client;
  out.requestId = id;
  out.target = target;
  out.payload.kind = kind;
  return out;
}
}  // namespace

int main() {
  Port port;
  CanvasControlService service(port, 1, 8);
  const CanvasTargetKey live{7, 11, 3, 9};

  auto queued = service.enqueue(request(1, 1, live, CanvasControlPayloadKind::kUndo));
  assert(queued.state == CanvasControlReceiptState::kQueued);
  assert(service.processPending(1) == 1);
  assert(service.receipt({1, 1})->state == CanvasControlReceiptState::kApplied);
  assert(port.applied == 1);

  auto duplicate = service.enqueue(request(1, 1, live, CanvasControlPayloadKind::kUndo));
  assert(duplicate.state == CanvasControlReceiptState::kDuplicate);
  assert(port.applied == 1);

  auto stale = service.enqueue(request(2, 1, CanvasTargetKey{7, 11, 2, 9},
                                      CanvasControlPayloadKind::kPanBy));
  assert(stale.state == CanvasControlReceiptState::kStaleTarget);
  assert(service.processPending(1) == 0);

  auto first = service.enqueue(request(2, 2, live, CanvasControlPayloadKind::kPanBy));
  auto full = service.enqueue(request(3, 1, live, CanvasControlPayloadKind::kPanBy));
  assert(first.state == CanvasControlReceiptState::kQueued);
  assert(full.state == CanvasControlReceiptState::kQueueFull);

  service.destroyTarget(live);
  assert(service.receipt({2, 2})->state == CanvasControlReceiptState::kTargetDestroyed);
  assert(service.processPending(2) == 0);

  const auto afterDestroy = service.enqueue(request(4, 1, live,
                                                     CanvasControlPayloadKind::kRedo));
  assert(afterDestroy.state == CanvasControlReceiptState::kStaleTarget);

  const auto otherClient = service.enqueue(request(9, 1, CanvasTargetKey{7, 12, 1, 1},
                                                        CanvasControlPayloadKind::kPanBy));
  assert(otherClient.state == CanvasControlReceiptState::kStaleTarget);
  assert(service.processPending(2) == 0);
  assert(service.receipt({9, 1})->state == CanvasControlReceiptState::kStaleTarget);

  CanvasControlService bounded(port, 4, 1);
  const auto a = bounded.enqueue(request(20, 1, live, CanvasControlPayloadKind::kPanBy));
  assert(a.state == CanvasControlReceiptState::kQueued);
  assert(bounded.processPending(1) == 1);
  const auto b = bounded.enqueue(request(21, 1, live, CanvasControlPayloadKind::kPanBy));
  assert(b.state == CanvasControlReceiptState::kQueued);
  assert(bounded.processPending(2) == 1);
  assert(!bounded.receipt({20, 1}));
  // Evicting detailed receipts must not make an old Undo executable again.
  assert(bounded.enqueue(request(20, 1, live, CanvasControlPayloadKind::kUndo)).state ==
         CanvasControlReceiptState::kExpired);
  return 0;
}

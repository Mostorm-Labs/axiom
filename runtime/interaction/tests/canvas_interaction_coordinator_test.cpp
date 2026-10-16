#include "canvas/interaction/canvas_interaction_coordinator.hpp"
#include "canvas/interaction/interaction_dispatch_runtime.hpp"
#include "canvas/interaction/viewport_interaction_controller.hpp"

#include <cassert>

using namespace canvas::interaction;
using canvas::input::PointerKey;
using canvas::input::PointerPhase;
using canvas::input::PointerSample;

static PointerSample sample(PointerKey key, PointerPhase phase, float x, float y,
                            std::uint64_t sequence) {
  return PointerSample{sequence, sequence, x, y, 0.5F, false, key, {}, {}, phase};
}

int main() {
  CanvasInteractionCoordinator coordinator;
  ViewportInteractionController viewport;
  const PointerKey a{1, 1, 1};
  const PointerKey b{1, 2, 1};
  auto first = coordinator.route(sample(a, PointerPhase::kDown, 0.0F, 0.0F, 1));
  assert(first.disposition == ContactDisposition::kPending);
  assert(first.route == InteractionRoute::kInk);
  assert(first.routesToInk);
  auto second = coordinator.route(sample(b, PointerPhase::kDown, 20.0F, 0.0F, 2));
  assert(second.viewportClaimed && second.becameViewport);
  assert(second.route == InteractionRoute::kViewport);
  assert(!second.routesToInk);
  assert(coordinator.route(sample(a, PointerPhase::kMove, 40.0F, 0.0F, 3), viewport)
             .routedSample.x == 40.0F);
  assert(coordinator.viewportSamples().size() == 2U);
  assert(!coordinator.canonicalMutation());
  auto predicted = sample(a, PointerPhase::kMove, 60.0F, 0.0F, 3);
  predicted.predicted = true;
  const auto predictedRoute = coordinator.route(predicted);
  assert(predictedRoute.disposition == ContactDisposition::kViewportGesture);
  assert(predictedRoute.route == InteractionRoute::kViewport);
  auto upA = coordinator.route(sample(a, PointerPhase::kUp, 0.0F, 0.0F, 4));
  assert(upA.viewportClaimed && !upA.endedViewport);
  auto upB = coordinator.route(sample(b, PointerPhase::kUp, 20.0F, 0.0F, 5));
  assert(!upB.viewportClaimed && upB.endedViewport);
  assert(coordinator.viewportSamples().empty());

  coordinator.reset();
  assert(coordinator.route(sample(a, PointerPhase::kDown, 0.0F, 0.0F, 6)).route == InteractionRoute::kInk);
  assert(coordinator.route(sample(a, PointerPhase::kMove, 20.0F, 0.0F, 7)).route == InteractionRoute::kInk);
  assert(coordinator.route(sample(b, PointerPhase::kDown, 20.0F, 0.0F, 8)).route == InteractionRoute::kInk);
  assert(!coordinator.viewportClaimed());

  struct DispatchPort final : InteractionDispatchPort {
    DispatchMode mode = DispatchMode::kBrush;
    struct Event final { DispatchEvent kind; std::uint64_t session; canvas::input::PointerKey key; };
    std::vector<Event> events;
    DispatchMode dispatchMode() const noexcept override { return mode; }
    bool selection(const canvas::input::PointerSample& sample, std::uint64_t id) noexcept override {
      events.push_back({DispatchEvent::kSelection, id, sample.key}); return true;
    }
    bool creation(const canvas::input::PointerSample& sample, std::uint64_t id) noexcept override {
      events.push_back({DispatchEvent::kCreation, id, sample.key}); return true;
    }
    bool pan(const canvas::input::PointerSample& sample, std::uint64_t id) noexcept override {
      events.push_back({DispatchEvent::kPan, id, sample.key}); return true;
    }
    bool brush(const canvas::input::PointerSample& sample, std::uint64_t id) noexcept override {
      events.push_back({DispatchEvent::kBrush, id, sample.key}); return true;
    }
    bool eraser(const canvas::input::PointerSample& sample, std::uint64_t id) noexcept override {
      events.push_back({DispatchEvent::kEraser, id, sample.key}); return true;
    }
    bool cancel(DispatchMode mode, const canvas::input::PointerSample& sample,
                std::uint64_t id) noexcept override {
      events.push_back({mode == DispatchMode::kSelection ? DispatchEvent::kSelection :
                        mode == DispatchMode::kCreation ? DispatchEvent::kCreation :
                        mode == DispatchMode::kPan ? DispatchEvent::kPan :
                        mode == DispatchMode::kObjectEraser || mode == DispatchMode::kPartialEraser
                            ? DispatchEvent::kEraser : DispatchEvent::kBrush,
                        id, sample.key}); return true;
    }
  } port;
  InteractionDispatchRuntime dispatch(port);
  assert(dispatch.submit(sample(a, PointerPhase::kDown, 1.0F, 2.0F, 9)));
  assert(port.events.back().kind == DispatchEvent::kBrush);
  const auto brushSession = port.events.back().session;
  assert(brushSession != 0U);
  port.mode = DispatchMode::kSelection;
  assert(dispatch.submit(sample(a, PointerPhase::kMove, 3.0F, 4.0F, 10)));
  assert(port.events.back().kind == DispatchEvent::kBrush);
  assert(port.events.back().session == brushSession);
  assert(!dispatch.submit(sample(b, PointerPhase::kMove, 3.0F, 4.0F, 10)));
  port.mode = DispatchMode::kCreation;
  assert(dispatch.submit(sample(b, PointerPhase::kDown, 5.0F, 6.0F, 11)));
  assert(port.events.back().kind == DispatchEvent::kCreation);
  assert(port.events.back().session != brushSession);
  assert(dispatch.submit(sample(b, PointerPhase::kUp, 7.0F, 8.0F, 12)));
  assert(port.events.back().kind == DispatchEvent::kCreation);
  assert(dispatch.activeCount() == 1U);
  assert(dispatch.cancel(sample(a, PointerPhase::kCancel, 3.0F, 4.0F, 13)));
  assert(dispatch.activeCount() == 0U);
  assert(!dispatch.cancel(sample(a, PointerPhase::kCancel, 3.0F, 4.0F, 14)));
  return 0;
}

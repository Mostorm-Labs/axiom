#include "canvas/interaction/interaction_dispatch_runtime.hpp"

namespace canvas::interaction {

bool InteractionDispatchRuntime::submit(const input::PointerSample& sample) noexcept {
  switch (port_.dispatchMode()) {
    case DispatchMode::kSelection:
      return port_.selection(sample);
    case DispatchMode::kCreation:
      return port_.creation(sample);
    case DispatchMode::kPan:
      return port_.pan(sample);
    case DispatchMode::kObjectEraser:
    case DispatchMode::kPartialEraser:
      return port_.eraser(sample);
    case DispatchMode::kBrush:
      return port_.brush(sample);
  }
  return false;
}

}  // namespace canvas::interaction

#include "canvas/debug_ui/panel_model.hpp"
#include <cassert>
int main() {
  using namespace canvas::debug_ui;
  DebugSnapshot snapshot{};
  assert(!DebugPanelModel::canSubmitProductControl(snapshot));
  snapshot.platform.availability = DebugAvailability::kAvailable;
  snapshot.product.availability = DebugAvailability::kAvailable;
  snapshot.stamp.runtimeGeneration = 1;
  assert(DebugPanelModel::canSubmitProductControl(snapshot));
  for (const auto state : {DebugAvailability::kDegraded, DebugAvailability::kUnsupported,
                           DebugAvailability::kUnavailable, DebugAvailability::kError}) {
    snapshot.platform.availability = state;
    assert(!DebugPanelModel::canSubmitProductControl(snapshot));
  }
  snapshot.platform.availability = DebugAvailability::kAvailable;
  snapshot.stamp.runtimeGeneration = 0;
  assert(!DebugPanelModel::canSubmitProductControl(snapshot));
}

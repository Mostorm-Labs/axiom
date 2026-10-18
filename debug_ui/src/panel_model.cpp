#include "canvas/debug_ui/panel_model.hpp"
namespace canvas::debug_ui {
bool DebugPanelModel::canSubmitProductControl(const DebugSnapshot& snapshot) noexcept {
 return snapshot.product.availability == DebugAvailability::kAvailable &&
        snapshot.stamp.runtimeGeneration != 0U;
}
}

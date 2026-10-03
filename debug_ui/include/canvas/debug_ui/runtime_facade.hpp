#pragma once

#include "canvas/runtime/runtime_facade.hpp"

namespace canvas::debug_ui {
// Compatibility aliases keep the controller source independent from the
// concrete Runtime implementation while the ownership remains in
// canvas::runtime. ImGui is a consumer and does not implement this API.
using RuntimeFacade = canvas::runtime::RuntimeFacade;
using ProductControlRequest = canvas::runtime::ProductControlRequest;
using ProductControlReceipt = canvas::runtime::ProductControlReceipt;
using ProductControlAction = canvas::runtime::ProductControlAction;
}  // namespace canvas::debug_ui

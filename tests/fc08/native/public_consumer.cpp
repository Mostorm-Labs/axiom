#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <string>

namespace {
template<class T> constexpr bool hasContainerAPI = requires { &T::submitContainerIntent; };
}

std::string fc08PublicDiscovery() {
  canvas::runtime::RuntimeComposition composition;
  if (!composition.open()) return "{\"opened\":false}";
  canvas::runtime::RuntimeFacade& consumer = composition.controlFacade();
  const auto targets = consumer.mountedCanvasTargets();
  const auto state = consumer.readRuntimeState();
  (void)state;
  std::string result = "{\"opened\":true,\"mounted_target_count\":" + std::to_string(targets.size());
  result += ",\"container_typed_api\":";
  result += hasContainerAPI<canvas::runtime::RuntimeFacade> ? "true" : "false";
  result += "}";
  return result;
}

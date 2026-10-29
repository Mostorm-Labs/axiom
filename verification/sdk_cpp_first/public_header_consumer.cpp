#include "canvas/runtime/runtime_facade.hpp"
#include <type_traits>

// Compile-only, header-boundary check. This does not instantiate an SDK.
static_assert(std::is_abstract_v<canvas::runtime::RuntimeFacade>);
void inspect(canvas::runtime::RuntimeFacade& facade) {
    (void)facade.readSelectionSummary();
    (void)facade.readMultiSelection();
}

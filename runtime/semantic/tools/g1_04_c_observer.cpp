#include "g1_04_c_fixture_runtime.hpp"

// The C3 observer deliberately has no product mutation include. Fixture
// bootstrap and production pre-apply observation are kept in the runtime
// adapter; this translation unit is the facts-only observer boundary marker.
namespace canvas::verification::g1_04_c {
static_assert(sizeof(CorpusReport) > 0U);
} // namespace canvas::verification::g1_04_c

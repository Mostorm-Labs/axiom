#include "canvas/ink/legacy_v1_readonly.hpp"

namespace canvas::ink {
// The type is intentionally behaviour-only; V1 decoding/rendering remains in
// the existing semantic/document readers.
static_assert(sizeof(LegacyV1ReadOnly) > 0);
}

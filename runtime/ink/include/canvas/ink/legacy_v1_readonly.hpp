#pragma once

#include <cstdint>

namespace canvas::ink {

enum class LegacyV1ReadOnlyError : std::uint8_t {
    kNone = 0,
    kMutationRejected,
    kNewAuthoringRejected,
};

// V1 compatibility is deliberately a read-only capability. It exposes no
// writer or authoring bridge and therefore cannot reach BrushSession.
class LegacyV1ReadOnly final {
  public:
    [[nodiscard]] bool canDecode() const noexcept { return true; }
    [[nodiscard]] bool canRender() const noexcept { return true; }
    [[nodiscard]] LegacyV1ReadOnlyError beginAuthoring() const noexcept {
        return LegacyV1ReadOnlyError::kNewAuthoringRejected;
    }
    [[nodiscard]] LegacyV1ReadOnlyError mutate() const noexcept {
        return LegacyV1ReadOnlyError::kMutationRejected;
    }
};

}  // namespace canvas::ink

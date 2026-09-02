#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace canvas::semantic {

class ChangeSet;
namespace internal {
struct PreparedChangeSet;
[[nodiscard]] ChangeSet finalizeChangeSet(
    PreparedChangeSet prepared,
    SemanticGeneration before,
    SemanticGeneration after);
} // namespace internal

using FieldId = std::uint32_t;

enum class SemanticChangeFlags : std::uint8_t {
    kNone = 0,
    kCreated = 1U << 0U,
    kDeleted = 1U << 1U,
    kPlacement = 1U << 2U,
    kTransform = 1U << 3U,
    kProperties = 1U << 4U,
    kContent = 1U << 5U,
    kEraseMasks = 1U << 6U,
};

[[nodiscard]] constexpr SemanticChangeFlags operator|(
    SemanticChangeFlags left, SemanticChangeFlags right) noexcept {
    return static_cast<SemanticChangeFlags>(
        static_cast<std::uint8_t>(left) | static_cast<std::uint8_t>(right));
}

struct ObjectSemanticChange final {
    ObjectId object_id{};
    SemanticChangeFlags flags = SemanticChangeFlags::kNone;
    std::vector<FieldId> changed_fields;
};

// Immutable post-commit semantic-impact event. It intentionally carries no
// derived scene, renderer, tile, GPU, or visual-contribution information.
class ChangeSet final {
  public:
    [[nodiscard]] static ChangeSet fromChanges(
        SemanticGeneration before_generation,
        SemanticGeneration after_generation,
        std::vector<ObjectSemanticChange> objects);

    [[nodiscard]] constexpr const SemanticGeneration& beforeGeneration() const noexcept {
        return before_generation_;
    }
    [[nodiscard]] constexpr const SemanticGeneration& afterGeneration() const noexcept {
        return after_generation_;
    }
    [[nodiscard]] std::span<const ObjectSemanticChange> objects() const noexcept {
        return objects_;
    }

  private:
    friend ChangeSet internal::finalizeChangeSet(
        internal::PreparedChangeSet,
        SemanticGeneration,
        SemanticGeneration);

    ChangeSet(
        SemanticGeneration before_generation,
        SemanticGeneration after_generation,
        std::vector<ObjectSemanticChange> objects) noexcept;

    SemanticGeneration before_generation_{};
    SemanticGeneration after_generation_{};
    std::vector<ObjectSemanticChange> objects_;
};

} // namespace canvas::semantic

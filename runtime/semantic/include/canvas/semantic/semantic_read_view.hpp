#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace canvas::semantic {

// Synchronous, non-owning view of canonical semantic state at a captured
// SemanticGeneration. This facade does not create a snapshot or own a second
// copy of semantic state; callers must not retain it across canonical mutation.
class SemanticReadView final {
  public:
    SemanticReadView(
        const ObjectStore& objects,
        SemanticGeneration generation) noexcept
        : objects_(objects), generation_(generation) {}

    [[nodiscard]] SemanticGeneration generation() const noexcept {
        return generation_;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return objects_.size();
    }

    [[nodiscard]] bool contains(const ObjectId& id) const noexcept {
        return objects_.contains(id);
    }

    [[nodiscard]] const ObjectRecord* find(const ObjectId& id) const noexcept {
        return objects_.find(id);
    }

    [[nodiscard]] std::vector<ObjectRecord> allObjects() const {
        return objects_.allObjects();
    }

    [[nodiscard]] std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const {
        return objects_.children(parent_id);
    }

  private:
    const ObjectStore& objects_;
    SemanticGeneration generation_;
};

} // namespace canvas::semantic

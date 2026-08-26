#include "canvas/semantic/change_set.hpp"

#include <algorithm>
#include <utility>

namespace canvas::semantic {

ChangeSet::ChangeSet(
    SemanticGeneration before_generation,
    SemanticGeneration after_generation,
    std::vector<ObjectSemanticChange> objects) noexcept
    : before_generation_(before_generation),
      after_generation_(after_generation),
      objects_(std::move(objects)) {}

ChangeSet ChangeSet::fromChanges(
    SemanticGeneration before_generation,
    SemanticGeneration after_generation,
    std::vector<ObjectSemanticChange> objects) {
    std::sort(objects.begin(), objects.end(), [](const auto& left, const auto& right) {
        return left.object_id < right.object_id;
    });

    std::vector<ObjectSemanticChange> merged;
    merged.reserve(objects.size());
    for (auto& change : objects) {
        std::sort(change.changed_fields.begin(), change.changed_fields.end());
        change.changed_fields.erase(
            std::unique(change.changed_fields.begin(), change.changed_fields.end()),
            change.changed_fields.end());
        if (!merged.empty() && merged.back().object_id == change.object_id) {
            auto& existing = merged.back();
            existing.flags = existing.flags | change.flags;
            existing.changed_fields.insert(
                existing.changed_fields.end(),
                change.changed_fields.begin(),
                change.changed_fields.end());
            std::sort(existing.changed_fields.begin(), existing.changed_fields.end());
            existing.changed_fields.erase(
                std::unique(existing.changed_fields.begin(), existing.changed_fields.end()),
                existing.changed_fields.end());
            continue;
        }
        merged.push_back(std::move(change));
    }
    return ChangeSet(before_generation, after_generation, std::move(merged));
}

} // namespace canvas::semantic

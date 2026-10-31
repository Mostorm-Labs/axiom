#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/foundation/document_background.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace canvas::semantic {
namespace internal { class ObjectStoreMutator; }

// Read-only semantic object storage boundary. Canonical writes remain private
// to Semantic Core and will be driven by OperationEngine in a later gate.
class ObjectStore {
  public:
    virtual ~ObjectStore() = default;
    // Document-level canonical metadata shares this store's lifetime; it is not
    // an ObjectRecord and never appears in size(), selection or object scans.
    [[nodiscard]] foundation::DocumentBackground documentBackground() const noexcept { return background_; }

    [[nodiscard]] virtual std::size_t size() const noexcept = 0;
    [[nodiscard]] virtual bool contains(const ObjectId& id) const noexcept = 0;
    [[nodiscard]] virtual const ObjectRecord* find(const ObjectId& id) const noexcept = 0;

    // This is an implementation/verification scan ordered by ObjectId bytes;
    // it is not product z-order, Placement order, or a wire ordering promise.
    [[nodiscard]] virtual std::vector<ObjectRecord> allObjects() const = 0;

    // Children are ordered by Placement.OrderKey's unsigned-byte comparison,
    // then ObjectId only as an implementation-deterministic tie-break.
    [[nodiscard]] virtual std::vector<ObjectRecord> children(
        const std::optional<ObjectId>& parent_id) const = 0;
  private:
    friend class internal::ObjectStoreMutator;
    foundation::DocumentBackground background_{};
};

} // namespace canvas::semantic

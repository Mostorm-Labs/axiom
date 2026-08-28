#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/operation_fingerprint.hpp"

#include <optional>

namespace canvas::semantic {

struct AppliedOperationEntry final {
    Operation canonical_operation{};
    std::optional<OperationFingerprint> fingerprint;
};

class AppliedOperationView {
  public:
    virtual ~AppliedOperationView() = default;
    [[nodiscard]] virtual std::optional<AppliedOperationEntry> find(
        const OperationId& id) const = 0;
};

struct StatefulValidationContext final {
    const ObjectStore& objects;
    const AppliedOperationView& applied_operations;
};

} // namespace canvas::semantic

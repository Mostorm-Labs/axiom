#pragma once

#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/staged_object_view.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <cstdint>

namespace canvas::semantic {

[[nodiscard]] StatefulResult validateConnectorReferences(
    const StagedObjectView& staged, const ConnectorContent& content);

[[nodiscard]] bool isConnectableObjectKind(
    ObjectKind kind, std::uint32_t kind_version) noexcept;

[[nodiscard]] StatefulResult validateStablePortForTarget(
    const ObjectRecord& target, const StablePortAnchor& anchor);

} // namespace canvas::semantic

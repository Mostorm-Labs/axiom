#pragma once
#include "canvas/semantic/container_operation.hpp"
namespace canvas::semantic { class ObjectStore; struct PreparedApplyPlan; struct StatefulResult; }
namespace canvas::semantic::internal {
bool normalizeContainerOperation(SetSectionContentOp&);
bool normalizeContainerOperation(EditContainerTopologyOp&);
bool validateContainerOperation(const SetSectionContentOp&) noexcept;
bool validateContainerOperation(const EditContainerTopologyOp&) noexcept;
StatefulResult prepareContainerOperation(const SetSectionContentOp&, const ObjectStore&, PreparedApplyPlan&);
StatefulResult prepareContainerOperation(const EditContainerTopologyOp&, const ObjectStore&, PreparedApplyPlan&);
} // namespace canvas::semantic::internal

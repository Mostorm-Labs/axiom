#pragma once

#include "canvas/semantic/object_store.hpp"
#include "canvas/semantic/operation_payload.hpp"
#include "canvas/semantic/stateful_validation.hpp"

#include <vector>

namespace canvas::semantic {

struct CreateObjectsStatePlanInputs final { std::vector<ObjectRecord> creates; };
struct ReplaceObjectsStatePlanInputs final { std::vector<ObjectRecord> replacements; };
struct SplitStrokesStatePlanInputs final {
    std::vector<ObjectId> source_delete_ids;
    std::vector<ObjectRecord> replacement_creates;
};

[[nodiscard]] StatefulResult validateInsertObjectsState(const InsertObjectsOp&, const ObjectStore&, CreateObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetPlacementsState(const SetPlacementsOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetTransformsState(const SetTransformsOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validatePatchPropertiesState(const PatchPropertiesOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetObjectSizeState(const SetObjectSizeOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetVectorPathGeometryState(const SetVectorPathGeometryOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetImageContentState(const SetImageContentOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateAddStrokeState(const AddStrokeOp&, const ObjectStore&, CreateObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSplitStrokesState(const SplitStrokesOp&, const ObjectStore&, SplitStrokesStatePlanInputs* out);
[[nodiscard]] StatefulResult validateAddEraseMasksState(const AddEraseMasksOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateRemoveEraseMasksState(const RemoveEraseMasksOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateEditRichTextState(const EditRichTextOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);
[[nodiscard]] StatefulResult validateSetConnectorContentState(const SetConnectorContentOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs* out);

} // namespace canvas::semantic

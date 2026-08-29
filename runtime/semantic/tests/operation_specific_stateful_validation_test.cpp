#include "canvas/semantic/operation_specific_validation.hpp"

#include <gtest/gtest.h>
#include <type_traits>

namespace canvas::semantic {
namespace {
using InsertFn = StatefulResult (*)(const InsertObjectsOp&, const ObjectStore&, CreateObjectsStatePlanInputs*);
using PlacementFn = StatefulResult (*)(const SetPlacementsOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using TransformFn = StatefulResult (*)(const SetTransformsOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using PatchFn = StatefulResult (*)(const PatchPropertiesOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using SizeFn = StatefulResult (*)(const SetObjectSizeOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using PathFn = StatefulResult (*)(const SetVectorPathGeometryOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using ImageFn = StatefulResult (*)(const SetImageContentOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using AddStrokeFn = StatefulResult (*)(const AddStrokeOp&, const ObjectStore&, CreateObjectsStatePlanInputs*);
using SplitFn = StatefulResult (*)(const SplitStrokesOp&, const ObjectStore&, SplitStrokesStatePlanInputs*);
using AddMaskFn = StatefulResult (*)(const AddEraseMasksOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using RemoveMaskFn = StatefulResult (*)(const RemoveEraseMasksOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using RichFn = StatefulResult (*)(const EditRichTextOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
using ConnectorFn = StatefulResult (*)(const SetConnectorContentOp&, const ObjectStore&, ReplaceObjectsStatePlanInputs*);
static_assert(std::is_same_v<decltype(&validateInsertObjectsState), InsertFn>);
static_assert(std::is_same_v<decltype(&validateSetPlacementsState), PlacementFn>);
static_assert(std::is_same_v<decltype(&validateSetTransformsState), TransformFn>);
static_assert(std::is_same_v<decltype(&validatePatchPropertiesState), PatchFn>);
static_assert(std::is_same_v<decltype(&validateSetObjectSizeState), SizeFn>);
static_assert(std::is_same_v<decltype(&validateSetVectorPathGeometryState), PathFn>);
static_assert(std::is_same_v<decltype(&validateSetImageContentState), ImageFn>);
static_assert(std::is_same_v<decltype(&validateAddStrokeState), AddStrokeFn>);
static_assert(std::is_same_v<decltype(&validateSplitStrokesState), SplitFn>);
static_assert(std::is_same_v<decltype(&validateAddEraseMasksState), AddMaskFn>);
static_assert(std::is_same_v<decltype(&validateRemoveEraseMasksState), RemoveMaskFn>);
static_assert(std::is_same_v<decltype(&validateEditRichTextState), RichFn>);
static_assert(std::is_same_v<decltype(&validateSetConnectorContentState), ConnectorFn>);
TEST(OperationSpecificStatefulValidation, PublicBoundaryCompiles) { SUCCEED(); }
} // namespace
} // namespace canvas::semantic

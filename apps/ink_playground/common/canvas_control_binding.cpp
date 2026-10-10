#include "canvas_control_binding.hpp"

#include <cmath>

namespace canvas::ink_playground {
namespace {
runtime::CanvasControlApplyResult failed(runtime::CanvasControlError error,
                                          const char* detail) {
  return {error, detail};
}
}

runtime::CanvasControlApplyResult CanvasControlBinding::apply(
    const runtime::CanvasControlRequest& request) {
  if (request.target != target_) return failed(runtime::CanvasControlError::kStaleTarget, "stale target");
  const auto& p = request.payload;
  bool applied = false;
  switch (p.kind) {
    case runtime::CanvasControlPayloadKind::kSelectTool:
      if (p.tool == runtime::CanvasToolKind::kSelect) applied = host_.setSelectionMode(true);
      else if (p.tool == runtime::CanvasToolKind::kPan)
        applied = host_.setPanTool(true);
      else if (p.tool == runtime::CanvasToolKind::kEraser)
        applied = host_.setSelectionMode(false) &&
                  host_.selectTool(InkPlaygroundHost::ToolMode::kPartialEraser);
      else
        applied = host_.setSelectionMode(false) &&
                  host_.selectTool(InkPlaygroundHost::ToolMode::kBrush);
      break;
    case runtime::CanvasControlPayloadKind::kSelectPreset:
      if (p.preset.profileId.empty()) return failed(runtime::CanvasControlError::kUnknownPreset, "empty preset");
      applied = host_.selectBrushProfile(p.preset.profileId, p.preset.revision);
      break;
    case runtime::CanvasControlPayloadKind::kPanBy:
      applied = host_.applyViewportNavigation({interaction::ViewportNavigationKind::kWheelPan,
                                               p.x, p.y, 0.0F, 0.0F, 1.0F});
      break;
    case runtime::CanvasControlPayloadKind::kZoomAt:
      applied = std::isfinite(p.scale) && p.scale > 0.0F &&
                host_.setViewportZoomAt(p.scale, p.x, p.y);
      break;
    case runtime::CanvasControlPayloadKind::kSetZoom:
      applied = std::isfinite(p.scale) && p.scale > 0.0F &&
                host_.setViewportZoomAt(
                    p.scale, p.x == 0.0F ? host_.surface().width * 0.5F : p.x,
                    p.y == 0.0F ? host_.surface().height * 0.5F : p.y);
      break;
    case runtime::CanvasControlPayloadKind::kFit:
      if (p.fitTarget == runtime::CanvasFitTarget::kSelection) applied = host_.fitViewportToSelection();
      else if (p.fitTarget == runtime::CanvasFitTarget::kObject) applied = host_.fitViewportToObject(p.objectId);
      else if (p.fitTarget == runtime::CanvasFitTarget::kWorldRect)
        applied = host_.fitViewportToContent(foundation::WorldRect{p.worldRect[0], p.worldRect[1],
                                                                    p.worldRect[2], p.worldRect[3]});
      else applied = host_.fitViewportToContent();
      break;
    case runtime::CanvasControlPayloadKind::kUndo:
      applied = host_.undo();
      break;
    case runtime::CanvasControlPayloadKind::kRedo:
      applied = host_.redo();
      break;
    case runtime::CanvasControlPayloadKind::kInkOptions:
      {
        const auto preset = host_.selectedBrushProfile();
        const auto presetRevision = host_.selectedBrushRevision();
        const auto defaults = host_.brushCatalogDefaults(preset, presetRevision);
        if (!defaults.has_value())
          return failed(runtime::CanvasControlError::kResourceUnavailable, "preset defaults unavailable");
        const auto current = host_.inkColor();
        const auto resolveFloat = [](const runtime::OptionPatch<float>& patch,
                                     float currentValue, float defaultValue) {
          return patch.kind == runtime::OptionPatchKind::kSet ? patch.value
              : patch.kind == runtime::OptionPatchKind::kClearOverride ? defaultValue
                                                                        : currentValue;
        };
        const auto resolveColor = [&](const runtime::OptionPatch<runtime::RgbaColor>& patch) {
          if (patch.kind == runtime::OptionPatchKind::kSet) return patch.value;
          if (patch.kind == runtime::OptionPatchKind::kClearOverride)
            return runtime::RgbaColor{defaults->red, defaults->green, defaults->blue,
                                      defaults->alpha};
          return runtime::RgbaColor{current[0], current[1], current[2], current[3]};
        };
        const auto color = resolveColor(p.ink.color);
        applied = host_.setInkOptions(
            resolveFloat(p.ink.size, host_.inkSize(), defaults->size),
            color.r, color.g, color.b, color.a,
            resolveFloat(p.ink.opacity, host_.inkOpacity(), defaults->opacity));
        if (applied) {
          const auto nextOverride = [](runtime::OptionPatchKind kind, bool current) {
            return kind == runtime::OptionPatchKind::kSet ? true
                : kind == runtime::OptionPatchKind::kClearOverride ? false : current;
          };
          host_.setInkOverrideState(
              nextOverride(p.ink.size.kind, host_.inkSizeOverridden()),
              nextOverride(p.ink.color.kind, host_.inkColorOverridden()),
              nextOverride(p.ink.opacity.kind, host_.inkOpacityOverridden()));
        }
      }
      break;
    case runtime::CanvasControlPayloadKind::kEraserOptions:
      {
        const auto mode = p.eraser.mode.kind == runtime::OptionPatchKind::kSet
            ? p.eraser.mode.value : host_.eraserMode();
        const auto diameter = p.eraser.diameterLogicalPx.kind == runtime::OptionPatchKind::kSet
            ? p.eraser.diameterLogicalPx.value : host_.eraserDiameterLogicalPx();
        if (mode != 1U && mode != 2U) return failed(runtime::CanvasControlError::kInvalidValue, "invalid eraser mode");
        if (!std::isfinite(diameter) || diameter <= 0.0F)
          return failed(runtime::CanvasControlError::kInvalidValue, "invalid eraser diameter");
        applied = host_.setEraserOptions(mode, diameter);
      }
      break;
  }
  if (!applied) return failed(runtime::CanvasControlError::kBusy, "owner rejected request");
  ++revision_;
  return {};
}

runtime::CanvasControlSnapshot CanvasControlBinding::snapshot(
    const runtime::CanvasTargetKey& target) const {
  runtime::CanvasControlSnapshot out = snapshot_;
  out.target = target;
  out.controlRevision = revision_;
  out.tool = host_.selectionMode() ? runtime::CanvasToolKind::kSelect
                                   : host_.panTool() ? runtime::CanvasToolKind::kPan
                                   : host_.toolMode() == InkPlaygroundHost::ToolMode::kPartialEraser
                                         ? runtime::CanvasToolKind::kEraser
                                         : runtime::CanvasToolKind::kInk;
  out.preset = {host_.selectedBrushProfile(), host_.selectedBrushRevision()};
  out.size = host_.inkSize();
  const auto color = host_.inkColor();
  out.color = {color[0], color[1], color[2], color[3]};
  out.opacity = host_.inkOpacity();
  out.eraserDiameterLogicalPx = host_.eraserDiameterLogicalPx();
  out.eraserMode = host_.eraserMode();
  out.targetAvailable = true;
  out.writable = true;
  out.busy = false;
  out.viewportWidth = static_cast<float>(host_.surface().width);
  out.viewportHeight = static_cast<float>(host_.surface().height);
  out.cameraScale = host_.viewportGesture().scale;
  out.cameraTranslationX = host_.viewportGesture().translationX;
  out.cameraTranslationY = host_.viewportGesture().translationY;
  out.canUndo = host_.canUndo();
  out.canRedo = host_.canRedo();
  out.selectedObjectCount = static_cast<std::uint32_t>(host_.selectedObjectCount());
  if (out.selectedObjectCount != 0U) out.selectedObject = host_.selectedPrimaryObject();
  return out;
}

}  // namespace canvas::ink_playground

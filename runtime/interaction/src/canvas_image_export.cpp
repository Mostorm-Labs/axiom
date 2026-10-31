#include "canvas/interaction/canvas_editing_runtime.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/visibility_resolver.hpp"
#if defined(CANVAS_RENDER_HAS_SKIA)
#include "canvas/render/image_encode.hpp"
#include "canvas/render/image_resource.hpp"
#endif
#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::interaction {
using runtime::ImageExportStatus;
runtime::ImageExportResult CanvasEditingRuntime::exportImage(
    const runtime::ImageExportOptions& options) noexcept {
  const auto error = [](ImageExportStatus status) {
    runtime::ImageExportResult result; result.status = status; return result;
  };
  if (std::this_thread::get_id() != ownerThread_) return error(ImageExportStatus::kWrongThread);
  if (closed_ || !surface_.available || !runtimeSceneHost_ || !sceneCoordinator_ || !viewportController_)
    return error(ImageExportStatus::kNotReady);
  if (imageExportActive_ || surfaceTransitionActive_ || historyPublicationPending_ ||
      explicitIntentActive_ || publicationCallbackActive_) return error(ImageExportStatus::kBusy);
  const auto generation = semanticGeneration_.current();
  if (options.expectedGeneration && *options.expectedGeneration != generation.value())
    return error(ImageExportStatus::kStale);
  if (!std::isfinite(options.scale) || options.scale <= 0 ||
      !std::isfinite(options.quality) || options.quality < 0 || options.quality > 1 ||
      options.padding > 1024 ||
      (options.format != runtime::ImageExportFormat::kPng && options.format != runtime::ImageExportFormat::kJpeg) ||
      (options.background != runtime::ImageExportBackground::kDocument &&
       options.background != runtime::ImageExportBackground::kSolid &&
       options.background != runtime::ImageExportBackground::kTransparent) ||
      (options.background == runtime::ImageExportBackground::kTransparent && options.format != runtime::ImageExportFormat::kPng))
    return error(ImageExportStatus::kInvalidArgument);
  for (const auto channel : options.backgroundColor)
    if (!std::isfinite(channel) || channel < 0 || channel > 1) return error(ImageExportStatus::kInvalidArgument);
  if (options.background == runtime::ImageExportBackground::kSolid && options.backgroundColor[3] != 1)
    return error(ImageExportStatus::kInvalidArgument);
#if !defined(CANVAS_RENDER_HAS_SKIA)
  return error(ImageExportStatus::kUnsupported);
#else
  struct Active { bool& value; explicit Active(bool& v):value(v){value=true;} ~Active(){value=false;} } active{imageExportActive_};
  try {
    if (!recoverHistoryScene()) return error(ImageExportStatus::kNotReady);
    const auto& scene = sceneCoordinator_->runtimeScene();
    auto region = options.bounds;
    if (!region) {
      std::optional<foundation::WorldRect> all;
      for (const auto& record : scene.records()) {
        if (!record.worldBounds.isFiniteAndOrdered()) return error(ImageExportStatus::kFailed);
        all = all ? foundation::unionRects(*all, record.worldBounds) : record.worldBounds;
      }
      if (all) {
        const auto visible = runtimeSceneHost_->query({*all});
        if (!visible) return error(ImageExportStatus::kFailed);
        std::optional<foundation::WorldRect> bounds;
        for (const auto id : visible.value().backToFront) {
          const auto* record = scene.find(id);
          if (!record || record->kind == semantic::ObjectKind::kGroup) continue;
          bounds = bounds ? foundation::unionRects(*bounds, record->worldBounds) : record->worldBounds;
        }
        if (bounds) region = std::array<double,4>{bounds->left,bounds->top,bounds->right,bounds->bottom};
      }
      if (!region) {
        const auto camera = viewportController_->state();
        if (!std::isfinite(camera.scale) || camera.scale <= 0) return error(ImageExportStatus::kNotReady);
        region = std::array<double,4>{-camera.translationX / camera.scale, -camera.translationY / camera.scale,
            (surface_.logicalWidth-camera.translationX) / camera.scale,
            (surface_.logicalHeight-camera.translationY) / camera.scale};
      }
    }
    auto bounds = *region;
    for (auto v : bounds) if (!std::isfinite(v)) return error(ImageExportStatus::kInvalidArgument);
    if (bounds[2] <= bounds[0] || bounds[3] <= bounds[1]) return error(ImageExportStatus::kInvalidArgument);
    const double width = std::ceil((bounds[2]-bounds[0])*options.scale + 2.0*options.padding);
    const double height = std::ceil((bounds[3]-bounds[1])*options.scale + 2.0*options.padding);
    if (!std::isfinite(width) || !std::isfinite(height) || width > 8192 || height > 8192 || width*height > 16777216)
      return error(ImageExportStatus::kTooLarge);
    if (width < 1 || height < 1) return error(ImageExportStatus::kInvalidArgument);
    bounds[0] -= options.padding/options.scale; bounds[1] -= options.padding/options.scale;
    bounds[2] = bounds[0]+width/options.scale; bounds[3] = bounds[1]+height/options.scale;
    foundation::WorldRect world{static_cast<float>(bounds[0]),static_cast<float>(bounds[1]),
                                 static_cast<float>(bounds[2]),static_cast<float>(bounds[3])};
    const float zoom = static_cast<float>(options.scale);
    const foundation::WorldPoint center{static_cast<float>(bounds[0]+(bounds[2]-bounds[0])/2),
                                        static_cast<float>(bounds[1]+(bounds[3]-bounds[1])/2)};
    if (!world.isFiniteAndOrdered() || world.left == world.right || world.top == world.bottom ||
        !std::isfinite(zoom) || zoom <= 0 || !std::isfinite(center.x) || !std::isfinite(center.y))
      return error(ImageExportStatus::kInvalidArgument);
    const auto w = static_cast<std::uint32_t>(width), h = static_cast<std::uint32_t>(height);
    render::RasterSkiaSurfaceProvider target;
    if (target.resize(w,h).code != render::BackendSubmissionCode::kAccepted) return error(ImageExportStatus::kFailed);
    const render::FrameState frame{viewId_,
      render::CameraState{center,zoom,0,render::CameraGeneration{1}},world,
      render::SurfaceMetrics{static_cast<float>(w),static_cast<float>(h),w,h,1,1},
      generation,runtimeSceneHost_->revision(),render::SurfaceGeneration{target.generation()},
      render::MetricsGeneration{1},render::FrameId{1},foundation::WorldRect{0,0,static_cast<float>(w),static_cast<float>(h)}};
    const auto visibility = render::VisibilityResolver::resolve(frame,*runtimeSceneHost_);
    if (!visibility) return error(ImageExportStatus::kFailed);
    auto references = render::DirectReferenceSource::build(frame,visibility.value(),scene);
    if (!references) return error(ImageExportStatus::kFailed);
    // Resolve only canonical text. Live editing presentation and selection
    // overrides never enter this frame; Sticky owns its clipped child label.
    for (auto& entry : references.value().entries) {
      if (textLayoutService_) {
        if (const auto* shape = std::get_if<semantic::ShapeContent>(&entry.record.content)) {
          entry.command = render::ShapeReferenceCommand{*shape,textLayoutService_->resolveShape(
              entry.record.objectId,*shape,entry.record.transform,false),false};
        }
        if (const auto* sticky = std::get_if<semantic::StickyContent>(&entry.record.content)) {
          for (const auto& child : scene.records()) {
            if (!child.placement.parent_id || *child.placement.parent_id != entry.record.objectId) continue;
            const auto* rich = std::get_if<semantic::RichTextContent>(&child.content);
            if (!rich) continue;
            entry.command = render::StickyReferenceCommand{*sticky,textLayoutService_->resolveStickyLabel(
                entry.record.objectId,*rich,*sticky,false,entry.record.transform),false};
            for (auto& other : references.value().entries)
              if (other.record.objectId == child.objectId) other.command = render::GroupReferenceCommand{};
            break;
          }
        }
      }
    }
    const auto validLayout = [](const auto& layout) {
      if (!layout || !layout->diagnostics.empty()) return false;
      for (const auto& font : layout->fonts) if (!font.bytes || font.bytes->empty()) return false;
      return true;
    };
    for (const auto& entry : references.value().entries) {
      if (const auto* image = std::get_if<render::ImageReferenceCommand>(&entry.command)) {
        if (!imageResolver_ || imageResolver_->resolve(image->content.resource_id)->state != render::ImageResourceState::kReady)
          return error(ImageExportStatus::kResourceUnavailable);
      }
      if(const auto* table=std::get_if<render::TableReferenceCommand>(&entry.command))if(!validLayout(table->layout))return error(ImageExportStatus::kResourceUnavailable);
      if (const auto* rich = std::get_if<render::RichTextReferenceCommand>(&entry.command))
        if (!validLayout(rich->layout)) return error(ImageExportStatus::kResourceUnavailable);
      if (const auto* shape = std::get_if<render::ShapeReferenceCommand>(&entry.command))
        if (shape->content.label && !validLayout(shape->labelLayout)) return error(ImageExportStatus::kResourceUnavailable);
      if (const auto* sticky = std::get_if<render::StickyReferenceCommand>(&entry.command))
        if (sticky->labelLayout && !validLayout(sticky->labelLayout)) return error(ImageExportStatus::kResourceUnavailable);
      if (const auto* section = std::get_if<render::SectionReferenceCommand>(&entry.command))
        if (!section->content.title.empty() && !validLayout(section->titleLayout)) return error(ImageExportStatus::kResourceUnavailable);
    }
    references.value().canonicalBytes.clear();
    references.value().digest.clear();
    auto background = semanticObjects_.documentBackground();
    if (!options.includePattern) background.mode = foundation::BackgroundMode::kPlain;
    if (options.background == runtime::ImageExportBackground::kSolid) {
      background.mode = foundation::BackgroundMode::kPlain; background.color = options.backgroundColor;
    }
    const auto plan = render::FramePlanBuilder::build(frame,references.value(),background,
        options.background == runtime::ImageExportBackground::kTransparent, true);
    if (!plan) return error(ImageExportStatus::kFailed);
    render::SkiaRenderer renderer(imageResolver_);
    if (renderer.renderFrame(target,plan.value()).code != render::BackendSubmissionCode::kAccepted)
      return error(ImageExportStatus::kFailed);
    auto bytes = render::encodeRasterImage(target,options.format == runtime::ImageExportFormat::kJpeg,
        static_cast<int>(std::round(options.quality*100)));
    if (bytes.empty()) return error(ImageExportStatus::kFailed);
    if (generation != semanticGeneration_.current()) return error(ImageExportStatus::kStale);
    return {ImageExportStatus::kExported,std::move(bytes),
        options.format == runtime::ImageExportFormat::kPng ? "image/png" : "image/jpeg",w,h,bounds,generation.value()};
  } catch (...) { return error(ImageExportStatus::kFailed); }
#endif
}
} // namespace canvas::interaction

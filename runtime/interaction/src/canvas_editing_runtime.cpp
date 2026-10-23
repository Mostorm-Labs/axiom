#include "canvas/interaction/canvas_editing_runtime.hpp"

#include "canvas/semantic/brush_engine_codec.hpp"
#include "canvas/semantic/commit_publication_projection.hpp"
#include "canvas/scene/direct_render_scene.hpp"
#include "canvas/scene/uniform_grid_spatial_index.hpp"
#include "canvas/scene/full_scene_compiler.hpp"
#include "canvas/scene/bounds_system.hpp"
#include "canvas/scene/derived_geometry.hpp"
#include "canvas/render/visibility_resolver.hpp"
#include "canvas/render/direct_reference_source.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/render/image_resource.hpp"

#include <cmath>
#include <memory>
#include <limits>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdio>
#include <array>
#include <iterator>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <sstream>
#include <type_traits>
#include <climits>
#define AXIOM_ANDROID_DIAG(...) std::fprintf(stderr, "[axiom] " __VA_ARGS__), std::fputc('\n', stderr)

namespace canvas::interaction {
namespace {
bool propertyBool(const semantic::PropertyBag& bag, std::uint32_t id, bool fallback) {
  for (const auto& entry : bag.entries) if (entry.field_id == id)
    if (const auto* value = std::get_if<bool>(&entry.value)) return *value;
  return fallback;
}
}
namespace {
bool validAppearance(const runtime::CanvasAppearance& a) {
  const auto validColor=[](runtime::RgbaColor c){
    for(const auto f:{c.r,c.g,c.b,c.a})if(!std::isfinite(f)||f<0||f>1)return false;
    return true;
  };
  return validColor(a.fill)&&validColor(a.stroke)&&std::isfinite(a.strokeWidth)&&
      a.strokeWidth>0&&std::isfinite(a.opacity)&&a.opacity>=0&&a.opacity<=1;
}
semantic::PropertyBag appearanceProperties(const runtime::CanvasAppearance& a,bool shape) {
  const auto color=[](runtime::RgbaColor c){return semantic::ColorValue{c.r,c.g,c.b,c.a};};
  semantic::PropertyBag bag;
  bag.entries.push_back({3U,a.opacity});
  if(shape)bag.entries.push_back({0x100U,semantic::FillStyleValue{
      a.fillEnabled?semantic::FillStyleValue{semantic::SolidFill{color(a.fill)}}:
                    semantic::FillStyleValue{semantic::NoFill{}}}});
  bag.entries.push_back({0x101U,semantic::StrokeStyleValue{
      a.strokeEnabled?semantic::StrokeStyleValue{semantic::SolidStroke{
          color(a.stroke),a.strokeWidth,semantic::StrokeCap::kButt,
          semantic::MiterJoin{4.0},semantic::SolidDash{}}}:
          semantic::StrokeStyleValue{semantic::NoStroke{}}}});
  return bag;
}

std::string sceneContentStamp(const canvas::scene::InspectionRecord& record) {
  std::ostringstream out;
  out.precision(17);
  out << record.referenceGeometryDigest << '|';
  for (const auto& entry : record.properties.entries) {
    out << entry.field_id << ':' << entry.value.index() << ':';
    std::visit([&](const auto& value) {
      using T = std::decay_t<decltype(value)>;
      if constexpr (std::is_same_v<T, bool> || std::is_same_v<T, float>) {
        out << value;
      } else if constexpr (std::is_same_v<T, semantic::BlendModeValue> ||
                           std::is_same_v<T, semantic::ConnectorDecorationValue>) {
        out << static_cast<unsigned>(value);
      } else if constexpr (std::is_same_v<T, semantic::ColorValue>) {
        out << value.r << ',' << value.g << ',' << value.b << ',' << value.a;
      } else if constexpr (std::is_same_v<T, semantic::FillStyleValue>) {
        out << value.index();
        std::visit([&](const auto& fill) {
          using F = std::decay_t<decltype(fill)>;
          if constexpr (std::is_same_v<F, semantic::SolidFill>) {
            out << fill.color.r << ',' << fill.color.g << ',' << fill.color.b << ',' << fill.color.a;
          }
        }, value);
      } else if constexpr (std::is_same_v<T, semantic::StrokeStyleValue>) {
        out << value.index();
        std::visit([&](const auto& stroke) {
          using S = std::decay_t<decltype(stroke)>;
          if constexpr (std::is_same_v<S, semantic::SolidStroke>) {
            out << stroke.color.r << ',' << stroke.color.g << ',' << stroke.color.b << ','
                << stroke.color.a << ':' << stroke.width << ':'
                  << static_cast<unsigned>(stroke.cap) << ':' << stroke.join.index() << ':';
                std::visit([&](const auto& join) {
                  using J = std::decay_t<decltype(join)>;
                  if constexpr (std::is_same_v<J, semantic::MiterJoin>) {
                    out << join.limit;
                  }
                }, stroke.join);
                out << ':' << stroke.dash.index() << ':';
                std::visit([&](const auto& dash) {
                  using D = std::decay_t<decltype(dash)>;
                  if constexpr (std::is_same_v<D, semantic::DashPattern>) {
                    out << dash.offset << ':' << dash.segments.size();
                    for (const auto segment : dash.segments) out << ':' << segment;
                  }
                }, stroke.dash);
          }
        }, value);
      }
    }, entry.value);
    out << '|';
  }
  for (const auto& mask : record.eraseMasks) {
    out << ':';
    for (const auto byte : mask.id.bytes) out << static_cast<unsigned>(byte) << ',';
    out << ':' << mask.geometry.index();
    std::visit([&](const auto& geometry) {
      using G = std::decay_t<decltype(geometry)>;
      if constexpr (std::is_same_v<G, semantic::SweptCircleMask>) {
        for (const auto& segment : geometry.segments) {
          for (const auto* knot : {&segment.p0, &segment.p1})
            out << knot->position.x << ',' << knot->position.y << ',' << knot->radius << ';';
          out << segment.control1.x << ',' << segment.control1.y << ','
              << segment.control2.x << ',' << segment.control2.y << '|';
        }
      } else {
        out << geometry.path.commands.size();
      }
    }, mask.geometry);
  }
  return out.str();
}

std::uint32_t sceneContentGeneration(const canvas::scene::InspectionRecord& record) {
  constexpr std::uint32_t kOffset = 2166136261U;
  constexpr std::uint32_t kPrime = 16777619U;
  auto hash = kOffset;
  for (const auto byte : sceneContentStamp(record)) {
    hash ^= static_cast<std::uint8_t>(byte);
    hash *= kPrime;
  }
  return hash == 0U ? 1U : hash;
}

canvas::SceneOrderKey renderOrderKey(const canvas::scene::InspectionRecord& record) {
  return canvas::SceneOrderKey(record.placement.order_key.bytes());
}

bool runtimeTraversalLess(const canvas::RuntimeSceneRecord& left,
                          const canvas::RuntimeSceneRecord& right) noexcept {
  const auto isInk = [](semantic::ObjectKind kind) noexcept {
    return kind == semantic::ObjectKind::kVectorStroke ||
           kind == semantic::ObjectKind::kDabStroke;
  };
  const bool leftInk = isInk(left.kind);
  const bool rightInk = isInk(right.kind);
  if (leftInk != rightInk) return !leftInk;
  if (left.placement.order_key != right.placement.order_key) {
    return left.placement.order_key < right.placement.order_key;
  }
  return left.objectId < right.objectId;
}

int fontCollectionIndex(const std::vector<std::uint8_t>& metadata) noexcept {
  std::size_t offset = 0U;
  while (offset < metadata.size()) {
    std::uint64_t key = 0U;
    unsigned shift = 0U;
    bool complete = false;
    while (offset < metadata.size() && shift < 64U) {
      const auto byte = metadata[offset++];
      key |= static_cast<std::uint64_t>(byte & 0x7FU) << shift;
      if ((byte & 0x80U) == 0U) { complete = true; break; }
      shift += 7U;
    }
    if (!complete || key == 0U || (key & 7U) != 0U) return 0;
    std::uint64_t value = 0U;
    shift = 0U; complete = false;
    while (offset < metadata.size() && shift < 64U) {
      const auto byte = metadata[offset++];
      value |= static_cast<std::uint64_t>(byte & 0x7FU) << shift;
      if ((byte & 0x80U) == 0U) { complete = true; break; }
      shift += 7U;
    }
    if (!complete) return 0;
    if ((key >> 3U) == 2U) {
      return value > static_cast<std::uint64_t>(INT_MAX) ? 0 : static_cast<int>(value);
    }
  }
  return 0;
}
}
class EditingSceneCompiler final : public canvas::ISemanticSceneCompiler {
 public:
  foundation::Result<canvas::CompiledSceneSnapshot> compileFull(
      const semantic::SemanticReadView& view) const override {
    const auto projection = canvas::scene::FullSceneCompiler::compile(view);
    if (!projection) return foundation::Result<canvas::CompiledSceneSnapshot>::failure(projection.error());
    canvas::CompiledSceneSnapshot result{canvas::SceneRevision(view.generation().value()), {}};
    result.records.reserve(projection.value().records.size());
    std::unordered_set<foundation::ObjectId, foundation::ObjectIdHash> activeIds;
    activeIds.reserve(projection.value().records.size());
    for (const auto& record : projection.value().records) activeIds.insert(record.objectId);
    for (auto it = renderSlots_.begin(); it != renderSlots_.end();) {
      it = activeIds.contains(it->first) ? std::next(it) : renderSlots_.erase(it);
    }
    for (auto it = hitSlots_.begin(); it != hitSlots_.end();) {
      it = activeIds.contains(it->first) ? std::next(it) : hitSlots_.erase(it);
    }
    const auto allocateSlot = [](auto& slots, auto& used, std::uint32_t candidate,
                                 foundation::ObjectId id) {
      if (const auto existing = slots.find(id); existing != slots.end()) return existing->second;
      for (;;) {
        if (used.insert(candidate).second) {
          slots.emplace(id, candidate);
          return candidate;
        }
        ++candidate;
      }
    };
    for (const auto& record : projection.value().records) {
      canvas::SceneObjectKind kind = canvas::SceneObjectKind::kShape;
      switch (record.kind) {
        case semantic::ObjectKind::kImage: kind = canvas::SceneObjectKind::kImage; break;
        case semantic::ObjectKind::kVectorPath: kind = canvas::SceneObjectKind::kVectorPath; break;
        case semantic::ObjectKind::kRichText: kind = canvas::SceneObjectKind::kRichText; break;
        case semantic::ObjectKind::kVectorStroke: kind = canvas::SceneObjectKind::kVectorStroke; break;
        case semantic::ObjectKind::kDabStroke: kind = canvas::SceneObjectKind::kDabStroke; break;
        case semantic::ObjectKind::kConnector: kind = canvas::SceneObjectKind::kConnector; break;
        case semantic::ObjectKind::kSticky: kind = canvas::SceneObjectKind::kSticky; break;
        default: break;
      }
      const auto flags = static_cast<canvas::SceneRecordFlags>(
          (propertyBool(record.properties, 1U, true) ? static_cast<std::uint32_t>(canvas::SceneRecordFlags::kVisible) : 0U) |
          static_cast<std::uint32_t>(canvas::SceneRecordFlags::kHitTestable) |
          (propertyBool(record.properties, 2U, false) ? static_cast<std::uint32_t>(canvas::SceneRecordFlags::kLocked) : 0U));
      std::uint64_t slot = 1469598103934665603ULL;
      for (const auto byte : record.objectId.bytes) { slot ^= byte; slot *= 1099511628211ULL; }
      const auto generation = sceneContentGeneration(record);
      const auto order = renderOrderKey(record);
      const auto renderSlot = allocateSlot(renderSlots_, renderSlotsUsed_, static_cast<std::uint32_t>(slot), record.objectId);
      const auto hitSlot = allocateSlot(hitSlots_, hitSlotsUsed_,
          static_cast<std::uint32_t>(slot ^ 0x9e3779b9U), record.objectId);
      result.records.push_back({record.objectId, canvas::SceneOrderKey(order.bytes()), kind, flags,
                                record.worldBounds, canvas::ContentRevision(generation),
                                canvas::RenderPayloadRef{renderSlot, generation},
                                canvas::HitGeometryRef{hitSlot, generation}, record.transform,
                                record.derivedGeometry, record.properties});
    }
    return foundation::Result<canvas::CompiledSceneSnapshot>::success(std::move(result));
  }
  foundation::Result<canvas::CompiledSceneDelta> compileDelta(
      const semantic::SemanticReadView& view, const semantic::ChangeSet& changes) const override {
    (void)view;
    (void)changes;
    return foundation::Result<canvas::CompiledSceneDelta>::failure(
        {foundation::ErrorCode::kRequiresFullRebuild, "editor scene changes use the shared full projection"});
  }
 private:
  mutable std::unordered_map<foundation::ObjectId, std::uint32_t,
                             foundation::ObjectIdHash> renderSlots_;
  mutable std::unordered_map<foundation::ObjectId, std::uint32_t,
                             foundation::ObjectIdHash> hitSlots_;
  mutable std::unordered_set<std::uint32_t> renderSlotsUsed_;
  mutable std::unordered_set<std::uint32_t> hitSlotsUsed_;
};
}  // namespace canvas::interaction

namespace {
constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

std::uint64_t hashBytes(std::uint64_t hash, const void* data, std::size_t size) noexcept {
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  for (std::size_t i = 0; i < size; ++i) {
    hash ^= bytes[i];
    hash *= kFnvPrime;
  }
  return hash;
}

template <typename T>
std::uint64_t hashValue(std::uint64_t hash, const T& value) noexcept {
  return hashBytes(hash, &value, sizeof(value));
}

std::int64_t quantize(double value) noexcept {
  return static_cast<std::int64_t>(std::llround(value * 1000.0));
}

std::uint64_t hashOutline(const std::vector<canvas::ink::reference::StrokeOutlinePoint>& outline) noexcept {
  auto hash = kFnvOffset;
  for (const auto& point : outline) {
    const auto x = quantize(point.x);
    const auto y = quantize(point.y);
    hash = hashValue(hash, x);
    hash = hashValue(hash, y);
  }
  return hash;
}

canvas::foundation::WorldRect transformedBounds(
    const canvas::foundation::WorldRect& bounds,
    const canvas::semantic::Transform2D& transform) noexcept {
  const std::array<canvas::foundation::WorldPoint, 4> corners{{
      {static_cast<float>(transform.a * bounds.left + transform.c * bounds.top + transform.tx),
       static_cast<float>(transform.b * bounds.left + transform.d * bounds.top + transform.ty)},
      {static_cast<float>(transform.a * bounds.right + transform.c * bounds.top + transform.tx),
       static_cast<float>(transform.b * bounds.right + transform.d * bounds.top + transform.ty)},
      {static_cast<float>(transform.a * bounds.right + transform.c * bounds.bottom + transform.tx),
       static_cast<float>(transform.b * bounds.right + transform.d * bounds.bottom + transform.ty)},
      {static_cast<float>(transform.a * bounds.left + transform.c * bounds.bottom + transform.tx),
       static_cast<float>(transform.b * bounds.left + transform.d * bounds.bottom + transform.ty)},
  }};
  canvas::foundation::WorldRect result{corners[0].x, corners[0].y, corners[0].x, corners[0].y};
  for (const auto& corner : corners) {
    result.left = std::min(result.left, corner.x);
    result.top = std::min(result.top, corner.y);
    result.right = std::max(result.right, corner.x);
    result.bottom = std::max(result.bottom, corner.y);
  }
  return result;
}

canvas::foundation::WorldRect viewBounds(
    const canvas::foundation::WorldRect& world,
    const canvas::interaction::ViewportGesture& viewport) noexcept {
  const float scale = viewport.scale > 0.0F && std::isfinite(viewport.scale)
      ? viewport.scale : 1.0F;
  return {world.left * scale + viewport.translationX,
          world.top * scale + viewport.translationY,
          world.right * scale + viewport.translationX,
          world.bottom * scale + viewport.translationY};
}

}  // namespace

namespace canvas::interaction {

CanvasEditingRuntime::CanvasEditingRuntime(render::ViewId viewId)
    : input_(std::make_unique<input::InputRouter>()),
      ink_(std::make_unique<ink::InkEngine>(ink::BrushDescriptor{1.0F})),
      preview_(std::make_unique<ink::PreviewModel>()),
      interaction_(std::make_unique<interaction::InteractionRuntime>(
          *this, *this, *this, *this, *this,static_cast<TextEditPort*>(this))),
      lifecycle_(std::make_unique<render::SurfaceLifecycle>(render::SurfaceSnapshot{
          viewId, render::SurfaceGeneration{1},
          render::MetricsGeneration{1},
          render::SurfaceMetrics{1.0F, 1.0F, 1, 1, 1.0F, 1.0F}})),
      tracker_(std::make_unique<render::PresentationTracker>(*lifecycle_)),
      coordinator_(std::make_unique<interaction::CanvasInteractionCoordinator>()),
      dispatchRuntime_(*this),
      viewportController_(std::make_unique<interaction::ViewportInteractionController>()),
      appBinding_(std::make_unique<render::RuntimeAppBinding>(
          viewId, render::SurfaceProfile{
              {"cpu-raster", render::RenderTargetKind::kCpuRaster,
               render::RenderTargetBackend::kRaster,
               render::RenderTargetFormat::kRgba8888,
               {1.0F, 1.0F, 1U, 1U, 1.0F, 1.0F},
               {false, false, false, true, false, false}},
              std::make_unique<render::RasterSkiaSurfaceProvider>()})),
      viewId_(viewId),
      runtimeSceneHost_(std::make_unique<canvas::Scene>(
          std::make_unique<canvas::DirectRenderScene>(),
          std::make_unique<canvas::UniformGridSpatialIndex>())),
      sceneBinding_(std::make_unique<canvas::SceneBinding>(*runtimeSceneHost_)),
      sceneCoordinator_(std::make_unique<canvas::IncrementalRuntimeCoordinator>(*sceneBinding_)),
      sceneCompiler_(std::make_unique<EditingSceneCompiler>()),
      history_(*this), ownerThread_(std::this_thread::get_id()) {
  appBinding_->setSessionCancelSink(this);
}

void CanvasEditingRuntime::bindResourceServices(
    render::ImageResourceResolver* images,
    text::RichTextLayoutService* textService) noexcept {
  skiaRenderer_.setImageResolver(images);
  previewRenderer_.setImageResolver(images);
  textLayoutService_ = textService;
  if (sceneCoordinator_ != nullptr) sceneCoordinator_->setTextLayoutService(textService);
}

void CanvasEditingRuntime::refreshResourceManifestServices() noexcept {
  if (textLayoutService_ != nullptr && resourceManifestPort_ != nullptr) {
    textLayoutService_->clearFonts();
    for (const auto& entry : resourceManifestPort_->entries) {
      if (entry.resource_kind != semantic::ResourceKind::kFont) continue;
      textLayoutService_->registerFont({entry.resource_id,
                                        entry.content_hash.substr(7U),
                                        fontCollectionIndex(entry.metadata)});
    }
  }
  (void)refreshResourceDerivedState();
}

bool CanvasEditingRuntime::refreshResourceDerivedState() noexcept {
  if (closed_ || std::this_thread::get_id() != ownerThread_) return false;
  if (sceneCoordinator_ != nullptr && textLayoutService_ != nullptr) {
    const auto refreshed = sceneCoordinator_->refreshTextResources();
    if (!refreshed) return false;
  }
  canonicalPresentationDirty_ = true;
  return true;
}

bool CanvasEditingRuntime::registerInputSource(input::InputSourceId source,
                                               std::string_view platform) noexcept {
  return !closed_ && appBinding_ != nullptr && appBinding_->registerInputSource({source, std::string(platform)});
}

void CanvasEditingRuntime::close() noexcept {
  if (closed_) return;
  if (surfaceTransitionActive_) {
    surfaceAbortRequested_ = true;
    surface_.available = false;
    closed_ = true;
    frameCoordinator_.close();
    return;
  }
  pendingCanonicalProvider_.reset();
  pendingPreviewProvider_.reset();
  cancelAllPointers();
  if (previewController_ != nullptr) (void)previewController_->cancel();
  frameCoordinator_.close();
  surface_.available = false;
  closed_ = true;
}

bool CanvasEditingRuntime::unregisterInputSource(input::InputSourceId source,
                                                  std::uint64_t sequence,
                                                  std::uint64_t timestampNs) noexcept {
  return appBinding_ != nullptr &&
      appBinding_->unregisterInputSource(source, sequence, timestampNs);
}

interaction::DispatchMode CanvasEditingRuntime::dispatchMode() const noexcept {
  if (selectionMode_) return interaction::DispatchMode::kSelection;
  if (creationMode_ || creationActive()) return interaction::DispatchMode::kCreation;
  if (panTool_) return interaction::DispatchMode::kPan;
  if (toolMode_ == ToolMode::kObjectEraser) return interaction::DispatchMode::kObjectEraser;
  if (toolMode_ == ToolMode::kPartialEraser) return interaction::DispatchMode::kPartialEraser;
  return interaction::DispatchMode::kBrush;
}

bool CanvasEditingRuntime::selection(const input::PointerSample& sample,
                                  std::uint64_t) noexcept {
  // Selection releases may legitimately reject their canonical commit (for
  // example when the operation-id frontier is exhausted) while the platform
  // input batch itself remains accepted.  The pre-dispatch Host path ignored
  // that commit result; keep the same boundary after routing through the
  // shared Runtime so a rejected no-op release cannot poison the input lane.
  if (!sample.predicted) {
    (void)selectionPointer(sample.key, sample.phase, sample.x, sample.y);
  }
  return true;
}

bool CanvasEditingRuntime::creation(const input::PointerSample& sample,
                                 std::uint64_t) noexcept {
  return sample.predicted || creationPointer(sample.key, sample.phase, sample.x, sample.y);
}

bool CanvasEditingRuntime::pan(const input::PointerSample& sample,
                            std::uint64_t) noexcept {
  if (!sample.key.valid()) return false;
  const auto session = sample.key;
  if (sample.phase == input::PointerPhase::kDown) {
    panPointers_[session] = {sample.x, sample.y};
    return true;
  }
  if (sample.phase == input::PointerPhase::kMove) {
    const auto it = panPointers_.find(session);
    if (it == panPointers_.end()) return true;
    const float dx = sample.x - it->second.first;
    const float dy = sample.y - it->second.second;
    if ((dx != 0.0F || dy != 0.0F) && !applyViewportNavigation(
            {interaction::ViewportNavigationKind::kWheelPan, -dx, -dy,
             sample.x, sample.y, 1.0F})) return false;
    it->second = {sample.x, sample.y};
    return true;
  }
  if (sample.phase == input::PointerPhase::kUp ||
      sample.phase == input::PointerPhase::kCancel) {
    panPointers_.erase(session);
  }
  return true;
}

bool CanvasEditingRuntime::brush(const input::PointerSample& sample,
                              std::uint64_t session) noexcept {
  if (!sample.key.valid()) return false;
  return brushBatch(std::span<const input::PointerSample>(&sample, 1U), session);
}

bool CanvasEditingRuntime::brushBatch(
    std::span<const input::PointerSample> samples,
    std::uint64_t session) noexcept {
  if (samples.empty()) return false;
  if (samples.front().phase == input::PointerPhase::kDown) {
    const auto profile = selectedBrushProfile_ == "vector-solid-v1" ? 1U
        : selectedBrushProfile_ == "marker-flat-v1" ? 2U
        : selectedBrushProfile_ == "chalk-grain-v1" ? 3U : 4U;
    if (!beginBrushSession(session, profile)) return false;
  }
  std::vector<ink::BrushSample> confirmed;
  std::vector<ink::BrushSample> predicted;
  confirmed.reserve(samples.size());
  for (const auto& sample : samples) {
    if (sample.phase == input::PointerPhase::kCancel) break;
    const ink::BrushSample point{sample.x, sample.y, sample.pressure,
                                 sample.capabilities.pressure, sample.sequence};
    if (sample.predicted) predicted.push_back(point);
    else {
      // A confirmed observation replaces the preceding speculative tail.
      predicted.clear();
      confirmed.push_back(point);
    }
  }
  if ((!confirmed.empty() || !predicted.empty()) &&
      !appendBrushSamples(session, confirmed, predicted, false)) return false;
  const auto& last = samples.back();
  if (!last.predicted && last.phase == input::PointerPhase::kUp)
    return finishBrushSession(session);
  if (!last.predicted && last.phase == input::PointerPhase::kCancel)
    return cancelBrushSession(session);
  return true;
}

bool CanvasEditingRuntime::eraser(const input::PointerSample& sample,
                               std::uint64_t session) noexcept {
  if (!sample.key.valid()) return false;
  if (sample.phase == input::PointerPhase::kDown && !sample.predicted &&
      !eraserBegin(session)) return false;
  if (sample.phase != input::PointerPhase::kCancel &&
      !eraserSample(session, sample.x, sample.y,
                    sample.phase != input::PointerPhase::kUp)) return false;
  if (sample.phase == input::PointerPhase::kUp && !sample.predicted) {
    const auto finished = eraserFinish(session);
    return finished;
  }
  if (sample.phase == input::PointerPhase::kCancel && !sample.predicted) {
    (void)eraserCancel(session);
  }
  return true;
}

bool CanvasEditingRuntime::cancel(interaction::DispatchMode mode,
                               const input::PointerSample& sample,
                               std::uint64_t session) noexcept {
  if (!sample.key.valid()) return false;
  switch (mode) {
    case interaction::DispatchMode::kBrush:
      return cancelBrushSession(session);
    case interaction::DispatchMode::kObjectEraser:
    case interaction::DispatchMode::kPartialEraser:
      return eraserCancel(session);
    case interaction::DispatchMode::kSelection:
      if (selectionMode_) {
        (void)selectionPointer(sample.key, input::PointerPhase::kCancel,
                                sample.x, sample.y);
      }
      return true;
    case interaction::DispatchMode::kCreation:
      creations_.erase(sample.key);
      creationFences_.erase(sample.key);
      if (creations_.empty() && !selectionTransformActive()) (void)clearTransientOverlay();
      canonicalPresentationDirty_ = true;
      return true;
    case interaction::DispatchMode::kPan:
      panPointers_.erase(sample.key);
      return true;
  }
  return false;
}

bool CanvasEditingRuntime::beginStroke(std::uint64_t strokeId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  strokeStartNs_ = 0;
  return ink_->begin(strokeId) && preview_->begin(strokeId) &&
         interaction_->startSession(strokeId);
}

bool CanvasEditingRuntime::beginStroke(const input::PointerKey& key, std::uint64_t strokeId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  if (!key.valid() || keyedStrokeIds_.contains(key)) return false;
  if (!ink_->begin(key, strokeId) || !preview_->beginKeyed(strokeId) ||
      !interaction_->startSession(key, strokeId)) return false;
  keyedStrokeIds_.emplace(key, strokeId);
  return true;
}

bool CanvasEditingRuntime::accept(const input::PointerSampleBatch& batch,
                               std::uint64_t observationTimeNs) {
  if (batch.samples.empty() ||
      input_->dispatch(batch) != input::DispatchDisposition::kDelivered) {
    return false;
  }
  if (interaction_->textSession() && interaction_->textSession()->read().phase != runtime::TextPhase::kClosed) return true;
  std::vector<ink::StrokePoint> confirmed;
  std::vector<ink::StrokePoint> predicted;
  confirmed.reserve(batch.samples.size());
  predicted.reserve(batch.samples.size());
  for (const auto& sample : batch.samples) {
    // Legacy/native ingress may omit phase on already-open sessions. Only
    // feed contacts into the coordinator once a down has established them or
    // the runtime already owns the key; otherwise preserve the keyed ink
    // session's existing append behavior.
    const bool trackedContact = sample.key.valid() &&
        (sample.phase == input::PointerPhase::kDown || coordinator_->hasContact(sample.key));
    interaction::InteractionRoutingResult routing{};
    if (trackedContact) {
      routing = coordinator_->route(sample, *viewportController_);
      if (routing.becameViewport) {
        for (const auto& key : routing.cancelPointers) {
          const auto existing = keyedStrokeIds_.find(key);
          if (existing == keyedStrokeIds_.end()) continue;
          (void)ink_->cancel(key);
          (void)preview_->cancelKeyed(existing->second);
          (void)interaction_->cancel(key, existing->second);
          keyedStrokeIds_.erase(existing);
        }
      }
      const auto viewportSamples = coordinator_->viewportSamples();
      if (viewportSamples.size() >= 2U) {
        (void)viewportController_->updateGesture(viewportSamples[0], viewportSamples[1]);
      }
      if (routing.endedViewport) viewportController_->endGesture();
      if (routing.cancelsInk && sample.phase != input::PointerPhase::kUp &&
          keyedStrokeIds_.contains(sample.key)) {
        const auto ignoredStroke = keyedStrokeIds_.at(sample.key);
        (void)ink_->cancel(sample.key);
        (void)preview_->cancelKeyed(ignoredStroke);
        (void)interaction_->cancel(sample.key, ignoredStroke);
        keyedStrokeIds_.erase(sample.key);
      }
    }
    const input::PointerSample contentSample = trackedContact ? routing.routedSample : sample;
    baselineTrace_.push_back({contentSample.sequence, contentSample.timestampNs,
                              sample.x, sample.y, contentSample.x, contentSample.y,
                              contentSample.pressure,
                              contentSample.key, contentSample.phase});
    baselineViewport_.push_back({contentSample.sequence, coordinator_->viewportClaimed(),
                                 viewportController_->state().scale,
                                 viewportController_->state().translationX,
                                 viewportController_->state().translationY});
    if (trackedContact && !routing.routesToInk) continue;
    const ink::StrokePoint point{contentSample.x, contentSample.y,
                                 contentSample.pressure};
    if (sample.predicted) {
      predicted.push_back(point);
    } else {
      if (!ink_->append(contentSample)) return false;
      confirmed.push_back(point);
    }
  }
  if (confirmed.empty() && predicted.empty()) {
    hud_.batch = batch.samples.size();
    return true;
  }
  const auto keyed = batch.samples.front().key;
  if (keyed.valid()) {
    const auto it = keyedStrokeIds_.find(keyed);
    if (it == keyedStrokeIds_.end() || !preview_->updateKeyed(it->second, confirmed, predicted)) return false;
  } else if (!preview_->update(confirmed, predicted)) return false;
  const auto firstNs = batch.samples.front().timestampNs;
  const auto lastNs = batch.samples.back().timestampNs;
  strokeStartNs_ = strokeStartNs_ == 0 ? firstNs : strokeStartNs_;
  hud_.batch = batch.samples.size();
  hud_.sampleHz = batch.samples.size() > 1 && lastNs > firstNs
                      ? static_cast<double>(batch.samples.size() - 1) * 1'000'000'000.0 /
                            static_cast<double>(lastNs - firstNs)
                      : 0.0;
  hud_.queueAgeMs = observationTimeNs >= lastNs
                        ? static_cast<double>(observationTimeNs - lastNs) / 1'000'000.0
                        : 0.0;
  hud_.inkMs = observationTimeNs >= strokeStartNs_
                   ? static_cast<double>(observationTimeNs - strokeStartNs_) / 1'000'000.0
                   : 0.0;
  if (keyed.valid()) {
    const auto* state = preview_->snapshot(keyedStrokeIds_.at(keyed));
    hud_.previewRevision = state == nullptr ? 0 : state->revision;
    hud_.predictionDepth = state == nullptr ? 0 : state->predicted.size();
  } else {
    hud_.previewRevision = preview_->snapshot().revision;
    hud_.predictionDepth = preview_->snapshot().predicted.size();
  }
  return true;
}

bool CanvasEditingRuntime::acceptPlatformBatch(const input::PlatformPointerBatch& batch,
                                            std::uint64_t observationTimeNs) {
  if (closed_ || surfaceTransitionActive_ || !surface_.available || std::this_thread::get_id() != ownerThread_ ||
      publicationCallbackActive_) return false;
  (void)observationTimeNs;
  if (batch.samples.empty()) return false;
  const auto source = batch.samples.front().source;
  if (appBinding_ == nullptr) return false;
  if (!appBinding_->hasInputSource(source)) {
    if (appBinding_->isRetiredInputSource(source)) return false;
    if (!appBinding_->registerInputSource({source, "platform"})) return false;
  }
  const auto routed = appBinding_->submit(source, batch);
  if (!routed.accepted && !routed.terminalCancel) { AXIOM_ANDROID_DIAG("ingress rejected samples=%zu", routed.normalized.samples.size()); return false; }
  if (imageCropPointer_) {
    // Retain normal ingress identities, then give Crop exclusive ownership.
    const auto consume = imageCropPointer_;
    for (const auto& sample : routed.normalized.samples) consume(sample);
    return routed.accepted || routed.terminalCancel;
  }
  std::vector<input::PointerSample> routedSamples;
  routedSamples.reserve(routed.normalized.samples.size());
  for (const auto& sample : routed.normalized.samples)
    if (!shapeExpandPointer(sample)) routedSamples.push_back(sample);
  if (routed.terminalCancel) shapeExpandPointerKeys_.clear();
  // Normalize/retire full pointer identities even while the text editor owns input.
  if(textTool_ || (interaction_->textSession() && interaction_->textSession()->read().phase!=runtime::TextPhase::kClosed) || !textPointerKeys_.empty()){
    for(const auto& sample:routedSamples)(void)textPointer(sample);
    if(routed.terminalCancel)textPointerKeys_.clear();
    return routed.accepted || routed.terminalCancel;
  }
  if (selectionMode_ || creationMode_ || creationActive() || panTool_) {
    for (const auto& sample : routedSamples) {
      if (!sample.key.valid()) continue;
      if (!dispatchRuntime_.submit(sample)) return false;
      if (selectionMode_ && !textTool_ &&
          (!interaction_->textSession() ||
           interaction_->textSession()->read().phase == runtime::TextPhase::kClosed))
        (void)observeTouchTextTap(sample);
    }
    if (routed.terminalCancel) cancelAllPointers();
    return routed.accepted || routed.terminalCancel;
  }
  bool previewDirty = false;
  std::vector<input::PointerSample> inkBatch;
  const auto flushInk = [&]() {
    if (inkBatch.empty()) return true;
    const bool accepted = dispatchRuntime_.submitBatch(inkBatch);
    inkBatch.clear();
    return accepted;
  };
  const auto viewportBefore = viewportController_->state();
  for (const auto& sample : routedSamples) {
    if (!sample.key.valid()) continue;
    if (panTool_) {
      if (sample.phase == input::PointerPhase::kDown) {
        panPointers_[sample.key] = {sample.x, sample.y};
      } else if (sample.phase == input::PointerPhase::kMove) {
        const auto it = panPointers_.find(sample.key);
        if (it != panPointers_.end()) {
          const float dx = sample.x - it->second.first;
          const float dy = sample.y - it->second.second;
          if ((dx != 0.0F || dy != 0.0F) && !applyViewportNavigation(
                  {interaction::ViewportNavigationKind::kWheelPan, -dx, -dy,
                   sample.x, sample.y, 1.0F})) return false;
          it->second = {sample.x, sample.y};
        }
      } else if (sample.phase == input::PointerPhase::kUp ||
                 sample.phase == input::PointerPhase::kCancel) {
        panPointers_.erase(sample.key);
      }
      baselineTrace_.push_back({sample.sequence, sample.timestampNs, sample.x, sample.y,
                                sample.x, sample.y, sample.pressure, sample.key,
                                sample.phase, interaction::ContactDisposition::kViewportGesture,
                                true, 0U, 0U});
      continue;
    }
    if (!inkBatch.empty() && (sample.key != inkBatch.front().key ||
        sample.phase == input::PointerPhase::kDown ||
        sample.phase == input::PointerPhase::kCancel)) {
      if (!flushInk()) return false;
    }
    auto routing = coordinator_->route(sample);
    if ((!routing.cancelPointers.empty() || routing.cancelsInk ||
         !routing.routesToInk) && !flushInk()) return false;
    if (routing.endedViewport) viewportController_->endGesture();
    // Preserve the original AutoIntent handoff boundary: once Runtime claims
    // a viewport gesture, every pending ink session participating in that
    // claim must be cancelled before any further sample can reach BrushSession.
    for (const auto& cancelled : routing.cancelPointers) {
      auto cancelSample = sample;
      cancelSample.key = cancelled;
      cancelSample.phase = input::PointerPhase::kCancel;
      (void)dispatchRuntime_.cancel(cancelSample);
    }
    if (routing.cancelsInk && sample.phase != input::PointerPhase::kUp) {
      (void)dispatchRuntime_.cancel(sample);
      continue;
    }
    if (sample.phase == input::PointerPhase::kDown) {
      baselineDownTimestamps_[sample.key] = sample.timestampNs;
    }
    const auto downIt = baselineDownTimestamps_.find(sample.key);
    const auto downTimestampNs = downIt == baselineDownTimestamps_.end()
        ? 0U : downIt->second;
    const auto downTimeDeltaNs = downTimestampNs != 0U &&
            sample.timestampNs >= downTimestampNs
        ? sample.timestampNs - downTimestampNs : 0U;
    if (coordinator_->viewportClaimed()) {
      const auto viewportSamples = coordinator_->viewportSamples();
      if (viewportSamples.size() >= 2U) {
        (void)viewportController_->updateGesture(viewportSamples[0], viewportSamples[1]);
      }
    }
    if (sample.key.valid()) {
      const auto content = viewportController_->viewToContent(sample.x, sample.y);
      routing.routedSample.x = content.first;
      routing.routedSample.y = content.second;
    }
    baselineTrace_.push_back({sample.sequence, sample.timestampNs, sample.x, sample.y,
                              routing.routedSample.x, routing.routedSample.y,
                              sample.pressure, sample.key, sample.phase,
                              routing.disposition, routing.viewportClaimed,
                              downTimestampNs, downTimeDeltaNs});
    baselineViewport_.push_back({sample.sequence, coordinator_->viewportClaimed(),
                                 viewportController_->state().scale,
                                 viewportController_->state().translationX,
                                 viewportController_->state().translationY});
    if (routing.routesToInk) {
      inkBatch.push_back(routing.routedSample);
      previewDirty = true;
    }
    if (sample.phase == input::PointerPhase::kUp ||
        sample.phase == input::PointerPhase::kCancel) {
      if (!flushInk()) return false;
      baselineDownTimestamps_.erase(sample.key);
    }
  }
  if (!flushInk()) return false;
  const auto viewportAfter = viewportController_->state();
  const bool viewportChanged = viewportBefore.scale != viewportAfter.scale ||
      viewportBefore.translationX != viewportAfter.translationX ||
      viewportBefore.translationY != viewportAfter.translationY;
  if (viewportChanged && previewController_ != nullptr && previewController_->active()) {
    if (!previewController_->updateViewport(viewportAfter.scale,
                                             viewportAfter.translationX,
                                             viewportAfter.translationY)) {
      return false;
    }
  }
  if (viewportChanged && activeSurfaceProvider() != nullptr && surface_.available) {
    // A viewport-only frame redraws the canonical document but is not the
    // canonical-visible receipt for the active brush commit.  Retained amber
    // preview must survive this redraw until the matching commit frame.
    if (platformPresentationDeferred_) {
      // Windows consumes this invalidation from its render pump. Keeping it
      // in Runtime makes the scheduling decision explicit without doing a
      // fence wait from the input callback.
      canonicalPresentationDirty_ = true;
    } else if (!presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false)) {
      return false;
    }
  }
  // Preview submission is display-frame gated by the platform host. The
  // batch only updates Runtime-retained geometry; Java/Choreographer calls
  // presentBrushPreview() once for the latest dirty revision.
  (void)previewDirty;
  if (routed.terminalCancel) cancelAllPointers();
  return true;
}

bool CanvasEditingRuntime::cancelPlatformPointer(const input::PointerKey& key,
                                                  std::uint64_t sequence,
                                                  std::uint64_t timestampNs) noexcept {
  if (!key.valid() || sequence == 0U) return false;
  if (appBinding_ == nullptr) return false;
  const auto result = appBinding_->cancelPointer(key, sequence, timestampNs);
  if (!result.accepted) return false;
  onSessionCancel(result);
  return true;
}

bool CanvasEditingRuntime::selectCreationPreset(CreationPreset preset) noexcept {
  if (closed_ || !brushSessions_.empty() || !eraserTraces_.empty() || !keyedStrokeIds_.empty() ||
      selectionMode_ || panTool_ || creationActive()) return false;
  creationPreset_ = preset;
  creationMode_ = true;
  return true;
}

bool CanvasEditingRuntime::creationPointer(const input::PointerKey& key, input::PointerPhase phase,
                                        float x, float y) noexcept {
  if (closed_ || std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  if (!key.valid() || !std::isfinite(x) || !std::isfinite(y) || surface_.width == 0U ||
      surface_.height == 0U) return false;
  const auto content = viewportController_->viewToContent(x, y);
  const auto viewportTopLeft = viewportController_->viewToContent(0.0F, 0.0F);
  const auto viewportBottomRight = viewportController_->viewToContent(
      surface_.logicalWidth, surface_.logicalHeight);
  const foundation::WorldRect viewport{
      (std::min)(viewportTopLeft.first, viewportBottomRight.first),
      (std::min)(viewportTopLeft.second, viewportBottomRight.second),
      (std::max)(viewportTopLeft.first, viewportBottomRight.first),
      (std::max)(viewportTopLeft.second, viewportBottomRight.second)};
  const auto eraseSession = [&] {
    creations_.erase(key);
    creationFences_.erase(key);
  };
  const auto refreshTargets = [&](CreationSession& creation) {
    if (!historySceneReady()) return false;
    const auto queried = runtimeSceneHost_->query(canvas::SceneQuery{viewport});
    if (!queried) return false;
    std::vector<interaction::SnapTarget> targets;
    const auto view = runtimeSceneHost_->read();
    for (const auto id : queried.value().backToFront) {
      const auto* record = view.find(id);
      if (record == nullptr ||
          (record->kind != canvas::SceneObjectKind::kShape &&
           record->kind != canvas::SceneObjectKind::kImage &&
           record->kind != canvas::SceneObjectKind::kSticky) ||
          std::find(selection_.selected().begin(), selection_.selected().end(), id) !=
              selection_.selected().end()) continue;
      targets.push_back({id, record->worldBounds, false, false});
    }
    creation.setSnapTargets(std::move(targets));
    creation.setSnapScale(viewportController_->state().scale);
    creationFences_[key] = {semanticGeneration_.current(), cameraGeneration()};
    return true;
  };
  if (phase == input::PointerPhase::kDown) {
    if (creations_.contains(key)) return false;
    auto& creation = creations_[key];
    latestCreationPointer_ = key;
    if (!refreshTargets(creation)) { eraseSession(); return false; }
    const auto seed = ++creationSerial_ + (std::uint64_t{1} << 48U);
    if (!creation.begin(key, creationPreset_, CreationStyle{appearanceProperties(futureAppearance_,
                        creationPreset_<=CreationPreset::kTriangle)},
                        {content.first, content.second}, viewport, seed)) { eraseSession(); return false; }
  canonicalPresentationDirty_ = true;
    if (platformPresentationDeferred_) transientOverlayDirty_ = true;
    else (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    return true;
  }
  const auto session = creations_.find(key);
  if (session == creations_.end()) return true;
  auto& creation = session->second;
  latestCreationPointer_ = key;
  canonicalPresentationDirty_ = true;
  if (phase == input::PointerPhase::kMove) {
    if (!refreshTargets(creation) || !creation.move(key, {content.first, content.second})) eraseSession();
    // The transient object and snap guides are part of the next display
    // frame, so make the owner host repaint immediately after every move.
    canonicalPresentationDirty_ = true;
    if (platformPresentationDeferred_) transientOverlayDirty_ = true;
    else (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    return true;
  }
  if (phase == input::PointerPhase::kCancel) {
    eraseSession();
    (void)clearTransientOverlay();
    return true;
  }
  if (phase != input::PointerPhase::kUp) return false;
  // Never silently re-snap at Up after the last visible preview became stale.
  const auto fence = creationFences_.find(key);
  if (fence == creationFences_.end() || fence->second.semantic != semanticGeneration_.current() ||
      fence->second.camera != cameraGeneration()) { eraseSession(); return false; }
  if (!creation.move(key, {content.first, content.second})) { eraseSession(); return true; }
  const auto commit = creation.finish(key);
  eraseSession();
  // Keep the last transient shape/guide visible until the canonical frame
  // containing the committed object is accepted. Hiding it here exposes the
  // previous canonical frame for one compositor refresh, which is the
  // pointer-up flash seen on Windows.
  transientOverlayDirty_ = false;
  transientOverlayHandoffPending_ = true;
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(true);
  if (!commit.has_value()) {
    return true;
  }
  const auto ordinal = localOperationOrdinal();
  if (ordinal == 0U) return false;
  semantic::Operation operation{
      semantic::OperationId(foundation::ObjectId::fromUint64(ordinal)), documentId_,
      1U, 1U, semantic::InsertObjectsOp{{commit->record}}};
  const auto inverseId = allocateOperationId();
  if (inverseId.isZero()) return false;
  const bool applied = applyLocalHistoryOperation(std::move(operation),
      {inverseId, documentId_, 1U, 1U, semantic::DeleteObjectsOp{{commit->record.id}}});
  // Keep the last transient creation frame visible until the canonical frame
  // containing the committed object is accepted. Clearing here exposes one
  // blank/old frame between pointer-up and the render-pump handoff.
  return applied;
}

bool CanvasEditingRuntime::commitStroke(const input::PointerKey& key, std::uint64_t strokeId,
                                     std::uint64_t operationId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  if (!keyedStrokeIds_.contains(key)) return false;
  const auto* state = preview_->snapshot(strokeId);
  const auto finished = ink_->finish(key);
  if (state == nullptr || !finished.has_value()) return false;
  const auto committedPoints = state->confirmed;
  const auto operation = legacyStrokeOperation(*finished, operationId);
  if (!interaction_->commit(key, strokeId, interaction::OperationRequest{operationId, operation})) return false;
  if (!committedPoints.empty()) committedStrokes_.push_back(committedPoints);
  preview_->cancelKeyed(strokeId);
  keyedStrokeIds_.erase(key);
  return true;
}

bool CanvasEditingRuntime::cancelStroke(const input::PointerKey& key) noexcept {
  const auto entry = keyedStrokeIds_.find(key);
  if (entry == keyedStrokeIds_.end()) return false;
  ink_->cancel(key);
  preview_->cancelKeyed(entry->second);
  if (!interaction_->cancel(key, entry->second)) return false;
  keyedStrokeIds_.erase(entry);
  return true;
}

void CanvasEditingRuntime::onSessionCancel(
    const input::PlatformInteractionIngressResult& result) noexcept {
  for (const auto& sample : result.normalized.samples) {
    if (!sample.key.valid()) continue;
    touchTapContacts_.erase(sample.key);
    previousTouchTap_.reset();
    if (touchTapContacts_.empty()) touchTapMultiContact_ = false;
    const auto routing = coordinator_->route(sample);
    if (routing.endedViewport) viewportController_->endGesture();
    baselineDownTimestamps_.erase(sample.key);
    (void)cancelStroke(sample.key);
    (void)dispatchRuntime_.cancel(sample);
    panPointers_.erase(sample.key);
    creations_.erase(sample.key);
    creationFences_.erase(sample.key);
    if (creations_.empty() && !selectionTransformActive()) (void)clearTransientOverlay();
    canonicalPresentationDirty_ = true;
  }
}

void CanvasEditingRuntime::cancelAllPointers() noexcept {
  textPointerKeys_.clear();
  resetTouchTextTap();
  shapeExpandPointerKeys_.clear();
  creations_.clear();
  creationFences_.clear();
  cancelSelectionTransform();
  for (const auto& [key, strokeId] : keyedStrokeIds_) {
    ink_->cancel(key);
    preview_->cancelKeyed(strokeId);
  }
  keyedStrokeIds_.clear();
  coordinator_->reset();
  viewportController_->reset();
  interaction_->cancelKeyedSessions(interaction::CancellationReason::kSourceLost);
  dispatchRuntime_.cancelAll();
  for (auto& [pointer, session] : brushSessions_) {
    (void)pointer;
    session->cancel();
  }
  brushSessions_.clear();
  brushSessionPackages_.clear();
  brushSessionSeeds_.clear();
  brushPreviews_.clear();
  panPointers_.clear();
  pendingCanonicalIdentities_.clear();
  eraserTraces_.clear();
  eraserPreviewRevisions_.clear();
}

bool CanvasEditingRuntime::beginBrushSession(std::uint64_t pointerId,
                                          std::uint32_t profile) noexcept {
  if (closed_ || std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  const auto profileId = profile == 1U ? "vector-solid-v1"
      : profile == 2U ? "marker-flat-v1"
      : profile == 3U ? "chalk-grain-v1" : profile == 4U ? "membrane-v1" : "";
  if (*profileId == '\0') return false;
  const auto loaded = brushCatalog_.loadDefault(profileId,
      profileId == selectedBrushProfile_ ? selectedBrushRevision_ : 1U);
  if (!loaded) return false;
  return beginBrushSession(pointerId, loaded.package, 0x4500ULL + pointerId);
}

bool CanvasEditingRuntime::beginBrushSession(std::uint64_t pointerId,
                                          const ink::BrushPackage& package,
                                          std::uint64_t seed) noexcept {
  if (closed_ || std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  std::lock_guard lock(previewStateMutex_);
  if (pointerId == 0U ||
      (package.profileId != "vector-solid-v1" && package.profileId != "marker-flat-v1" &&
       package.profileId != "chalk-grain-v1" && package.profileId != "membrane-v1") ||
      package.packageId.empty() ||
      brushSessions_.contains(pointerId)) return false;
  auto capturedPackage = package;
  capturedPackage.vector.size = inkSize_;
  capturedPackage.paint.red = inkRed_;
  capturedPackage.paint.green = inkGreen_;
  capturedPackage.paint.blue = inkBlue_;
  capturedPackage.paint.alpha = inkAlpha_;
  capturedPackage.paint.opacity = inkOpacity_;
  auto state = ink::resolveBrushState(capturedPackage, seed);
  brushPackageDigest_ = ink::brushPackageCanonicalDigest(package);
  brushResolvedStateDigest_ = kFnvOffset;
  brushResolvedStateDigest_ = hashBytes(brushResolvedStateDigest_, package.packageId.data(),
                                        package.packageId.size());
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, package.revision);
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.size));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.thinning));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.smoothing));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, quantize(package.vector.streamline));
  brushResolvedStateDigest_ = hashValue(brushResolvedStateDigest_, state.seed);
  auto session = std::make_unique<ink::BrushSession>(pointerId, std::move(state));
  if (!session->begin()) return false;
  brushSessions_.emplace(pointerId, std::move(session));
  brushSessionPackages_[pointerId] = capturedPackage;
  brushSessionSeeds_[pointerId] = seed;
  brushPreviews_.erase(pointerId);
  // Partial eraser uses a white presentation-only override. A subsequent
  // brush session must start with the normal transient tint; otherwise the
  // preview surface is submitted but visually indistinguishable from the
  // canonical stroke unless the host supplies a presentation tint.
  if (previewController_ != nullptr) {
    render::PreviewStyleOverride style;
    style.red = 1.0F;
    style.green = 0.62F;
    style.blue = 0.04F;
    style.alpha = 0.92F;
    style.opacityMultiplier = 0.92F;
    style.overrideColor = true;
    style.overrideOpacity = true;
    style.preferCapturedPaint = false;
    previewController_->setPresentationStyle(style);
  }
  if (arcPreviewSink_ != nullptr) {
    ink::BrushPreviewDelta initial;
    const auto disposition = arcPreviewSink_->begin(
        ink::PreviewIdentity{1U, pointerId, 1U}, initial);
    if (disposition == ink::PreviewSubmitResult::kRejected) {
      brushSessions_.erase(pointerId);
      brushSessionPackages_.erase(pointerId);
      brushSessionSeeds_.erase(pointerId);
      return false;
    }
  }
  if (previewProvider_ == nullptr || previewController_ == nullptr ||
      !previewController_->begin(1U, pointerId, 1U, previewProvider_->generation())) {
    if (arcPreviewSink_ != nullptr)
      (void)arcPreviewSink_->cancel(ink::PreviewIdentity{1U, pointerId, 1U});
    brushSessions_.erase(pointerId);
    brushSessionPackages_.erase(pointerId);
    brushSessionSeeds_.erase(pointerId);
    brushPreviews_.erase(pointerId);
    return false;
  }
  return true;
}

bool CanvasEditingRuntime::selectTool(ToolMode mode) noexcept {
  if (closed_ || !brushSessions_.empty()) return false;
  dispatchRuntime_.cancelAll();
  toolMode_ = mode;
  panTool_ = false;
  creationMode_ = false;
  creations_.clear();
  creationFences_.clear();
  return true;
}

bool CanvasEditingRuntime::setPanTool(bool enabled) noexcept {
  if (!brushSessions_.empty() || !eraserTraces_.empty() || !keyedStrokeIds_.empty()) return false;
  dispatchRuntime_.cancelAll();
  panTool_ = enabled;
  creationMode_ = false;
  creations_.clear();
  creationFences_.clear();
  if (enabled) {
    toolMode_ = ToolMode::kBrush;
    if (selectionMode_) (void)setSelectionMode(false);
  }
  return true;
}

bool CanvasEditingRuntime::setSelectionMode(bool enabled) noexcept {
  if (!brushSessions_.empty() || !eraserTraces_.empty() || !keyedStrokeIds_.empty()) {
    return false;
  }
  if (selectionMode_ != enabled) {
    dispatchRuntime_.cancelAll();
    resetTouchTextTap();
  }
  selectionMode_ = enabled;
  if (enabled) {
    creationMode_ = false;
    creations_.clear();
    creationFences_.clear();
  }
  if (!enabled) { cancelSelectionTransform(); selection_.cancel(); selectionView_.reset(); }
  canonicalPresentationDirty_ = true;
  return true;
}

bool CanvasEditingRuntime::selectAtViewPoint(float x, float y) noexcept {
  if (!selectionMode_ || runtimeSceneHost_ == nullptr ||
      !std::isfinite(x) || !std::isfinite(y)) return false;
  const auto content = viewportController_->viewToContent(x, y);
  const auto tested = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
      foundation::WorldPoint{content.first, content.second}, 4.0F,
      canvas::HitTestFilter{canvas::HitTestKindMask::kAll, true}, 1U});
  if (!tested || tested.value().frontToBack.empty()) {
    selection_.cancel();
    selectionView_.reset();
    snapGuides_.clear();
    canonicalPresentationDirty_ = true;
    return true;
  }
  const bool selected = selection_.click(tested.value().frontToBack.front());
  if (selected) {
    snapGuides_.clear();
    canonicalPresentationDirty_ = true;
  }
  return selected && renderSelectionOverlay();
}

void CanvasEditingRuntime::observeApplied(const semantic::Operation& operation,
                                           const semantic::ApplyResult& result) noexcept {
  if (result.disposition != semantic::ApplyDisposition::kApplied || !result.commit_record) return;
  lastAppliedCommit_ = *result.commit_record;
  if (localOperationSink_ != nullptr &&
      semantic::localBridgePublicationDisposition(*result.commit_record) ==
          semantic::LocalBridgePublicationDisposition::kEligible) {
    if (publicationCallbackActive_) return;
    publicationCallbackActive_ = true;
    localOperationSink_->applied(operation, *result.commit_record);
    publicationCallbackActive_ = false;
  }
}

semantic::ApplyResult CanvasEditingRuntime::applyExternalOperation(
    const semantic::Operation& operation, semantic::ApplySource source) {
  if (closed_ || !surface_.available || std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return {};
  if ((source != semantic::ApplySource::kRemoteSync && source != semantic::ApplySource::kRestoreReplay) ||
      operation.document_id != documentId_) return {};
  const auto result = operationEngine_.apply(operation, source, semanticObjects_,
      *resourceManifestPort_, appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (result.disposition == semantic::ApplyDisposition::kApplied && result.commit_record) {
    const semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current(), resourceManifestPort_);
    const canvas::SceneCommitInput input(result.commit_record->before_generation,
        result.commit_record->after_generation, view, &result.commit_record->change_set);
    (void)sceneCoordinator_->apply(*sceneCompiler_, input);
    observeApplied(operation, result);
    if (operation.kind() == semantic::OperationKind::kUpdateResourceManifest) {
      refreshResourceManifestServices();
    }
    canonicalPresentationDirty_ = true;
  }
  return result;
}

bool CanvasEditingRuntime::canonicalWriteAdmission() const noexcept {
  return !closed_ && !surfaceTransitionActive_ && surface_.available &&
         std::this_thread::get_id() == ownerThread_ && !publicationCallbackActive_;
}

bool CanvasEditingRuntime::textReady() const noexcept {
  return canonicalWriteAdmission() && historySceneReady() && brushSessions_.empty() && eraserTraces_.empty() && keyedStrokeIds_.empty() &&
         creations_.empty() && panPointers_.empty() && dispatchRuntime_.activeCount()==0 && !selectionTransformActive();
}
bool CanvasEditingRuntime::textWritable(semantic::ObjectId id) const noexcept {
  const auto* current=semanticObjects_.find(id);
  for(std::size_t count=0;current && count<=semanticObjects_.size();++count) {
    for(const auto& entry:current->properties.entries) {
      if(entry.field_id==2 && std::holds_alternative<bool>(entry.value) && std::get<bool>(entry.value))return false;
      if(entry.field_id==1 && std::holds_alternative<bool>(entry.value) && !std::get<bool>(entry.value))return false;
    }
    if(!current->placement.parent_id)return true;
    current=semanticObjects_.find(*current->placement.parent_id);
  }
  return false;
}
semantic::ObjectId CanvasEditingRuntime::allocateTextIdentity() {
  while(nextTextIdentity_!=0) {
    auto id=foundation::ObjectId::fromUint64(nextTextIdentity_++);id.bytes[15]=0xF2;
    bool found=semanticObjects_.contains(id);
    for(const auto& object:semanticObjects_.allObjects())if(const auto* rich=std::get_if<semantic::RichTextContent>(&object.content))
      for(const auto& paragraph:rich->document.paragraphs)found=found || paragraph.id==id;
    if(!found)return id;
  }return {};
}
semantic::PrepareResult CanvasEditingRuntime::prepareTextOperation(const semantic::Operation& operation) const {
  return operationEngine_.prepare(operation,{semanticObjects_,appliedOperations_,resourceManifestPort_});
}

bool CanvasEditingRuntime::applyResourceManifestOperation(const semantic::Operation& operation) noexcept {
  if (!canonicalWriteAdmission() || !resourceManifestPort_ ||
      operation.kind() != semantic::OperationKind::kUpdateResourceManifest) return false;
  const auto applied = operationEngine_.apply(operation, semantic::ApplySource::kLocalCommand,
      semanticObjects_, *resourceManifestPort_, appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied) return false;
  if (applied.commit_record && sceneCoordinator_ && sceneCompiler_) {
    const semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current(), resourceManifestPort_);
    const canvas::SceneCommitInput input(applied.commit_record->before_generation,
        applied.commit_record->after_generation, view, &applied.commit_record->change_set);
    (void)sceneCoordinator_->apply(*sceneCompiler_, input);
  }
  observeApplied(operation, applied);
  ++submittedOperationCount_;
  refreshResourceManifestServices();
  canonicalPresentationDirty_ = true;
  return true;
}

bool CanvasEditingRuntime::applyLocalHistoryOperation(
    semantic::Operation forward, semantic::Operation inverse) noexcept {
  if (!canonicalWriteAdmission()) return false;
  if (!historySceneReady() || forward.id.isZero() || inverse.id.isZero()) return false;
  const auto prepared=operationEngine_.prepare(forward,{semanticObjects_,appliedOperations_,resourceManifestPort_});
  if(prepared.disposition!=semantic::PrepareDisposition::kPrepared || !prepared.plan) return false;
  interaction::HistoryAspect aspect=interaction::HistoryAspect::kWholeRecord;
  if(forward.kind()==semantic::OperationKind::kSetTransforms) aspect=interaction::HistoryAspect::kTransform;
  if(forward.kind()==semantic::OperationKind::kSetObjectSize) aspect=interaction::HistoryAspect::kSize;
  if(forward.kind()==semantic::OperationKind::kSetPlacements) aspect=interaction::HistoryAspect::kPlacement;
  if(forward.kind()==semantic::OperationKind::kPatchProperties) aspect=interaction::HistoryAspect::kProperties;
  if(forward.kind()==semantic::OperationKind::kEditRichText) aspect=interaction::HistoryAspect::kRichTextContent;
  std::vector<interaction::HistoryExpectation> before,after;
  const auto capture=[&](semantic::ObjectId id,const semantic::ObjectRecord* post) {
    const auto* prior=semanticObjects_.find(id);
    std::vector<std::uint32_t> fields;
    if(const auto* patches=std::get_if<semantic::PatchPropertiesOp>(&forward.payload))
      for(const auto& patch:patches->patches)if(patch.object_id==id)fields.push_back(patch.field_id);
    before.push_back({id,prior?std::optional<semantic::ObjectRecord>(*prior):std::nullopt,aspect,fields});
    after.push_back({id,post?std::optional<semantic::ObjectRecord>(*post):std::nullopt,aspect,fields});
  };
  for(const auto& object:prepared.plan->creates)capture(object.id,&object);
  for(const auto& object:prepared.plan->replacements)capture(object.id,&object);
  for(const auto id:prepared.plan->deletes)capture(id,nullptr);
  auto historyEntry=history_.prepareRecord(forward,{inverse},std::move(before),std::move(after));
  if(!historyEntry) return false;
  const auto applied = resourceManifestPort_
      ? operationEngine_.apply(forward, semantic::ApplySource::kLocalInteraction, semanticObjects_, *resourceManifestPort_, appliedOperations_, semanticGeneration_, canonicalCommitClock_)
      : operationEngine_.apply(forward, semantic::ApplySource::kLocalInteraction, semanticObjects_, appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied) return false;
  history_.commitPrepared(std::move(*historyEntry));
  if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr &&
      sceneCompiler_ != nullptr) {
    const semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current(), resourceManifestPort_);
    const canvas::SceneCommitInput input(applied.commit_record->before_generation,
        applied.commit_record->after_generation, view, &applied.commit_record->change_set);
    (void)sceneCoordinator_->apply(*sceneCompiler_, input);
  }
  observeApplied(forward, applied);
  ++submittedOperationCount_;
  canonicalPresentationDirty_ = true;
  return true;
}

bool CanvasEditingRuntime::applyTextOperation(semantic::Operation forward,
                                               semantic::Operation inverse) {
  const auto* edit = std::get_if<semantic::EditRichTextOp>(&forward.payload);
  const bool isTextEdit = edit != nullptr;
  const auto target = edit ? edit->object_id : semantic::ObjectId{};
  if (!applyLocalHistoryOperation(std::move(forward), std::move(inverse))) return false;
  if (!isTextEdit) return true;
  const auto* updated = semanticObjects_.find(target);
  const auto* shape = updated ? std::get_if<semantic::ShapeContent>(&updated->content) : nullptr;
  bool hasText = false;
  if (shape && shape->label)
    for (const auto& paragraph : shape->label->document.paragraphs)
      for (const auto& run : paragraph.runs) hasText = hasText || !run.text.empty();
  if (hasText) (void)expandShapeLabel(target);
  return true;
}

bool CanvasEditingRuntime::setSelectedObjectSize(double width, double height) noexcept {
  if (selection_.primary().isZero() || !std::isfinite(width) || !std::isfinite(height) ||
      width <= 0.0 || height <= 0.0) return false;
  const auto* before = semanticObjects_.find(selection_.primary());
  if (before == nullptr) return false;
  for (const auto& entry : before->properties.entries)
    if (entry.field_id == 0x00000002U && std::get_if<bool>(&entry.value) != nullptr &&
        std::get<bool>(entry.value)) return false;
  const auto* shape = std::get_if<semantic::ShapeContent>(&before->content);
  if (shape == nullptr) return false;
  const auto forwardId = allocateOperationId(), inverseId = allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return false;
  return applyLocalHistoryOperation(
      {forwardId, documentId_, 1U, 1U,
       semantic::SetObjectSizeOp{{{before->id, width, height}}}},
      {inverseId, documentId_, 1U, 1U,
       semantic::SetObjectSizeOp{{{before->id, shape->width, shape->height}}}});
}

std::optional<semantic::Transform2D> CanvasEditingRuntime::fittedShapeLabelTransform(
    const semantic::ObjectRecord& record) {
  const auto* shape = std::get_if<semantic::ShapeContent>(&record.content);
  if (!textLayoutService_ || !shape || shape->shape_kind < 1U || shape->shape_kind > 3U)
    return {};
  const auto content = shape->label.value_or(semantic::RichTextContent{});
  bool hasText = false;
  for (const auto& paragraph : content.document.paragraphs)
    for (const auto& run : paragraph.runs) hasText = hasText || !run.text.empty();
  if (!hasText) return record.transform;
  const auto layout = textLayoutService_->resolveShapeNatural(record.id, content, *shape,
                                                               record.transform);
  if (!layout || !layout->diagnostics.empty()) return {};
  const float sy = std::max(0.001F, std::hypot(static_cast<float>(record.transform.c),
                                                static_cast<float>(record.transform.d)));
  const double currentWorldHeight = shape->height * sy;
  const double availableTextHeight = layout->localBounds.bottom - layout->labelOffsetY;
  if (!std::isfinite(availableTextHeight) || availableTextHeight <= 0.0) return {};
  const double verticalInset = shape->shape_kind == 2U ? 0.70710678 :
                              shape->shape_kind == 3U ? 0.5 : 1.0;
  constexpr double padding = 16.0;
  const double requiredWorldHeight =
      (availableTextHeight + padding) / verticalInset;
  if (requiredWorldHeight <= currentWorldHeight + 0.5) return record.transform;
  const double scaleY = requiredWorldHeight / currentWorldHeight;
  auto transform = record.transform;
  transform.c *= scaleY; transform.d *= scaleY;
  // Keep the bottom edge anchored; extending the local Y basis moves only
  // the top edge toward negative canvas Y.
  transform.tx += (record.transform.c - transform.c) * shape->height;
  transform.ty += (record.transform.d - transform.d) * shape->height;
  return transform;
}

bool CanvasEditingRuntime::expandShapeLabel(semantic::ObjectId id) {
  if (!canonicalWriteAdmission() || !textWritable(id)) return false;
  const auto* before = semanticObjects_.find(id);
  if (!before) return false;
  const auto transform = fittedShapeLabelTransform(*before);
  if (!transform) return false;
  if (*transform == before->transform) return true;
  const auto forwardId = allocateOperationId(), inverseId = allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return false;
  return applyLocalHistoryOperation(
      {forwardId, documentId_, 1U, 1U, semantic::SetTransformsOp{{{id, *transform}}}},
      {inverseId, documentId_, 1U, 1U, semantic::SetTransformsOp{{{id, before->transform}}}});
}

bool CanvasEditingRuntime::patchSelectedProperty(std::uint32_t fieldId,
                                               semantic::PropertyValue value) noexcept {
  if (selection_.primary().isZero() ||
      (selectedObjectLocked() && !(fieldId == 2U && std::get_if<bool>(&value) && !std::get<bool>(value)))) return false;
  const auto* before = semanticObjects_.find(selection_.primary());
  if (before == nullptr) return false;
  const auto it = std::find_if(before->properties.entries.begin(), before->properties.entries.end(),
      [fieldId](const auto& entry) { return entry.field_id == fieldId; });
  const auto forwardId = allocateOperationId(), inverseId = allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return false;
  semantic::PropertyPatch inversePatch{before->id, fieldId,
      it == before->properties.entries.end() ? semantic::PropertyPatchAction::kClear
                                              : semantic::PropertyPatchAction::kSet,
      it == before->properties.entries.end() ? std::variant<std::monostate, semantic::PropertyValue>{}
                                             : std::variant<std::monostate, semantic::PropertyValue>{it->value}};
  return applyLocalHistoryOperation(
      {forwardId, documentId_, 1U, 1U,
       semantic::PatchPropertiesOp{{{before->id, fieldId, semantic::PropertyPatchAction::kSet, value}}}},
      {inverseId, documentId_, 1U, 1U, semantic::PatchPropertiesOp{{inversePatch}}});
}

bool CanvasEditingRuntime::unlockSelectedObjects() noexcept {
  return patchSelectedProperty(0x00000002U, semantic::PropertyValue{false});
}

bool CanvasEditingRuntime::setFutureAppearance(const runtime::CanvasAppearance& appearance) noexcept {
  if(!validAppearance(appearance))return false;
  futureAppearance_=appearance; return true;
}
bool CanvasEditingRuntime::setSelectedAppearance(const runtime::CanvasAppearance& appearance) noexcept {
  const auto* before=semanticObjects_.find(selection_.primary());
  if(!before || selectedObjectLocked() || !validAppearance(appearance) ||
      (before->kind!=semantic::ObjectKind::kShape && before->kind!=semantic::ObjectKind::kConnector))return false;
  semantic::PatchPropertiesOp forward,inverse;
  auto desiredProperties = appearanceProperties(appearance,before->kind==semantic::ObjectKind::kShape);
  for (auto& entry : desiredProperties.entries) {
    if (entry.field_id == 0x101U && appearance.strokeEnabled) {
      const auto current = std::find_if(before->properties.entries.begin(),
          before->properties.entries.end(), [](const auto& item) { return item.field_id == 0x101U; });
      if (current != before->properties.entries.end()) {
        const auto* currentStyle = std::get_if<semantic::StrokeStyleValue>(&current->value);
        if (currentStyle != nullptr && std::holds_alternative<semantic::SolidStroke>(*currentStyle)) {
          auto merged = std::get<semantic::SolidStroke>(std::get<semantic::StrokeStyleValue>(entry.value));
          const auto& preserved = std::get<semantic::SolidStroke>(*currentStyle);
          merged.cap = preserved.cap;
          merged.join = preserved.join;
          merged.dash = preserved.dash;
          entry.value = semantic::StrokeStyleValue{std::move(merged)};
        }
      }
    }
    forward.patches.push_back({before->id,entry.field_id,semantic::PropertyPatchAction::kSet,entry.value});
    const auto prior=std::find_if(before->properties.entries.begin(),before->properties.entries.end(),
        [&](const auto& item){return item.field_id==entry.field_id;});
    inverse.patches.push_back({before->id,entry.field_id,
      prior==before->properties.entries.end()?semantic::PropertyPatchAction::kClear:semantic::PropertyPatchAction::kSet,
      prior==before->properties.entries.end()?std::variant<std::monostate,semantic::PropertyValue>{}:
                                             std::variant<std::monostate,semantic::PropertyValue>{prior->value}});
  }
  const auto f=allocateOperationId(),i=allocateOperationId();
  return applyLocalHistoryOperation({f,documentId_,1,1,std::move(forward)},
      {i,documentId_,1,1,std::move(inverse)});
}
bool CanvasEditingRuntime::setSelectedStrokeWidth(double width) noexcept {
  if (!std::isfinite(width) || width <= 0.0 || selection_.primary().isZero() || selectedObjectLocked()) return false;
  const auto* before = semanticObjects_.find(selection_.primary());
  if (before == nullptr) return false;
  const auto it = std::find_if(before->properties.entries.begin(), before->properties.entries.end(),
      [](const auto& entry) { return entry.field_id == 0x101U; });
  if (it == before->properties.entries.end()) return false;
  const auto* style = std::get_if<semantic::StrokeStyleValue>(&it->value);
  if (style == nullptr || !std::holds_alternative<semantic::SolidStroke>(*style)) return false;
  auto patched = std::get<semantic::SolidStroke>(*style);
  if (patched.width == width) return true;
  const auto forwardId = allocateOperationId(), inverseId = allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return false;
  patched.width = width;
  semantic::PropertyPatch forwardPatch{before->id, 0x101U,
      semantic::PropertyPatchAction::kSet,
      semantic::PropertyValue{semantic::StrokeStyleValue{patched}}};
  semantic::PropertyPatch inversePatch{before->id, 0x101U,
      semantic::PropertyPatchAction::kSet, it->value};
  semantic::PatchPropertiesOp forwardPayload;
  forwardPayload.patches.push_back(std::move(forwardPatch));
  semantic::PatchPropertiesOp inversePayload;
  inversePayload.patches.push_back(std::move(inversePatch));
  return applyLocalHistoryOperation(
      {forwardId, documentId_, 1U, 1U, std::move(forwardPayload)},
      {inverseId, documentId_, 1U, 1U, std::move(inversePayload)});
}
bool CanvasEditingRuntime::selectedObjectLocked() const noexcept {
  const auto* object=semanticObjects_.find(selection_.primary());
  return object && propertyBool(object->properties,2U,false);
}
std::optional<semantic::ObjectRecord> CanvasEditingRuntime::selectedObjectRecord() const {
  const auto* object=semanticObjects_.find(selection_.primary());
  return object ? std::optional<semantic::ObjectRecord>(*object) : std::nullopt;
}
bool CanvasEditingRuntime::setSelectedTransform(const semantic::Transform2D& transform) noexcept {
  const auto* before=semanticObjects_.find(selection_.primary());
  if(!before || selectedObjectLocked()) return false;
  const auto forward=allocateOperationId(),inverse=allocateOperationId();
  return applyLocalHistoryOperation({forward,documentId_,1,1,semantic::SetTransformsOp{{{before->id,transform}}}},
      {inverse,documentId_,1,1,semantic::SetTransformsOp{{{before->id,before->transform}}}});
}
bool CanvasEditingRuntime::setSelectedOrder(bool front) noexcept {
  const auto* before=semanticObjects_.find(selection_.primary());
  if(!before || selectedObjectLocked()) return false;
  auto siblings=semanticObjects_.children(before->placement.parent_id);
  if(siblings.size()<2) return true;
  auto placement=before->placement;
  if(front) {
    const auto bytes=siblings.back().placement.order_key.bytes();
    std::vector<std::uint8_t> key(bytes.begin(),bytes.end());
    if(key.size()>=semantic::OrderKey::kMaxSize) return false;
    key.push_back(128); placement.order_key=semantic::OrderKey(std::move(key));
  } else {
    const auto bytes=siblings.front().placement.order_key.bytes();
    std::vector<std::uint8_t> key;
    bool bounded=false;
    for(const auto byte:bytes) {
      if(byte>0) {
        key.push_back(static_cast<std::uint8_t>(byte/2));
        if(key.back()==0) key.push_back(128);
        bounded=true;
        break;
      }
      key.push_back(0);
    }
    if(!bounded || key.size()>semantic::OrderKey::kMaxSize) return false;
    placement.order_key=semantic::OrderKey(std::move(key));
  }
  const auto forward=allocateOperationId(),inverse=allocateOperationId();
  return applyLocalHistoryOperation({forward,documentId_,1,1,semantic::SetPlacementsOp{{{before->id,placement}}}},
      {inverse,documentId_,1,1,semantic::SetPlacementsOp{{{before->id,before->placement}}}});
}
bool CanvasEditingRuntime::deleteSelectedObjects() noexcept {
  if(selection_.primary().isZero() || selectedObjectLocked()) return false;
  const auto forwardId=allocateOperationId(),inverseId=allocateOperationId();
  semantic::Operation operation{forwardId,documentId_,1,1,
      semantic::DeleteObjectsOp{{selection_.selected().begin(),selection_.selected().end()}}};
  const auto plan=operationEngine_.prepare(operation,{semanticObjects_,appliedOperations_,resourceManifestPort_});
  if(plan.disposition!=semantic::PrepareDisposition::kPrepared || !plan.plan) return false;
  semantic::RestoreObjectsOp inverse;
  for(const auto id:plan.plan->deletes) {
    const auto* object=semanticObjects_.find(id); if(!object) return false;
    inverse.objects.push_back(*object);
  }
  if(!applyLocalHistoryOperation(std::move(operation),{inverseId,documentId_,1,1,std::move(inverse)})) return false;
  selection_.cancel(); selectionView_.reset(); return true;
}

bool CanvasEditingRuntime::toggleSelectionAtViewPoint(float x, float y) noexcept {
  if (!selectionMode_ || runtimeSceneHost_ == nullptr ||
      !std::isfinite(x) || !std::isfinite(y)) return false;
  const auto content = viewportController_->viewToContent(x, y);
  const auto tested = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
      foundation::WorldPoint{content.first, content.second}, 4.0F,
      canvas::HitTestFilter{canvas::HitTestKindMask::kAll, true}, 1U});
  if (!tested || tested.value().frontToBack.empty()) return false;
  if (!selection_.toggle(tested.value().frontToBack.front())) return false;
  snapGuides_.clear();
  canonicalPresentationDirty_ = true;
  if (selection_.primary().isZero()) {
    selectionView_.reset();
    return true;
  }
  return renderSelectionOverlay();
}

bool CanvasEditingRuntime::renderSelectionOverlay() noexcept {
  if (!selectionMode_ || selection_.primary().isZero() ||
      sceneCoordinator_ == nullptr || surface_.width == 0U || surface_.height == 0U) {
    selectionView_.reset();
    return false;
  }
  const auto* record = sceneCoordinator_->runtimeScene().find(selection_.primary());
  if (record == nullptr || !record->worldBounds.isFiniteAndOrdered()) {
    selectionView_.reset();
    return false;
  }
  const auto viewport = viewportController_->state();
  const float zoom = viewport.scale > 0.0F && std::isfinite(viewport.scale)
      ? viewport.scale : 1.0F;
  const auto center = foundation::WorldPoint{
      (surface_.logicalWidth * 0.5F - viewport.translationX) / zoom,
      (surface_.logicalHeight * 0.5F - viewport.translationY) / zoom};
  const auto frame = render::FrameState{
      viewId_,
      render::CameraState{center, zoom, 0.0F,
                          render::CameraGeneration{cameraGeneration()}},
      foundation::WorldRect{-1.0e9F, -1.0e9F, 1.0e9F, 1.0e9F},
      render::SurfaceMetrics{surface_.logicalWidth, surface_.logicalHeight,
                             surface_.width, surface_.height,
                             surface_.devicePixelRatio, surface_.displayScale},
      semanticGeneration_.current(), runtimeSceneHost_->revision(),
      render::SurfaceGeneration{surface_.generation},
      render::MetricsGeneration{surface_.metricsGeneration},
      render::FrameId{nextCanonicalFrameId()},
      foundation::WorldRect{0.0F, 0.0F, surface_.logicalWidth,
                            surface_.logicalHeight}};
  selectionView_ = std::make_unique<render::RenderViewRuntime>(frame);
  const auto cropBounds = imageCropPresentation_
      ? std::optional(canvas::scene::computeBounds(*imageCropPresentation_)) : std::nullopt;
  std::vector<render::AttachmentPointGeometry> attachmentPoints;
  if (record->kind == semantic::ObjectKind::kShape ||
      record->kind == semantic::ObjectKind::kImage ||
      record->kind == semantic::ObjectKind::kSticky) {
    const auto& bounds = cropBounds ? cropBounds->geometry : record->geometryBounds;
    const auto point = [&](float x, float y) {
      const auto world = render::ScreenPoint{
          static_cast<float>(record->transform.a * x + record->transform.c * y + record->transform.tx),
          static_cast<float>(record->transform.b * x + record->transform.d * y + record->transform.ty)};
      const auto screen = render::ScreenPoint{
          static_cast<float>(surface_.logicalWidth * 0.5F +
                             viewport.scale * (world.x - center.x)),
          static_cast<float>(surface_.logicalHeight * 0.5F +
                             viewport.scale * (world.y - center.y))};
      attachmentPoints.push_back({screen, true});
    };
    point((bounds.left + bounds.right) * 0.5F, bounds.top);
    point(bounds.right, (bounds.top + bounds.bottom) * 0.5F);
    point((bounds.left + bounds.right) * 0.5F, bounds.bottom);
    point(bounds.left, (bounds.top + bounds.bottom) * 0.5F);
  }
  const bool updated = selectionView_->editingOverlay().update(render::EditingOverlayInput{
      record->objectId, cropBounds ? cropBounds->visual : record->visualBounds,
      transformOverrides_.find(record->objectId) == nullptr ? record->transform
          : *transformOverrides_.find(record->objectId),
      true, false, transformDrag_.active(),
      render::HandleCapabilities{
          !selectedObjectLocked(), !selectedObjectLocked(), !selectedObjectLocked() && !imageCropPresentation_},
      snapGuides_, attachmentPoints});
  if (updated) ++selectionOverlayUpdates_;
  return updated;
}

void CanvasEditingRuntime::cancelSelectionTransform() noexcept {
  transformDrag_.cancel();
  snapResolver_.reset();
  snapCandidateCount_ = 0;
  snapGuides_.clear();
  if (selectionMode_ && !selection_.primary().isZero()) {
    (void)renderSelectionOverlay();
  }
  editingPointer_ = {};
  editingChanged_ = false;
  canonicalPresentationDirty_ = true;
}

bool CanvasEditingRuntime::selectionPointer(std::uint64_t pointer, input::PointerPhase phase,
                                         float x, float y) noexcept {
  const auto key = editingPointer_.valid() && editingPointer_.pointer == pointer
      ? editingPointer_ : input::PointerKey{~std::uint64_t{0}, pointer, 1U};
  return selectionPointer(key, phase, x, y);
}

bool CanvasEditingRuntime::selectionPointer(const input::PointerKey& key, input::PointerPhase phase,
                                         float x, float y) noexcept {
  if (!selectionMode_ || !key.valid()) return false;
  if (phase == input::PointerPhase::kCancel) {
    if (editingPointer_ == key) cancelSelectionTransform();
    (void)clearTransientOverlay();
    return true;
  }
  if (!std::isfinite(x) || !std::isfinite(y)) {
    if (editingPointer_ == key) cancelSelectionTransform();
    return false;
  }
  const auto world = viewportController_->viewToContent(x, y);
  if (phase == input::PointerPhase::kDown) {
    if (editingPointer_.valid()) return true;
    (void)renderSelectionOverlay();
    editingHandle_ = selectionOverlay() == nullptr ? render::HandleKind::kNone
        : selectionOverlay()->hitTest({x,y});
    bool moveSelected = false;
    if (editingHandle_ == render::HandleKind::kNone && runtimeSceneHost_ != nullptr) {
      const auto hit = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
          {world.first,world.second},4.0F,{canvas::HitTestKindMask::kAll,true},1U});
      moveSelected = hit && !hit.value().frontToBack.empty() &&
          hit.value().frontToBack.front() == selection_.primary();
    }
    if (editingHandle_ == render::HandleKind::kNone && !moveSelected) {
      (void)selectAtViewPoint(x,y);
      editingPointer_ = key;
      (void)presentTransientOverlayFrame(nextCanonicalFrameId());
      return true;
    }
    const auto* object = semanticObjects_.find(selection_.primary());
    if (object == nullptr) return false;
    if (propertyBool(object->properties, 2U, false)) return true;
    const auto bounds = canvas::computeBounds(*object);
    const auto& t = object->transform;
    if (!bounds.finite || std::abs(t.a*t.d-t.b*t.c) < 1e-12) return false;
    const std::array targets{std::pair{object->id,t}};
    if (!transformDrag_.begin(targets)) return false;
    editingPointer_ = key;
    editingInitial_ = t;
    editingLocalBounds_ = bounds.visual;
    editingDownWorld_ = {world.first,world.second};
    editingGeneration_ = semanticGeneration_.current();
    editingChanged_ = false;
    (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    return true;
  }
  if (key != editingPointer_) return true;
  if (transformDrag_.active()) {
    if (editingGeneration_ != semanticGeneration_.current()) {
      cancelSelectionTransform(); return false;
    }
    const auto& t = editingInitial_;
    semantic::Transform2D next=t;
    using H=render::HandleKind;
    const bool left=editingHandle_==H::kLeft||editingHandle_==H::kTopLeft||editingHandle_==H::kBottomLeft;
    const bool right=editingHandle_==H::kRight||editingHandle_==H::kTopRight||editingHandle_==H::kBottomRight;
    const bool top=editingHandle_==H::kTop||editingHandle_==H::kTopLeft||editingHandle_==H::kTopRight;
    const bool bottom=editingHandle_==H::kBottom||editingHandle_==H::kBottomLeft||editingHandle_==H::kBottomRight;
    const double ax=left?editingLocalBounds_.right:editingLocalBounds_.left;
    const double ay=top?editingLocalBounds_.bottom:editingLocalBounds_.top;
    double sx=1.0, sy=1.0;
    if (editingHandle_ == render::HandleKind::kNone) {
      next.tx += world.first-editingDownWorld_.x;
      next.ty += world.second-editingDownWorld_.y;
    } else if (editingHandle_ == render::HandleKind::kRotation) {
      const double cx=(editingLocalBounds_.left+editingLocalBounds_.right)*0.5;
      const double cy=(editingLocalBounds_.top+editingLocalBounds_.bottom)*0.5;
      const double wx=t.a*cx+t.c*cy+t.tx, wy=t.b*cx+t.d*cy+t.ty;
      const double angle=std::atan2(world.second-wy,world.first-wx) -
          std::atan2(editingDownWorld_.y-wy,editingDownWorld_.x-wx);
      // Preserve the exact initial transform for a no-op release. Computing
      // center + (translation - center) can introduce rounding-only edits.
      if (angle != 0.0) {
        const double c=std::cos(angle),s=std::sin(angle);
        next={c*t.a-s*t.b,s*t.a+c*t.b,c*t.c-s*t.d,s*t.c+c*t.d,
              wx+c*(t.tx-wx)-s*(t.ty-wy),wy+s*(t.tx-wx)+c*(t.ty-wy)};
      }
    } else {
      const double det=t.a*t.d-t.b*t.c;
      const double dx=world.first-editingDownWorld_.x,dy=world.second-editingDownWorld_.y;
      const double localDx=(t.d*dx-t.c*dy)/det, localDy=(-t.b*dx+t.a*dy)/det;
      const double width=editingLocalBounds_.right-editingLocalBounds_.left;
      const double height=editingLocalBounds_.bottom-editingLocalBounds_.top;
      sx=(left||right)&&width>1e-6 ? std::clamp(1.0+localDx/(left?-width:width),0.01,100.0):1.0;
      sy=(top||bottom)&&height>1e-6 ? std::clamp(1.0+localDy/(top?-height:height),0.01,100.0):1.0;
      next={t.a*sx,t.b*sx,t.c*sy,t.d*sy,t.tx+t.a*ax*(1-sx)+t.c*ay*(1-sy),
                                         t.ty+t.b*ax*(1-sx)+t.d*ay*(1-sy)};
    }
    if (sceneCoordinator_ != nullptr) {
      const auto viewport = viewportController_->state();
      const auto sourceWorld = transformedBounds(editingLocalBounds_, next);
      const float zoom = viewport.scale > 0.0F && std::isfinite(viewport.scale)
          ? viewport.scale : 1.0F;
      const float snapWorldRadius = interaction::SnapResolver::kEngageThresholdLogicalPx /
          std::max(zoom, 1.0e-6F);
      const canvas::foundation::WorldRect queryBounds{
          sourceWorld.left - snapWorldRadius,
          sourceWorld.top - snapWorldRadius,
          sourceWorld.right + snapWorldRadius,
          sourceWorld.bottom + snapWorldRadius};
      std::vector<interaction::SnapTarget> targets;
      const auto queried = runtimeSceneHost_->query(canvas::SceneQuery{queryBounds});
      if (queried) {
        targets.reserve(queried.value().backToFront.size());
      }
      if (queried) for (const auto objectId : queried.value().backToFront) {
        const auto* record = sceneCoordinator_->runtimeScene().find(objectId);
        if (record == nullptr) continue;
        if (record->objectId == selection_.primary()) continue;
        targets.push_back({record->objectId, viewBounds(record->worldBounds, viewport),
                           false, transformOverrides_.find(record->objectId) != nullptr});
      }
      const auto resolved = snapResolver_.resolve(
          interaction::SnapSource{viewBounds(sourceWorld, viewport)}, targets, 1.0F);
      snapCandidateCount_ = resolved.candidatesExamined;
      snapGuides_.clear();
      const auto appendGuide = [&](const auto& guide) {
        if (!guide.has_value()) return;
        if (guide->axis == interaction::SnapGuideAxis::kVertical) {
          snapGuides_.push_back({render::SnapGuideAxis::kVertical,
                                 {guide->position, guide->spanStart},
                                 {guide->position, guide->spanEnd}});
        } else {
          snapGuides_.push_back({render::SnapGuideAxis::kHorizontal,
                                 {guide->spanStart, guide->position},
                                 {guide->spanEnd, guide->position}});
        }
      };
      appendGuide(resolved.xGuide);
      appendGuide(resolved.yGuide);
      const float scale = viewport.scale > 0.0F ? viewport.scale : 1.0F;
      const auto featureValue = [](const foundation::WorldRect& rect,
                                   interaction::SnapFeatureKind feature,
                                   bool horizontal) {
        const double minimum = horizontal ? rect.left : rect.top;
        const double maximum = horizontal ? rect.right : rect.bottom;
        if (feature == interaction::SnapFeatureKind::kCenter) {
          return (minimum + maximum) * 0.5;
        }
        return feature == interaction::SnapFeatureKind::kMinEdge ? minimum : maximum;
      };
      const auto rebuildScaled = [&](double sxValue, double syValue) {
        return semantic::Transform2D{
            t.a * sxValue, t.b * sxValue, t.c * syValue, t.d * syValue,
            t.tx + t.a * ax * (1.0 - sxValue) + t.c * ay * (1.0 - syValue),
            t.ty + t.b * ax * (1.0 - sxValue) + t.d * ay * (1.0 - syValue)};
      };
      double snappedSx = sx;
      double snappedSy = sy;
      const auto solveScaleForSnap = [&](const interaction::SnapCandidate& candidate,
                                         bool horizontal, double& factor) {
        const auto current = transformedBounds(editingLocalBounds_,
            rebuildScaled(snappedSx, snappedSy));
        const double currentValue = featureValue(current, candidate.sourceFeature, horizontal);
        const double targetValue = currentValue +
            static_cast<double>(candidate.correction) / static_cast<double>(scale);
        const double epsilon = 1.0e-4;
        const double probe = std::clamp(factor + epsilon, 0.01, 100.0);
        const auto probed = horizontal
            ? transformedBounds(editingLocalBounds_, rebuildScaled(probe, snappedSy))
            : transformedBounds(editingLocalBounds_, rebuildScaled(snappedSx, probe));
        const double derivative =
            (featureValue(probed, candidate.sourceFeature, horizontal) - currentValue) /
            (probe - factor);
        if (!std::isfinite(derivative) || std::abs(derivative) < 1.0e-8) return;
        factor = std::clamp(factor + (targetValue - currentValue) / derivative,
                            0.01, 100.0);
      };
      const bool resizeX = left || right;
      const bool resizeY = top || bottom;
      if (resolved.x.has_value()) {
        if (resizeX) solveScaleForSnap(*resolved.x, true, snappedSx);
        else next.tx += resolved.x->correction / scale;
      }
      if (resolved.y.has_value()) {
        if (resizeY) solveScaleForSnap(*resolved.y, false, snappedSy);
        else next.ty += resolved.y->correction / scale;
      }
      if (resizeX || resizeY) next = rebuildScaled(snappedSx, snappedSy);
    }
    editingChanged_ = !(next==editingInitial_);
    const std::array values{std::pair{selection_.primary(),next}};
    if (!transformDrag_.preview(values)) { cancelSelectionTransform(); return false; }
    // Refresh the transient overlay immediately so a guide appears/disappears
    // in the same interaction frame as the resolved snap, without waiting for
    // a canonical operation or a later unrelated repaint.
    (void)renderSelectionOverlay();
  canonicalPresentationDirty_ = true;
    (void)presentTransientOverlayFrame(nextCanonicalFrameId());
    if (phase == input::PointerPhase::kUp) {
      const bool accepted=!editingChanged_ || transformDrag_.commit();
      const bool needsHandoff = accepted && editingChanged_;
      if (needsHandoff) {
        transientOverlayHandoffPending_ = true;
        if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(true);
      }
      cancelSelectionTransform();
      // A click on an already selected Shape draws an opaque transient copy,
      // but creates no canonical commit/handoff. Retire that no-op preview so
      // subsequent text/preedit frames cannot remain hidden beneath it.
      if (!needsHandoff) (void)clearTransientOverlay();
      return accepted;
    }
  }
  if (phase == input::PointerPhase::kUp) {
    editingPointer_={};
    (void)clearTransientOverlay();
  }
  return true;
}

interaction::SubmitResult CanvasEditingRuntime::submit(const semantic::SetTransformsOp& transforms) {
  if (transforms.items.size()!=1U || !historySceneReady() ||
      editingGeneration_!=semanticGeneration_.current()) return interaction::SubmitResult::rejected();
  const auto* before=semanticObjects_.find(transforms.items.front().object_id);
  if (before==nullptr || before->id!=selection_.primary() || selectedObjectLocked()) return interaction::SubmitResult::rejected();
  const auto forwardId=allocateOperationId(), inverseId=allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return interaction::SubmitResult::rejected();
  const semantic::Operation operation{forwardId,documentId_,1U,1U,transforms};
  semantic::Operation inverse{inverseId,documentId_,1U,1U,
      semantic::SetTransformsOp{{{before->id,before->transform}}}};
  return applyLocalHistoryOperation(operation, std::move(inverse))
      ? interaction::SubmitResult::acceptedResult() : interaction::SubmitResult::rejected();
}

bool CanvasEditingRuntime::selectBrushProfile(std::string_view profileId,
                                           std::uint32_t revision) noexcept {
  if (!brushSessions_.empty() ||
      (revision != 1U && !(profileId == "chalk-grain-v1" &&
                            (revision == 2U || revision == 3U || revision == 4U)))) return false;
  const auto loaded = brushCatalog_.loadDefault(profileId, revision);
  if (!loaded) return false;
  selectedBrushProfile_ = std::string(profileId);
  selectedBrushRevision_ = revision;
  if (!inkSizeOverride_) inkSize_ = static_cast<float>(loaded.package.vector.size);
  if (!inkColorOverride_) {
    inkRed_ = static_cast<float>(loaded.package.paint.red);
    inkGreen_ = static_cast<float>(loaded.package.paint.green);
    inkBlue_ = static_cast<float>(loaded.package.paint.blue);
    inkAlpha_ = static_cast<float>(loaded.package.paint.alpha);
  }
  if (!inkOpacityOverride_) inkOpacity_ = static_cast<float>(loaded.package.paint.opacity);
  return true;
}

bool CanvasEditingRuntime::setInkOptions(float size, float red, float green, float blue,
                                      float alpha, float opacity) noexcept {
  if (!std::isfinite(size) || size <= 0.0F || !std::isfinite(red) ||
      !std::isfinite(green) || !std::isfinite(blue) || !std::isfinite(alpha) ||
      !std::isfinite(opacity) || red < 0.0F || red > 1.0F || green < 0.0F ||
      green > 1.0F || blue < 0.0F || blue > 1.0F || alpha < 0.0F || alpha > 1.0F ||
      opacity < 0.0F || opacity > 1.0F || !brushSessions_.empty()) return false;
  inkSize_ = size;
  inkRed_ = red;
  inkGreen_ = green;
  inkBlue_ = blue;
  inkAlpha_ = alpha;
  inkOpacity_ = opacity;
  inkSizeOverride_ = true;
  inkColorOverride_ = true;
  inkOpacityOverride_ = true;
  return true;
}

void CanvasEditingRuntime::clearInkSizeOverride() noexcept { inkSizeOverride_ = false; }
void CanvasEditingRuntime::clearInkColorOverride() noexcept { inkColorOverride_ = false; }
void CanvasEditingRuntime::clearInkOpacityOverride() noexcept { inkOpacityOverride_ = false; }

bool CanvasEditingRuntime::setEraserDiameterLogicalPx(float diameter) noexcept {
  if (!std::isfinite(diameter) || diameter <= 0.0F || !eraserTraces_.empty()) return false;
  eraserDiameterLogicalPx_ = diameter;
  return true;
}

bool CanvasEditingRuntime::setEraserMode(std::uint32_t mode) noexcept {
  if (mode != 1U && mode != 2U) return false;
  if (!brushSessions_.empty() || !eraserTraces_.empty()) return false;
  eraserMode_ = mode;
  toolMode_ = mode == 1U ? ToolMode::kObjectEraser : ToolMode::kPartialEraser;
  panTool_ = false;
  return true;
}

bool CanvasEditingRuntime::setEraserOptions(std::uint32_t mode, float diameter) noexcept {
  if (mode != 1U && mode != 2U) return false;
  if (!std::isfinite(diameter) || diameter <= 0.0F ||
      !brushSessions_.empty() || !eraserTraces_.empty()) return false;
  eraserMode_ = mode;
  eraserDiameterLogicalPx_ = diameter;
  toolMode_ = mode == 1U ? ToolMode::kObjectEraser : ToolMode::kPartialEraser;
  panTool_ = false;
  return true;
}

std::optional<CanvasEditingRuntime::InkDefaults> CanvasEditingRuntime::brushCatalogDefaults(
    std::string_view profile, std::uint32_t revision) const noexcept {
  const auto loaded = brushCatalog_.loadDefault(profile, revision);
  if (!loaded) return std::nullopt;
  const auto& paint = loaded.package.paint;
  return InkDefaults{static_cast<float>(loaded.package.vector.size),
                     static_cast<float>(paint.red), static_cast<float>(paint.green),
                     static_cast<float>(paint.blue), static_cast<float>(paint.alpha),
                     static_cast<float>(paint.opacity)};
}

bool CanvasEditingRuntime::eraserBegin(std::uint64_t pointerId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  // A selection/creation frame uses the same transient provider as the
  // eraser. Hide and clear that frame before starting a new eraser session;
  // otherwise a just-moved Shape can remain composited at its former
  // position for one refresh while the canonical frame is handed off.
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(false);
  if (transientOverlayDirty_ || selectionOverlay() != nullptr) {
    (void)clearTransientOverlay();
  }
  // A transform commit leaves the canonical surface dirty until the platform
  // render pump presents it. Do that handoff before starting an eraser so the
  // overlay cannot expose the shape's former location for one frame.
  if (canonicalPresentationDirty_ && surface_.available && activeSurfaceProvider() != nullptr) {
    if (!presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false)) return false;
  }
  std::lock_guard lock(previewStateMutex_);
  if (toolMode_ == ToolMode::kBrush || pointerId == 0U || eraserTraces_.contains(pointerId)) return false;
  // A prior brush/eraser handoff may have left a retained contour while its
  // canonical frame was presented through the local raster provider. Clear
  // that stale transient before starting a new eraser session.
  if (previewController_ != nullptr && previewController_->active() &&
      previewProvider_ != nullptr) {
    (void)previewController_->cancel();
    (void)previewController_->retireSession(pointerId, previewProvider_->generation());
    previewProvider_->setOverlayVisible(false);
  }
  eraserTraces_[pointerId] = {};
  eraserPreviewRevisions_[pointerId] = 0U;
  if (previewController_ == nullptr || previewProvider_ == nullptr ||
      !previewController_->begin(1U, pointerId, 1U, previewProvider_->generation())) {
    eraserTraces_.erase(pointerId);
    eraserPreviewRevisions_.erase(pointerId);
    return false;
  }
  return true;
}

bool CanvasEditingRuntime::eraserSample(std::uint64_t pointerId, double x, double y,
                                     bool renderPreview) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const auto it = eraserTraces_.find(pointerId);
  if (it == eraserTraces_.end() || !std::isfinite(x) || !std::isfinite(y)) return false;
  it->second.push_back({static_cast<float>(x), static_cast<float>(y)});
  constexpr std::size_t kSegments = 24U;
  const double kRadius = static_cast<double>(eraserDiameterLogicalPx_) * 0.5;
  ink::BrushPreviewDelta delta;
  delta.revision = ++eraserPreviewRevisions_[pointerId];
  const auto& trace = it->second;
  if (trace.size() == 1U) {
    delta.outline.reserve(kSegments);
    for (std::size_t i = 0; i < kSegments; ++i) {
      const double angle = 2.0 * 3.14159265358979323846 * static_cast<double>(i) /
                           static_cast<double>(kSegments);
      delta.outline.push_back({x + kRadius * std::cos(angle),
                               y + kRadius * std::sin(angle)});
    }
  } else {
    // Build one closed swept-band contour from the retained eraser trace.
    // This keeps the erased region visibly continuous instead of showing
    // only the latest pointer-centered circle.
    std::vector<ink::reference::StrokeOutlinePoint> left;
    std::vector<ink::reference::StrokeOutlinePoint> right;
    left.reserve(trace.size());
    right.reserve(trace.size());
    for (std::size_t i = 0; i < trace.size(); ++i) {
      const auto& current = trace[i];
      const auto& previous = trace[i == 0U ? i : i - 1U];
      const auto& next = trace[i + 1U < trace.size() ? i + 1U : i];
      const double dx = static_cast<double>(next.x - previous.x);
      const double dy = static_cast<double>(next.y - previous.y);
      const double length = std::hypot(dx, dy);
      const double nx = length > 0.0 ? -dy / length : 0.0;
      const double ny = length > 0.0 ? dx / length : 1.0;
      left.push_back({current.x + kRadius * nx, current.y + kRadius * ny});
      right.push_back({current.x - kRadius * nx, current.y - kRadius * ny});
    }
    delta.outline.reserve(left.size() + right.size());
    delta.outline.insert(delta.outline.end(), left.begin(), left.end());
    for (auto reverse = right.rbegin(); reverse != right.rend(); ++reverse) {
      delta.outline.push_back(*reverse);
    }
  }
  const auto viewport = viewportController_->state();
  render::PreviewStyleOverride style;
  if (toolMode_ == ToolMode::kPartialEraser) {
    // Preview is an independent transparent overlay.  For partial erase we
    // paint the swept region with the canvas background so the user sees the
    // final visual effect grow sample-by-sample; semantic masks are still
    // committed only by eraserFinish(). Object eraser keeps the amber tool
    // indicator and its existing commit-time disappearance semantics.
    style.red = 1.0F;
    style.green = 1.0F;
    style.blue = 1.0F;
    style.alpha = 1.0F;
    style.opacityMultiplier = 1.0F;
    style.overrideColor = true;
    style.overrideOpacity = true;
    style.preferCapturedPaint = false;

    // A partial eraser is a stroke-only operation.  Do not paint the white
    // preview mask over a non-ink object: that transparent overlay would
    // otherwise make a Shape appear partially erased even though the commit
    // path correctly excludes Shapes.  When the trace is over a Shape (or
    // another non-ink object), keep the preview surface clear and leave the
    // canonical object untouched.
    bool overlapsNonInk = false;
    bool overlapsInk = false;
    if (runtimeSceneHost_ != nullptr) {
      for (const auto& point : trace) {
        const auto tested = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
            {point.x, point.y}, static_cast<float>(kRadius),
            canvas::HitTestFilter{canvas::HitTestKindMask::kAll, false}, 16U});
        if (!tested) continue;
        for (const auto id : tested.value().frontToBack) {
          const auto* object = semanticObjects_.find(id);
          if (object == nullptr) continue;
          if (object->kind != semantic::ObjectKind::kVectorStroke &&
              object->kind != semantic::ObjectKind::kDabStroke) {
            overlapsNonInk = true;
          } else {
            overlapsInk = true;
          }
        }
        if (overlapsNonInk && overlapsInk) break;
      }
    }
    // The preview layer is a presentation-only mask and cannot clip itself
    // against canonical object coverage. Never paint it when the trace is
    // outside ink, or when it crosses a non-ink object: otherwise a moved
    // Shape exposes a white/purple ghost along its former path.
    if (!overlapsInk || overlapsNonInk) {
      delta.outline.clear();
      delta.dabs.clear();
    }
  }
  previewController_->setPresentationStyle(style);
  return previewController_->updateForSession(pointerId, delta, viewport.scale,
                                               viewport.translationX,
                                               viewport.translationY) &&
         (!renderPreview || platformPresentationDeferred_
              ? true : previewController_->renderIfDirty(style));
}

bool CanvasEditingRuntime::eraserFinish(std::uint64_t pointerId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  std::lock_guard lock(previewStateMutex_);
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(false);
  const auto it = eraserTraces_.find(pointerId);
  if (it == eraserTraces_.end()) return false;
  if (!canonicalWriteAdmission()) return false;
  if (it->second.empty() || runtimeSceneHost_ == nullptr) {
    eraserTraces_.erase(it);
    eraserPreviewRevisions_.erase(pointerId);
    if (previewController_ != nullptr && previewController_->active()) {
      (void)previewController_->cancelSession(pointerId);
      (void)previewController_->retireSession(pointerId, previewProvider_->generation());
    }
    return false;
  }
  std::unordered_set<foundation::ObjectId, foundation::ObjectIdHash> hit;
  for (const auto& point : it->second) {
    const auto tested = runtimeSceneHost_->hitTest(canvas::HitTestRequest{
        point, eraserDiameterLogicalPx_ * 0.5F, canvas::HitTestFilter{static_cast<canvas::HitTestKindMask>(
            static_cast<std::uint32_t>(canvas::HitTestKindMask::kVectorStroke) |
            static_cast<std::uint32_t>(canvas::HitTestKindMask::kDabStroke)), false}, 64U});
    if (!tested) {
      eraserTraces_.erase(it);
      eraserPreviewRevisions_.erase(pointerId);
      if (previewController_ != nullptr && previewController_->active() &&
          previewProvider_ != nullptr) {
        (void)previewController_->cancelSession(pointerId);
        (void)previewController_->retireSession(pointerId, previewProvider_->generation());
      }
      return false;
    }
    for (const auto id : tested.value().frontToBack) hit.insert(id);
  }
  if (hit.empty()) {
    eraserTraces_.erase(it);
    eraserPreviewRevisions_.erase(pointerId);
    if (previewController_ != nullptr && previewController_->active()) {
      (void)previewController_->cancelSession(pointerId);
      (void)previewController_->retireSession(pointerId, previewProvider_->generation());
    }
    return true;
  }
  semantic::Operation operation;
  const auto operationOrdinal = localOperationOrdinal();
  if (operationOrdinal == 0U) return false;
  operation.id = semantic::OperationId(foundation::ObjectId::fromUint64(operationOrdinal));
  operation.document_id = documentId_;
  operation.schema_version = 1U;
  operation.payload_version = 1U;
  if (toolMode_ == ToolMode::kObjectEraser) {
    semantic::DeleteObjectsOp payload;
    payload.object_ids.assign(hit.begin(), hit.end());
    std::sort(payload.object_ids.begin(), payload.object_ids.end());
    operation.payload = std::move(payload);
  } else {
    const double radius = static_cast<double>(eraserDiameterLogicalPx_) * 0.5;
    semantic::AddEraseMasksOp maskPayload;
    std::size_t replacementOrdinal = 0U;
    for (const auto id : hit) {
      // Keep the canonical stroke intact and attach a renderer-neutral mask.
      // Splitting by deleting outline vertices and implicitly closing each
      // remainder manufactures long phantom edges (and the visible arcs/
      // triangles reported in the Web UI). SplitStrokes needs a true polygon
      // clipper; until that authority exists, fail over to the same mask
      // representation used by marker/chalk.
      if (it->second.empty()) continue;
      const auto* object = semanticObjects_.find(id);
      if (object == nullptr) return false;
      const auto& transform = object->transform;
      const double determinant = transform.a * transform.d - transform.b * transform.c;
      if (!std::isfinite(determinant) || determinant == 0.0) return false;
      const auto localPoint = [&](const semantic::Vec2& point) {
        const double x = point.x - transform.tx;
        const double y = point.y - transform.ty;
        return semantic::Vec2{(transform.d * x - transform.c * y) / determinant,
                              (transform.a * y - transform.b * x) / determinant};
      };
      // EraseMaskGeometry is object-local. The retained trace and preview
      // are world-space; convert at this operation construction boundary.
      semantic::SweptCircleMask swept;
      for (std::size_t i = 1; i < it->second.size(); ++i) {
        const auto& a = it->second[i - 1U];
        const auto& b = it->second[i];
        swept.segments.push_back({
            {{a.x, a.y}, radius}, {{b.x, b.y}, radius},
            {a.x + (b.x - a.x) / 3.0F, a.y + (b.y - a.y) / 3.0F},
            {a.x + (b.x - a.x) * 2.0F / 3.0F, a.y + (b.y - a.y) * 2.0F / 3.0F}});
      }
      if (swept.segments.empty()) {
        const auto& p = it->second.front();
        swept.segments.push_back({{{p.x, p.y}, radius}, {{p.x, p.y}, radius},
                                  {p.x, p.y}, {p.x, p.y}});
      }
      semantic::EraseMaskGeometry geometry;
      const double scaleX = std::hypot(transform.a, transform.b);
      const double scaleY = std::hypot(transform.c, transform.d);
      const double scale = std::max(scaleX, scaleY);
      const bool similarity = std::abs(scaleX - scaleY) <= 1e-12 * scale &&
          std::abs((transform.a / scale) * (transform.c / scale) +
                   (transform.b / scale) * (transform.d / scale)) <= 1e-12;
      if (similarity) {
        for (auto& segment : swept.segments) {
          segment.p0.position = localPoint(segment.p0.position);
          segment.p1.position = localPoint(segment.p1.position);
          segment.control1 = localPoint(segment.control1);
          segment.control2 = localPoint(segment.control2);
          segment.p0.radius /= scaleX;
          segment.p1.radius /= scaleX;
        }
        geometry = std::move(swept);
      } else {
        // A world circle is an ellipse in a nonuniformly scaled/sheared
        // object's local space. Use the existing FilledPathMask variant for
        // the inverse-mapped union of round caps and connecting rectangles.
        // Never approximate it with a single local radius.
        semantic::FilledPathMask filled;
        auto& commands = filled.path.commands;
        constexpr double kCircleControl = 0.5522847498307936;
        for (const auto& point : it->second) {
          const double x = point.x, y = point.y;
          const double k = radius * kCircleControl;
          commands.push_back(semantic::MoveTo{localPoint({x + radius, y})});
          commands.push_back(semantic::CubicTo{localPoint({x + radius, y + k}),
              localPoint({x + k, y + radius}), localPoint({x, y + radius})});
          commands.push_back(semantic::CubicTo{localPoint({x - k, y + radius}),
              localPoint({x - radius, y + k}), localPoint({x - radius, y})});
          commands.push_back(semantic::CubicTo{localPoint({x - radius, y - k}),
              localPoint({x - k, y - radius}), localPoint({x, y - radius})});
          commands.push_back(semantic::CubicTo{localPoint({x + k, y - radius}),
              localPoint({x + radius, y - k}), localPoint({x + radius, y})});
          commands.push_back(semantic::ClosePath{});
        }
        for (std::size_t i = 1; i < it->second.size(); ++i) {
          const auto& a = it->second[i - 1U];
          const auto& b = it->second[i];
          const double dx = static_cast<double>(b.x) - a.x;
          const double dy = static_cast<double>(b.y) - a.y;
          const double length = std::hypot(dx, dy);
          if (length == 0.0) continue;
          const double nx = -dy * radius / length, ny = dx * radius / length;
          commands.push_back(semantic::MoveTo{localPoint({a.x + nx, a.y + ny})});
          commands.push_back(semantic::LineTo{localPoint({a.x - nx, a.y - ny})});
          commands.push_back(semantic::LineTo{localPoint({b.x - nx, b.y - ny})});
          commands.push_back(semantic::LineTo{localPoint({b.x + nx, b.y + ny})});
          commands.push_back(semantic::ClosePath{});
        }
        geometry = std::move(filled);
      }
      maskPayload.items.push_back({id, {{foundation::ObjectId::fromUint64(
          operationOrdinal * 1000U + ++replacementOrdinal), std::move(geometry)}}});
    }
    operation.payload = std::move(maskPayload);
  }
  semantic::Operation inverse;
  inverse.id = allocateOperationId();
  if (inverse.id.isZero()) return false;
  inverse.document_id = documentId_;
  inverse.schema_version = 1U;
  inverse.payload_version = 1U;
  if (toolMode_ == ToolMode::kObjectEraser) {
    semantic::RestoreObjectsOp restore;
    for (const auto id : std::get<semantic::DeleteObjectsOp>(operation.payload).object_ids) {
      const auto* record = semanticObjects_.find(id);
      if (record != nullptr) restore.objects.push_back(*record);
    }
    inverse.payload = std::move(restore);
  } else {
    semantic::RemoveEraseMasksOp remove;
    for (const auto& item : std::get<semantic::AddEraseMasksOp>(operation.payload).items) {
      semantic::EraseMaskRemoveItem target;
      target.object_id = item.object_id;
      for (const auto& mask : item.masks) target.mask_ids.push_back(mask.id);
      remove.items.push_back(std::move(target));
    }
    inverse.payload = std::move(remove);
  }
  const auto applied = operationEngine_.apply(operation, semantic::ApplySource::kLocalInteraction,
      semanticObjects_, appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  eraserTraces_.erase(it);
  eraserPreviewRevisions_.erase(pointerId);
  if (applied.disposition != semantic::ApplyDisposition::kApplied) return false;
  observeApplied(operation, applied);
  if (!history_.record(operation, {std::move(inverse)})) return false;
  ++submittedOperationCount_;
  if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr && sceneCompiler_ != nullptr) {
    semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
    canvas::SceneCommitInput input(applied.commit_record->before_generation,
                                   applied.commit_record->after_generation, view,
                                   &applied.commit_record->change_set);
    // Semantic acceptance is final. Scene is derived and can recover at the
    // next frame; a failed projection must not lose the accepted history.
    (void)sceneCoordinator_->apply(*sceneCompiler_, input);
  }
  canonicalPresentationDirty_ = true;
  if (!platformPresentationDeferred_) {
    // Semantic acceptance is already complete. A failed derived present must
    // not skip preview cleanup; the next frame may recover the Scene while the
    // eraser overlay is retired immediately.
    (void)presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false);
  }
  if (previewController_ != nullptr && previewController_->active()) {
    if (!previewController_->cancelSession(pointerId) || previewProvider_ == nullptr ||
        !previewController_->retireSession(pointerId, previewProvider_->generation())) return false;
  }
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(false);
  return true;
}

bool CanvasEditingRuntime::eraserCancel(std::uint64_t pointerId) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const bool erased = eraserTraces_.erase(pointerId) != 0U;
  eraserPreviewRevisions_.erase(pointerId);
  // Hide the platform overlay before any clear/retire present. DirectComposition
  // can otherwise expose the last white eraser frame for one refresh interval.
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(false);
  if (previewController_ != nullptr && previewController_->active()) {
    if (!previewController_->cancelSession(pointerId) || previewProvider_ == nullptr ||
        !previewController_->retireSession(pointerId, previewProvider_->generation())) return false;
  }
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(false);
  return erased;
}

bool CanvasEditingRuntime::appendBrushSample(std::uint64_t pointerId, double x, double y,
                                          double pressure, std::uint64_t sequence,
                                          bool predicted, bool renderPreview) noexcept {
  const ink::BrushSample sample{x, y, pressure, true, sequence};
  return predicted
      ? appendBrushSamples(pointerId, {}, std::span<const ink::BrushSample>(&sample, 1), renderPreview)
      : appendBrushSamples(pointerId, std::span<const ink::BrushSample>(&sample, 1), {}, renderPreview);
}

bool CanvasEditingRuntime::appendBrushSamples(
    std::uint64_t pointerId, std::span<const ink::BrushSample> confirmed,
    std::span<const ink::BrushSample> predicted, bool renderPreview) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  ink::BrushPreviewDelta delta;
  const auto error = it->second->append(confirmed, predicted, delta);
  if (error != ink::BrushSessionError::kNone) { AXIOM_ANDROID_DIAG("BrushSession append error=%d confirmed=%zu predicted=%zu", static_cast<int>(error), confirmed.size(), predicted.size()); return false; }
  if (arcPreviewSink_ != nullptr) {
    const ink::PreviewIdentity identity{1U, pointerId, 1U};
    const auto disposition = arcPreviewSink_->update(identity, delta);
    if (disposition == ink::PreviewSubmitResult::kRejected) return false;
  }
  const auto viewport = viewportController_->state();
  if (previewController_ == nullptr ||
      !previewController_->updateForSession(pointerId, delta, viewport.scale,
                                  viewport.translationX,
                                  viewport.translationY)) { AXIOM_ANDROID_DIAG("preview update failed rev=%llu active=%d state_rev=%llu gen=%llu geom_gen=%llu geom_rev=%llu", static_cast<unsigned long long>(delta.revision), previewController_ != nullptr && previewController_->active() ? 1 : 0, previewController_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewController_->state().contentRevision), previewProvider_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewProvider_->generation()), previewController_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewController_->geometry().surfaceGeneration.value()), previewController_ == nullptr ? 0ULL : static_cast<unsigned long long>(previewController_->geometry().revision)); return false; }
  if (renderPreview && !platformPresentationDeferred_ &&
      !previewController_->renderIfDirty()) { AXIOM_ANDROID_DIAG("preview render failed rev=%llu", static_cast<unsigned long long>(delta.revision)); return false; }
  brushPreviews_[pointerId] = std::move(delta.outline);
  brushPreviewDigest_ = hashOutline(brushPreviews_[pointerId]);
  return true;
}

bool CanvasEditingRuntime::presentBrushPreview() noexcept {
  std::lock_guard lock(previewStateMutex_);
  return previewController_ != nullptr &&
      previewController_->renderIfDirty(previewController_->presentationStyle());
}

bool CanvasEditingRuntime::capturePreviewPresentation(
    PreviewPresentationCapture& capture) noexcept {
  std::lock_guard lock(previewStateMutex_);
  if (previewController_ == nullptr || !previewController_->active() ||
      !previewController_->state().dirty) return false;
  capture.geometry = previewController_->geometry();
  capture.style = previewController_->presentationStyle();
  capture.contentRevision = previewController_->state().contentRevision;
  capture.surfaceGeneration = previewController_->state().generation;
  return true;
}

bool CanvasEditingRuntime::renderPreviewPresentation(
    const PreviewPresentationCapture& capture,
    std::uint64_t* presentCount) noexcept {
  previewPresentationRetryable_ = false;
  std::lock_guard renderLock(previewRenderMutex_);
  std::lock_guard providerLock(previewProviderMutex_);
  render::SkiaSurfaceProvider* provider = nullptr;
  {
    std::lock_guard stateLock(previewStateMutex_);
    if (previewController_ == nullptr || !previewController_->active() ||
        previewProvider_ == nullptr ||
        previewProvider_->generation() != capture.surfaceGeneration) return false;
    provider = previewProvider_;
  }
  const auto result = previewRenderer_.renderPreview(*provider, capture.geometry,
                                                  capture.style);
  previewPresentationRetryable_ = result.code == render::BackendSubmissionCode::kRetry;
  if (result.code != render::BackendSubmissionCode::kAccepted) return false;
  // Windows deferred presentation submits the preview directly from the
  // platform render pump, bypassing PreviewSurfaceController::renderIfDirty.
  // Make the successfully submitted frame visible at this boundary; the
  // later UI-thread acknowledgement only records the presentation receipt
  // and must not be the sole visibility transition because CanonicalVisible
  // can race that message.
  provider->setOverlayVisible(true);
  if (presentCount != nullptr) *presentCount = provider->presentCount();
  return true;
}

bool CanvasEditingRuntime::acknowledgePreviewPresentation(
    const PreviewPresentationCapture& capture, std::uint64_t presentCount) noexcept {
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard stateLock(previewStateMutex_);
  if (previewController_ == nullptr || previewProvider_ == nullptr ||
      previewProvider_->generation() != capture.surfaceGeneration) return false;
  const bool accepted = previewController_->markPresented(
      capture.contentRevision, capture.surfaceGeneration, presentCount);
  if (accepted) previewProvider_->setOverlayVisible(true);
  if (!accepted || pendingCanonicalIdentities_.empty()) {
    return accepted;
  }
  const auto* canonical = lastSubmittedCanonicalFrame_.has_value()
      ? &*lastSubmittedCanonicalFrame_ : lastPresentedCanonicalFrame_.has_value()
          ? &*lastPresentedCanonicalFrame_ : nullptr;
  const auto* canonicalState = lastSubmittedCanonicalFrameState_ != nullptr
      ? lastSubmittedCanonicalFrameState_.get()
      : lastPresentedCanonicalFrameState_.get();
  if (canonical == nullptr || canonicalState == nullptr) return accepted;
  const bool canonicalQualified = presentedSemanticGeneration_.value() ==
      canonical->sceneGeneration && presentedSurfaceGeneration_ == canonical->surfaceGeneration &&
      presentedMetricsGeneration_ == canonical->metricsGeneration;
  if (!canonicalQualified || !previewController_->active()) return accepted;
  std::vector<std::uint64_t> retired;
  for (const auto& [pointer, handoffIdentity] : pendingCanonicalIdentities_) {
    if (canonicalVisibilitySink_ != nullptr) {
      const auto handoff = canonicalVisibilitySink_->canonicalVisible(
          handoffIdentity, *canonicalState);
      if (handoff == ink::HandoffResult::kRejected) return false;
      if (handoff != ink::HandoffResult::kAccepted) continue;
    }
    if (!previewController_->retireSession(handoffIdentity.session,
                                           previewProvider_->generation())) {
      return false;
    }
    retired.push_back(pointer);
  }
  for (const auto pointer : retired) pendingCanonicalIdentities_.erase(pointer);
  return accepted;
}

void CanvasEditingRuntime::setPreviewOverlayVisible(bool visible) noexcept {
  std::lock_guard providerLock(previewProviderMutex_);
  if (previewProvider_ != nullptr) previewProvider_->setOverlayVisible(visible);
}

void CanvasEditingRuntime::setPlatformPresentationDeferred(bool deferred) noexcept {
  platformPresentationDeferred_ = deferred;
  if (previewController_ != nullptr) previewController_->setPresentationDeferred(deferred);
}

bool CanvasEditingRuntime::finishBrushSession(std::uint64_t pointerId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  std::lock_guard lock(previewStateMutex_);
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  if (!canonicalWriteAdmission()) return false;
  AXIOM_ANDROID_DIAG("finish begin pointer=%llu objects=%zu scene_gen=%llu", static_cast<unsigned long long>(pointerId), semanticObjects_.size(), static_cast<unsigned long long>(semanticGeneration_.current().value()));
  ink::BrushCommitIntent intent;
  if (it->second->seal(intent) != ink::BrushSessionError::kNone) return false;
  brushSealedOutlineDigest_ = hashOutline(intent.outline);
  const auto package = brushSessionPackages_.at(pointerId);
  const auto seed = brushSessionSeeds_.at(pointerId);
  ink::BrushSession replay(pointerId, ink::resolveBrushState(package, seed));
    ink::BrushPreviewDelta replayPreview;
  ink::BrushCommitIntent replayIntent;
  if (!replay.begin() ||
      replay.append(intent.confirmed, {}, replayPreview) != ink::BrushSessionError::kNone ||
      replay.seal(replayIntent) != ink::BrushSessionError::kNone) return false;
  brushReplayDigest_ = hashOutline(replayIntent.outline);
  if (brushReplayDigest_ != brushSealedOutlineDigest_) return false;
  const bool usesDabs = (package.profileId == "chalk-grain-v1" &&
                         package.revision >= 2U && package.revision <= 4U) ||
                        package.profileId == "membrane-v1";
  if (usesDabs && replayIntent.dabDigest != intent.dabDigest) return false;
  if (!BrushCommitAdapter::valid(intent, package)) return false;
  brushDigest_ = hashValue(hashValue(hashValue(kFnvOffset, brushSealedOutlineDigest_),
                                     brushReplayDigest_), intent.revision);
  const auto operationId = localOperationOrdinal();
  if (operationId == 0U) return false;
  const auto operation = BrushCommitAdapter::build(
      intent, package, operationId, documentId_);
  semantic::Operation inverse;
  inverse.id = allocateOperationId();
  if (inverse.id.isZero()) return false;
  inverse.document_id = documentId_;
  inverse.schema_version = 1U;
  inverse.payload_version = 1U;
  inverse.payload = semantic::DeleteObjectsOp{{foundation::ObjectId::fromUint64(operationId)}};
  const auto applied = operationEngine_.apply(
      operation, semantic::ApplySource::kLocalInteraction, semanticObjects_,
      appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied) {
    return false;
  }
  observeApplied(operation, applied);
  if (!history_.record(operation, {std::move(inverse)})) return false;
  ++submittedOperationCount_;
  if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr &&
      sceneCompiler_ != nullptr) {
    semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
    canvas::SceneCommitInput sceneInput(
        applied.commit_record->before_generation,
        applied.commit_record->after_generation,
        view, &applied.commit_record->change_set);
    const auto synchronized = sceneCoordinator_->apply(*sceneCompiler_, sceneInput);
    AXIOM_ANDROID_DIAG("scene apply pointer=%llu ok=%d objects=%zu scene_gen=%llu", static_cast<unsigned long long>(pointerId), synchronized ? 1 : 0, semanticObjects_.size(), static_cast<unsigned long long>(semanticGeneration_.current().value()));
  }
  canonicalPresentationDirty_ = true;
  if (applied.commit_record.has_value()) {
    const ink::CanonicalHandoffIdentity identity{
        1U, pointerId, 1U, applied.commit_record->operation_id,
        applied.commit_record->commit_stamp,
        render::SurfaceGeneration{previewProvider_ == nullptr
                                      ? surface_.generation
                                      : previewProvider_->generation()}};
    if (canonicalVisibilitySink_ != nullptr &&
        canonicalVisibilitySink_->canonicalCommitted(identity, *applied.commit_record) ==
            ink::HandoffResult::kRejected) return false;
    lastCanonicalIdentity_ = identity;
    pendingCanonicalIdentities_[pointerId] = identity;
  }
  // The semantic commit is now authoritative; submit its scene immediately
  // so platform hosts do not need a second input event to complete the
  // canonical-visible handoff.
  // The Android presenter performs the first canonical frame when pixels are
  // requested; the semantic commit itself only records the handoff. This
  // keeps the PresentationTracker frame identity single-owner.
  brushSessions_.erase(it);
  brushSessionPackages_.erase(pointerId);
  brushSessionSeeds_.erase(pointerId);
  brushPreviews_.erase(pointerId);
  // Canonical visibility is emitted after the next successful canonical
  // present. Keep preview state alive until that receipt arrives.
  return true;
}

bool CanvasEditingRuntime::cancelBrushSession(std::uint64_t pointerId) noexcept {
  std::lock_guard lock(previewStateMutex_);
  const auto it = brushSessions_.find(pointerId);
  if (it == brushSessions_.end()) return false;
  it->second->cancel();
  brushSessions_.erase(it);
  brushSessionPackages_.erase(pointerId);
  brushSessionSeeds_.erase(pointerId);
  brushPreviews_.erase(pointerId);
  bool cleaned = true;
  if (arcPreviewSink_ != nullptr)
    cleaned = arcPreviewSink_->cancel(ink::PreviewIdentity{1U, pointerId, 1U}) !=
        ink::PreviewSubmitResult::kRejected && cleaned;
  if (previewController_ != nullptr && previewProvider_ != nullptr) {
    cleaned = previewController_->cancelSession(pointerId) && cleaned;
    cleaned = previewController_->retireSession(pointerId, previewProvider_->generation()) && cleaned;
    if (!platformPresentationDeferred_) cleaned = previewController_->renderIfDirty() && cleaned;
  }
  return cleaned;
}

std::vector<render::BrushRenderPoint> CanvasEditingRuntime::brushRenderPoints() const {
  auto result = committedBrushPoints_;
  for (const auto& [pointer, outline] : brushPreviews_) {
    (void)pointer;
    for (const auto& point : outline) {
      result.push_back({static_cast<float>(point.x), static_cast<float>(point.y),
                        1.0F, 0.0F, 0.7F, 1U});
    }
  }
  return result;
}

interaction::ContactDisposition CanvasEditingRuntime::pointerDisposition(
    const input::PointerKey& key) const noexcept {
  return coordinator_->disposition(key);
}

bool CanvasEditingRuntime::applyViewportNavigation(
    const interaction::ViewportNavigationSample& sample) noexcept {
  std::lock_guard lock(previewStateMutex_);
  if (viewportController_ == nullptr ||
      !viewportController_->applyNavigation(sample)) {
    return false;
  }

  const auto viewport = viewportController_->state();
  // Navigation is also a preview-surface transform invalidation.  Without
  // this update an already-retained amber outline keeps its pre-gesture
  // matrix until the next brush sample arrives, which makes the pinch appear
  // to be applied one input event late.
  if (previewController_ != nullptr && previewController_->active() &&
      !previewController_->updateViewport(viewport.scale,
                                           viewport.translationX,
                                           viewport.translationY)) {
    return false;
  }

  // Navigation changes the canonical camera even when no new semantic or
  // brush input arrives.  Treat it as an independent canonical-surface
  // invalidation so the Web/Android/Windows display cannot remain at the old
  // transform until the next pointer sample happens to trigger a frame.
  if (surface_.available && activeSurfaceProvider() != nullptr) {
    if (platformPresentationDeferred_) {
      canonicalPresentationDirty_ = true;
      return true;
    }
    return presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false);
  }
  return true;
}

bool CanvasEditingRuntime::setViewportZoomAt(float zoom, float anchorX,
                                          float anchorY) noexcept {
  std::lock_guard lock(previewStateMutex_);
  if (viewportController_ == nullptr || !std::isfinite(zoom) || zoom <= 0.0F ||
      !std::isfinite(anchorX) || !std::isfinite(anchorY)) {
    return false;
  }
  const auto current = viewportController_->state();
  if (current.scale <= 0.0F || !std::isfinite(current.scale)) return false;
  const float ratio = zoom / current.scale;
  const float translationX = anchorX - ratio * (anchorX - current.translationX);
  const float translationY = anchorY - ratio * (anchorY - current.translationY);
  if (!viewportController_->setCamera(zoom, translationX, translationY)) return false;
  const auto viewport = viewportController_->state();
  if (previewController_ != nullptr && previewController_->active() &&
      !previewController_->updateViewport(viewport.scale, viewport.translationX,
                                           viewport.translationY)) {
    return false;
  }
  if (surface_.available && activeSurfaceProvider() != nullptr) {
    if (platformPresentationDeferred_) {
      canonicalPresentationDirty_ = true;
      return true;
    }
    return presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false);
  }
  return true;
}

bool CanvasEditingRuntime::fitViewportToContent(
    const std::optional<foundation::WorldRect>& requestedTarget) noexcept {
  if (viewportController_ == nullptr || surface_.width == 0U || surface_.height == 0U) {
    return false;
  }
  std::optional<foundation::WorldRect> target = requestedTarget;
  if (!target.has_value() && runtimeSceneHost_ != nullptr) {
    const auto bounds = runtimeSceneHost_->publishedBounds();
    if (!runtimeSceneHost_->read().records().empty() && bounds.isFiniteAndOrdered()) {
      target = bounds;
    }
  }
  if (!target.has_value()) return false;
  const auto fit = interaction::computeViewportFit(
      target, surface_.logicalWidth, surface_.logicalHeight,
      0.0F, 1.0F);
  if (!fit.accepted) return false;
  const float translationX = surface_.logicalWidth * 0.5F - fit.zoom * fit.centerX;
  const float translationY = surface_.logicalHeight * 0.5F - fit.zoom * fit.centerY;
  if (!viewportController_->setCamera(fit.zoom, translationX, translationY)) return false;
  const auto viewport = viewportController_->state();
  if (previewController_ != nullptr && previewController_->active() &&
      !previewController_->updateViewport(viewport.scale, viewport.translationX,
                                          viewport.translationY)) {
    return false;
  }
  if (surface_.available && activeSurfaceProvider() != nullptr) {
    if (platformPresentationDeferred_) {
      canonicalPresentationDirty_ = true;
      return true;
    }
    return presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false);
  }
  return true;
}

bool CanvasEditingRuntime::fitViewportToSelection() noexcept {
  if (selection_.primary().isZero() || sceneCoordinator_ == nullptr) return false;
  const auto* record = sceneCoordinator_->runtimeScene().find(selection_.primary());
  if (record == nullptr) return false;
  return fitViewportToContent(record->worldBounds);
}

bool CanvasEditingRuntime::fitViewportToObject(foundation::ObjectId objectId) noexcept {
  if (objectId.isZero() || sceneCoordinator_ == nullptr) return false;
  const auto* record = sceneCoordinator_->runtimeScene().find(objectId);
  if (record == nullptr) return false;
  return fitViewportToContent(record->worldBounds);
}

bool CanvasEditingRuntime::commitStroke(std::uint64_t strokeId,
                                     std::uint64_t operationId) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || publicationCallbackActive_) return false;
  const auto committedPoints = preview_->snapshot().confirmed;
  const auto finished = ink_->finish();
  if (!finished.has_value()) return false;
  const auto operation = legacyStrokeOperation(*finished, operationId);
  if (!interaction_->commit(strokeId, interaction::OperationRequest{operationId, operation})) return false;
  if (!committedPoints.empty()) committedStrokes_.push_back(committedPoints);
  preview_->cancel();
  return true;
}

semantic::Operation CanvasEditingRuntime::legacyStrokeOperation(
    const ink::StrokeRecord& stroke, std::uint64_t operationId) const {
  semantic::ObjectRecord object{};
  object.id = foundation::ObjectId::fromUint64(stroke.id);
  object.kind = semantic::ObjectKind::kVectorStroke;
  object.kind_version = 1;
  // Legacy/native stroke commits share the same top drawing lane as the
  // BrushCommitAdapter.  Keeping this path in the old low lane makes a line
  // render underneath a subsequently created shape even though the current
  // brush path is ordered correctly.
  object.placement.order_key = semantic::OrderKey{{0xFFU,
      static_cast<std::uint8_t>(1U + (stroke.id % 254U))}};
  semantic::StrokeRecord content{};
  content.brush.brush_family_id = 1;
  content.brush.brush_version = 1;
  content.brush.nominal_size = 1;
  content.brush.opacity = 1;
  content.brush.color = {0, 0, 0, 1};
  semantic::VectorStrokeData data;
  for (const auto& point : stroke.points) data.samples.push_back({{point.x, point.y}, 1, {0, 0}});
  content.data = std::move(data);
  object.content = semantic::VectorStrokeContent{std::move(content)};
  return {semantic::OperationId{foundation::ObjectId::fromUint64(operationId)}, documentId_,
          1, 1, semantic::InsertObjectsOp{{std::move(object)}}};
}

interaction::SubmitResult CanvasEditingRuntime::submit(
    const interaction::OperationRequest& request) {
  if (!canonicalWriteAdmission())
    return interaction::SubmitResult::rejected();
  if (request.operationId == 0U) return interaction::SubmitResult::rejected();
  // Identity alone carries no semantic intent. Legacy Ink supplies its
  // actual confirmed VectorStroke; ordinary BrushSession uses its adapter.
  if (!request.operation.has_value()) return interaction::SubmitResult::rejected();
  const auto& operation = *request.operation;
  if (operation.document_id != documentId_ ||
      operation.id != semantic::OperationId(canvas::foundation::ObjectId::fromUint64(request.operationId)))
    return interaction::SubmitResult::rejected();
  const auto applied = operationEngine_.apply(
      operation, semantic::ApplySource::kLocalInteraction, semanticObjects_,
      appliedOperations_, semanticGeneration_, canonicalCommitClock_);
  if (applied.disposition != semantic::ApplyDisposition::kApplied &&
      applied.disposition != semantic::ApplyDisposition::kAlreadyApplied) {
    return interaction::SubmitResult::rejected();
  }
  // A duplicate operation is idempotently accepted by the semantic engine,
  // but it is not a new local document revision and must not advance the
  // host's product revision counter.
  if (applied.disposition == semantic::ApplyDisposition::kApplied) {
    if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr && sceneCompiler_ != nullptr) {
      const semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
      const canvas::SceneCommitInput input(applied.commit_record->before_generation,
          applied.commit_record->after_generation, view, &applied.commit_record->change_set);
      (void)sceneCoordinator_->apply(*sceneCompiler_, input);
    }
    ++submittedOperationCount_;
    observeApplied(operation, applied);
  }
    canonicalPresentationDirty_ = true;
  return interaction::SubmitResult::acceptedResult();
}

interaction::SubmitResult CanvasEditingRuntime::submitConditional(
    std::span<const semantic::Operation> operations, semantic::ApplySource source,
    std::span<const interaction::HistoryExpectation> expectations) {
  for(const auto& operation:operations)if(const auto* text=std::get_if<semantic::EditRichTextOp>(&operation.payload))
    if(!textWritable(text->object_id))return interaction::SubmitResult::rejected();
  if(!interaction::historyExpectationsMatch(semanticObjects_,expectations))
    return interaction::SubmitResult::rejected();
  // Resolve the actual deletion closure in this same owner phase. A remote
  // child or dependent connector that was not part of the captured postimage
  // makes compensation conflict, before any canonical apply or publication.
  for(const auto& operation:operations)if(operation.kind()==semantic::OperationKind::kDeleteObjects && !expectations.empty()) {
    const auto plan=operationEngine_.prepare(operation,{semanticObjects_,appliedOperations_,resourceManifestPort_});
    if(!plan.plan || plan.disposition!=semantic::PrepareDisposition::kPrepared)
      return interaction::SubmitResult::rejected();
    for(const auto id:plan.plan->deletes)
      if(std::none_of(expectations.begin(),expectations.end(),[id](const auto& item){
           return item.objectId==id && item.record.has_value();})) return interaction::SubmitResult::rejected();
  }
  return submit(operations,source);
}

semantic::OperationId CanvasEditingRuntime::allocateOperationId() {
  while (nextHistoryOperationOrdinal_ != 0U) {
    const auto id = semantic::OperationId(
        foundation::ObjectId::fromUint64(nextHistoryOperationOrdinal_++));
    if (!appliedOperations_.find(id).has_value()) return id;
  }
  return {};
}

std::uint64_t CanvasEditingRuntime::localOperationOrdinal() const noexcept {
  // Preserve the existing low-ID/order-key sequence for ordinary drawings;
  // the compatibility port can consume arbitrary identities independently.
  auto ordinal = static_cast<std::uint64_t>(submittedOperationCount_ + 1U);
  while (ordinal != 0U && ordinal < (std::uint64_t{1} << 63U)) {
    const auto id = foundation::ObjectId::fromUint64(ordinal);
    if (!appliedOperations_.find(semantic::OperationId(id)).has_value() &&
        !semanticObjects_.contains(id)) return ordinal;
    ++ordinal;
  }
  return 0U;
}

bool CanvasEditingRuntime::historySceneReady() const noexcept {
  return runtimeSceneHost_ != nullptr && sceneCoordinator_ != nullptr &&
         runtimeSceneHost_->semanticGeneration() == semanticGeneration_.current() &&
         sceneCoordinator_->runtimeScene().generation() == semanticGeneration_.current();
}

bool CanvasEditingRuntime::recoverHistoryScene() noexcept {
  if (historySceneReady()) return true;
  if (sceneCoordinator_ == nullptr || sceneCompiler_ == nullptr) return false;
  const semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
  const canvas::SceneCommitInput recovery(semanticGeneration_.current(), view);
  return static_cast<bool>(sceneCoordinator_->recover(*sceneCompiler_, recovery));
}

interaction::SubmitResult CanvasEditingRuntime::submit(
    std::span<const semantic::Operation> operations, semantic::ApplySource source) {
  if (!canonicalWriteAdmission())
    return interaction::SubmitResult::rejected();
  // This host records only one-operation compensation entries. Reject larger
  // batches before mutation; other EditorHistory owners may support them.
  if (operations.size() != 1U || source != semantic::ApplySource::kUndoRedo ||
      !historySceneReady()) return interaction::SubmitResult::rejected();
  for (const auto& operation : operations) {
    const auto applied = resourceManifestPort_
        ? operationEngine_.apply(operation, source, semanticObjects_, *resourceManifestPort_, appliedOperations_, semanticGeneration_, canonicalCommitClock_)
        : operationEngine_.apply(operation, source, semanticObjects_, appliedOperations_, semanticGeneration_, canonicalCommitClock_);
    if (applied.disposition != semantic::ApplyDisposition::kApplied) {
      return interaction::SubmitResult::rejected();
    }
    observeApplied(operation, applied);
    if (applied.commit_record.has_value() && sceneCoordinator_ != nullptr &&
        sceneCompiler_ != nullptr) {
      semantic::SemanticReadView view(semanticObjects_, semanticGeneration_.current());
      canvas::SceneCommitInput input(applied.commit_record->before_generation,
                                     applied.commit_record->after_generation, view,
                                     &applied.commit_record->change_set);
      (void)sceneCoordinator_->apply(*sceneCompiler_, input);
    }
    ++submittedOperationCount_;
  }
  if (!operations.empty()) {
    // Once Semantic has accepted, presentation/projection failure cannot
    // reject that mutation. The next frame recovers the derived Scene.
    if (!platformPresentationDeferred_) {
      (void)presentCanonicalFrame(nextCanonicalFrameId(), 0.0, false);
    }
  }
  return interaction::SubmitResult::acceptedResult();
}

void CanvasEditingRuntime::cancel(std::uint64_t) noexcept {
  ink_->cancel();
  preview_->cancel();
}

void CanvasEditingRuntime::recordPresentation(std::string evidenceKind,
                                            std::size_t pendingHandoffs,
                                            double frameMs) {
  hud_.presentEvidenceKind = std::move(evidenceKind);
  hud_.pendingHandoffCount = pendingHandoffs;
  hud_.frameMs = frameMs;
}

bool CanvasEditingRuntime::stageSurfaceProvider(
    std::string profileId, std::unique_ptr<render::SkiaSurfaceProvider> provider,
    bool preview) noexcept {
  if (closed_ || surfaceTransitionActive_ || std::this_thread::get_id() != ownerThread_ ||
      profileId.empty() || !provider) return false;
  auto info = provider->describe();
  info.profileId = std::move(profileId);
  if (info.metrics.physicalWidth == 0U || info.metrics.physicalHeight == 0U) return false;
  auto& pending = preview ? pendingPreviewProvider_ : pendingCanonicalProvider_;
  pending.emplace(std::move(info), std::move(provider));
  return true;
}

void CanvasEditingRuntime::publishSurfaceBinding() noexcept {
  const auto& snapshot = appBinding_->surfaces().lifecycle().current();
  const auto& metrics = snapshot.metrics;
  surface_ = SurfaceBinding{snapshot.surfaceGeneration.value(), snapshot.metricsGeneration.value(),
      metrics.logicalWidth, metrics.logicalHeight, metrics.physicalWidth,
      metrics.physicalHeight, appBinding_->surfaces().lifecycle().available(),
      metrics.devicePixelRatio, metrics.displayScale};
  lifecycle_ = std::make_unique<render::SurfaceLifecycle>(snapshot);
  tracker_ = std::make_unique<render::PresentationTracker>(*lifecycle_);
  frameCoordinator_.invalidate(render::FrameRole::kCanonical);
  frameCoordinator_.invalidate(render::FrameRole::kPreview);
  lastSubmittedCanonicalFrame_.reset();
  lastSubmittedCanonicalFrameState_.reset();
  lastPresentedCanonicalFrame_.reset();
  lastPresentedCanonicalFrameState_.reset();
  presentedSurfaceGeneration_ = {};
  presentedMetricsGeneration_ = {};
  canonicalPresentationDirty_ = true;
}

bool CanvasEditingRuntime::commitSurfaceBinding(
    const render::SurfaceMetrics& metrics, bool restoring) noexcept {
  if (closed_ || surfaceTransitionActive_ || std::this_thread::get_id() != ownerThread_ ||
      appBinding_ == nullptr || !std::isfinite(metrics.logicalWidth) ||
      !std::isfinite(metrics.logicalHeight) || metrics.logicalWidth <= 0.0F ||
      metrics.logicalHeight <= 0.0F || metrics.physicalWidth == 0U ||
      metrics.physicalHeight == 0U || !std::isfinite(metrics.devicePixelRatio) ||
      metrics.devicePixelRatio <= 0.0F || !std::isfinite(metrics.displayScale) ||
      metrics.displayScale <= 0.0F) return false;
  const bool unchanged = !restoring && !hasStagedSurfaceProviders() && surface_.available &&
      metrics == appBinding_->surfaces().lifecycle().current().metrics;
  if (unchanged) return true;
  if (appBinding_->surfaces().validateUpdate(metrics, pendingCanonicalProvider_) !=
      render::SurfaceProviderDisposition::kCommitted ||
      (appBinding_->previewSurfaces() &&
       appBinding_->previewSurfaces()->validateUpdate(metrics, pendingPreviewProvider_) !=
           render::SurfaceProviderDisposition::kCommitted)) return false;
  if (editingPointer_.valid()) cancelSelectionTransform();
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard stateLock(previewStateMutex_);
  surfaceTransitionActive_ = true;
  surfaceAbortRequested_ = false;
  std::optional<render::PreviewGeometry> retainedPreview;
  if (previewController_ && previewController_->active()) retainedPreview = previewController_->geometry();
  const auto previewStyle = previewController_ ? previewController_->presentationStyle()
                                               : render::PreviewStyleOverride{};
  // The fallback is also a separate provider. Its initial prepared generation
  // is retained; no repeated resize is used to manufacture an identity.
  if (appBinding_->previewSurfaces() == nullptr) {
    auto provider = std::make_unique<render::TransparentOverlaySkiaSurfaceProvider>();
    if (provider->resize(metrics.physicalWidth, metrics.physicalHeight).code !=
        render::BackendSubmissionCode::kAccepted) {
      surfaceTransitionActive_ = false;
      return false;
    }
    auto info = provider->describe();
    info.profileId = "runtime-preview";
    if (appBinding_->registerPreviewSurfaceProfile({info, std::move(provider)}) !=
        render::SurfaceProviderDisposition::kCommitted) {
      surfaceTransitionActive_ = false;
      return false;
    }
  }
  const auto result = appBinding_->updateSurfacePair(metrics,
      pendingCanonicalProvider_, pendingPreviewProvider_, restoring, surface_.generation == 0U,
      [this] { return !closed_ && !surfaceAbortRequested_; });
  surfaceTransitionActive_ = false;
  if (result != render::SurfaceProviderDisposition::kCommitted) {
    pendingCanonicalProvider_.reset();
    pendingPreviewProvider_.reset();
    if (surfaceAbortRequested_ || closed_) appBinding_->invalidateSurfacePair();
    if (!appBinding_->surfaces().lifecycle().available()) {
      surface_.available = false;
      if (lifecycle_) (void)lifecycle_->markLost(viewId_);
      frameCoordinator_.invalidate(render::FrameRole::kCanonical);
      frameCoordinator_.invalidate(render::FrameRole::kPreview);
      lastSubmittedCanonicalFrame_.reset();
      lastSubmittedCanonicalFrameState_.reset();
    }
    return false;
  }
  resetTouchTextTap();
  pendingCanonicalProvider_.reset();
  pendingPreviewProvider_.reset();
  auto* nextPreview = appBinding_->previewSurfaces()->activeProvider();
  if (previewProvider_ != nextPreview || !previewController_) {
    previewProvider_ = nextPreview;
    previewController_ = std::make_unique<render::PreviewSurfaceController>(skiaRenderer_, *previewProvider_);
    previewController_->setPresentationDeferred(platformPresentationDeferred_);
    previewController_->setPresentationStyle(previewStyle);
    if (retainedPreview) previewController_->retainGeometry(*retainedPreview);
  } else if (previewController_->active()) {
    (void)previewController_->rebind(previewProvider_->generation());
  }
  previewController_->setSurfaceMetrics(metrics);
  publishSurfaceBinding();
  return true;
}

bool CanvasEditingRuntime::bindSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return bindSurface(render::SurfaceMetrics{static_cast<float>(width),
      static_cast<float>(height), width, height, 1.0F, 1.0F});
}

bool CanvasEditingRuntime::bindSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (!commitSurfaceBinding(metrics, false)) return false;
  ++resizeEvents_;
  return true;
}

bool CanvasEditingRuntime::resizeSurface(std::uint32_t width, std::uint32_t height) noexcept {
  return resizeSurface(render::SurfaceMetrics{static_cast<float>(width),
      static_cast<float>(height), width, height, 1.0F, 1.0F});
}

bool CanvasEditingRuntime::resizeSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (!surface_.available || !commitSurfaceBinding(metrics, false)) return false;
  ++resizeEvents_;
  return true;
}

bool CanvasEditingRuntime::loseSurface() noexcept {
  if (closed_ || std::this_thread::get_id() != ownerThread_) return false;
  resetTouchTextTap();
  if (surfaceTransitionActive_) {
    surfaceAbortRequested_ = true;
    surface_.available = false;
    return true;
  }
  if (editingPointer_.valid()) cancelSelectionTransform();
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard stateLock(previewStateMutex_);
  appBinding_->invalidateSurfacePair();
  pendingCanonicalProvider_.reset();
  pendingPreviewProvider_.reset();
  surface_.available = false;
  (void)lifecycle_->markLost(viewId_);
  frameCoordinator_.invalidate(render::FrameRole::kCanonical);
  frameCoordinator_.invalidate(render::FrameRole::kPreview);
  lastSubmittedCanonicalFrame_.reset();
  lastSubmittedCanonicalFrameState_.reset();
  ++surfaceLostEvents_;
  return true;
}

bool CanvasEditingRuntime::rebindSurface() noexcept {
  if (!appBinding_) return false;
  return rebindSurface(appBinding_->surfaces().lifecycle().current().metrics);
}

bool CanvasEditingRuntime::rebindSurface(const render::SurfaceMetrics& metrics) noexcept {
  if (!commitSurfaceBinding(metrics, true)) return false;
  ++rebindEvents_;
  return true;
}

bool CanvasEditingRuntime::adoptPreparedSurface(const render::SurfaceMetrics& metrics) noexcept {
  return commitSurfaceBinding(metrics, !surface_.available);
}

bool CanvasEditingRuntime::rebindPreviewSurface() noexcept {
  std::lock_guard providerLock(previewProviderMutex_);
  std::lock_guard lock(previewStateMutex_);
  if (appBinding_ == nullptr || previewProvider_ == nullptr ||
      appBinding_->rebindPreviewSurface() !=
          render::SurfaceProviderDisposition::kCommitted) {
    return false;
  }
  return previewController_ == nullptr || !previewController_->active() ||
         previewController_->rebind(previewProvider_->generation());
}

bool CanvasEditingRuntime::registerSurfaceProvider(
    std::string profileId, std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  return stageSurfaceProvider(std::move(profileId), std::move(provider), false);
}

bool CanvasEditingRuntime::selectCanonicalSurfaceProfile(
    std::string_view profileId, std::uint64_t expectedGeneration,
    render::RenderTargetFormat format) noexcept {
  if (appBinding_ == nullptr || profileId.empty()) return false;
  if (appBinding_->surfaces().lifecycle().current().surfaceGeneration.value() !=
      expectedGeneration) return false;
  if (editingPointer_.valid()) cancelSelectionTransform();
  if (appBinding_->selectRenderProfile(profileId, format) !=
      render::SurfaceProviderDisposition::kCommitted) return false;
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.metricsGeneration = appBinding_->surfaces().lifecycle().current().metricsGeneration.value();
  const auto& accepted = appBinding_->surfaces().activeInfo().metrics;
  surface_.width = accepted.physicalWidth;
  surface_.height = accepted.physicalHeight;
  surface_.logicalWidth = accepted.logicalWidth;
  surface_.logicalHeight = accepted.logicalHeight;
  surface_.devicePixelRatio = accepted.devicePixelRatio;
  surface_.displayScale = accepted.displayScale;
  surface_.available = true;
  return true;
}

bool CanvasEditingRuntime::rebindCanonicalSurface(std::uint64_t expectedGeneration) noexcept {
  if (appBinding_ == nullptr ||
      appBinding_->surfaces().lifecycle().current().surfaceGeneration.value() !=
          expectedGeneration) return false;
  if (editingPointer_.valid()) cancelSelectionTransform();
  if (appBinding_->rebindSurface() != render::SurfaceProviderDisposition::kCommitted) return false;
  surface_.generation = appBinding_->surfaces().lifecycle().current().surfaceGeneration.value();
  surface_.metricsGeneration = appBinding_->surfaces().lifecycle().current().metricsGeneration.value();
  surface_.available = true;
  return true;
}

bool CanvasEditingRuntime::registerPreviewSurfaceProvider(
    std::string profileId, std::unique_ptr<render::SkiaSurfaceProvider> provider) noexcept {
  return stageSurfaceProvider(std::move(profileId), std::move(provider), true);
}

render::SkiaSurfaceProvider* CanvasEditingRuntime::activeSurfaceProvider() noexcept {
  return appBinding_ == nullptr ? nullptr : appBinding_->surfaces().activeProvider();
}

render::SkiaSurfaceProvider* CanvasEditingRuntime::previewSurfaceProvider() noexcept {
  return previewProvider_;
}

render::CanonicalViewportTransform CanvasEditingRuntime::previewViewport() const noexcept {
  return previewController_ == nullptr
      ? render::CanonicalViewportTransform{}
      : previewController_->geometry().viewport;
}

const render::SurfaceRenderState& CanvasEditingRuntime::previewRenderState() const noexcept {
  static const render::SurfaceRenderState unavailable{};
  return previewController_ == nullptr ? unavailable : previewController_->state();
}

std::uint64_t CanvasEditingRuntime::previewSurfaceGeneration() const noexcept {
  return previewProvider_ == nullptr ? 0U : previewProvider_->generation();
}

std::uint64_t CanvasEditingRuntime::previewSubmissionCount() const noexcept {
  return previewProvider_ == nullptr ? 0U : previewProvider_->presentCount();
}

std::uint64_t CanvasEditingRuntime::previewPresentCount() const noexcept {
  return previewProvider_ == nullptr ? 0U : previewProvider_->presentCount();
}

bool CanvasEditingRuntime::previewActive() const noexcept {
  return previewController_ != nullptr && previewController_->active();
}

bool CanvasEditingRuntime::presentCanonicalFrame(std::uint64_t frameId,
                                              double frameMs,
                                              bool retirePreview) noexcept {
  canonicalPresentationRetryable_ = false;
  AXIOM_ANDROID_DIAG("present begin frame=%llu objects=%zu scene_records=%zu", static_cast<unsigned long long>(frameId), semanticObjects_.size(), sceneCoordinator_ == nullptr ? 0U : sceneCoordinator_->runtimeScene().records().size());
  if (!surface_.available || frameId == 0U) { AXIOM_ANDROID_DIAG("present reject surface=%d frame=%llu", surface_.available ? 1 : 0, static_cast<unsigned long long>(frameId)); return false; }
  if (frameIdHighWater_ == std::numeric_limits<std::uint64_t>::max()) return false;
  if (frameId <= frameIdHighWater_) return false;
  frameIdHighWater_ = frameId;
  if (runtimeSceneHost_ == nullptr || sceneCoordinator_ == nullptr ||
      activeSurfaceProvider() == nullptr) { AXIOM_ANDROID_DIAG("present reject missing deps"); return false; }
  const auto semanticGeneration = semanticGeneration_.current();
  if (!recoverHistoryScene()) { AXIOM_ANDROID_DIAG("present reject scene generation"); return false; }
  const auto viewport = viewportController_->state();
  const float zoom = viewport.scale > 0.0F && std::isfinite(viewport.scale)
      ? viewport.scale : 1.0F;
  const float centerX = surface_.logicalWidth * 0.5F;
  const float centerY = surface_.logicalHeight * 0.5F;
  // Visibility queries are expressed in world space, while the platform
  // surface bounds are view-space pixels.  Convert all four view corners
  // through the inverse camera transform before querying the Scene.  Using
  // {0,0,width,height} here silently drops strokes whose entire world-space
  // bounds are outside that screen-space rectangle (the Android "only lines
  // crossing the centre become canonical" symptom).
  const auto viewToWorld = [&](float viewX, float viewY) {
    return foundation::WorldPoint{
        (viewX - viewport.translationX) / zoom,
        (viewY - viewport.translationY) / zoom};
  };
  const std::array<foundation::WorldPoint, 4> worldCorners{
      viewToWorld(0.0F, 0.0F), viewToWorld(surface_.logicalWidth, 0.0F),
      viewToWorld(0.0F, surface_.logicalHeight),
      viewToWorld(surface_.logicalWidth, surface_.logicalHeight)};
  foundation::WorldRect worldViewport{worldCorners[0].x, worldCorners[0].y,
                                      worldCorners[0].x, worldCorners[0].y};
  for (std::size_t i = 1; i < worldCorners.size(); ++i) {
    worldViewport.left = std::min(worldViewport.left, worldCorners[i].x);
    worldViewport.top = std::min(worldViewport.top, worldCorners[i].y);
    worldViewport.right = std::max(worldViewport.right, worldCorners[i].x);
    worldViewport.bottom = std::max(worldViewport.bottom, worldCorners[i].y);
  }
  const render::FrameState frame{
      viewId_,
      render::CameraState{foundation::WorldPoint{
                              (centerX - viewport.translationX) / zoom,
                              (centerY - viewport.translationY) / zoom},
                          zoom, 0.0F,
                              render::CameraGeneration{cameraGeneration()}},
      // ReferenceDrawList's clip is a view-space contract in the current
      // renderer authority. Keep it covering the physical target; the
      // camera above carries the actual pinch transform.
      worldViewport,
       render::SurfaceMetrics{surface_.logicalWidth, surface_.logicalHeight,
                             surface_.width, surface_.height,
                             surface_.devicePixelRatio, surface_.displayScale},
      semanticGeneration, runtimeSceneHost_->revision(),
      render::SurfaceGeneration{surface_.generation},
       render::MetricsGeneration{surface_.metricsGeneration}, render::FrameId{frameId},
      foundation::WorldRect{0.0F, 0.0F, surface_.logicalWidth,
                            surface_.logicalHeight}};
  const render::RuntimeFrameCapture canonicalCapture{
      {frame.viewId, frame.frameId, frame.sceneReadToken.value(),
       frame.sceneGeneration.value(), frame.surfaceGeneration, frame.metricsGeneration},
      frame.metrics, render::FrameRole::kCanonical};
  auto visibility = render::VisibilityResolver::resolve(frame, *runtimeSceneHost_);
  if (!visibility) { AXIOM_ANDROID_DIAG("present visibility failed"); return false; }
  // A selected object can be dragged into view from outside the canonical
  // visibility set. Extend only this view's transient traversal, never Scene.
  const auto transientObject = imageCropPresentation_ ? imageCropPresentation_->id : selection_.primary();
  if ((transformDrag_.active() || imageCropPresentation_) &&
      std::find(visibility.value().backToFront.begin(),visibility.value().backToFront.end(),
                transientObject)==visibility.value().backToFront.end() &&
      sceneCoordinator_->runtimeScene().find(transientObject)) {
    auto& ids=visibility.value().backToFront;
    ids.push_back(transientObject);
    std::sort(ids.begin(),ids.end(),[&](auto a,auto b) {
      return runtimeTraversalLess(*sceneCoordinator_->runtimeScene().find(a),
                                  *sceneCoordinator_->runtimeScene().find(b));
    });
    visibility.value().visibleRecords=ids.size();
  }
  auto references = render::DirectReferenceSource::build(
      frame, visibility.value(), sceneCoordinator_->runtimeScene());
  if (!references) { AXIOM_ANDROID_DIAG("present references failed"); return false; }
  // The copy belongs to this frame only. Canonical storage and the shared
  // RuntimeScene retain their original records throughout a drag.
  for (auto& entry: references.value().entries) {
    if (const auto* transform=transformOverrides_.find(entry.record.objectId)) {
      entry.record.transform=*transform;
      if (textLayoutService_) {
        if (const auto* shape = std::get_if<semantic::ShapeContent>(&entry.record.content)) {
          const auto layout = textLayoutService_->resolveShape(
              entry.record.objectId, *shape, entry.record.transform,
              entry.record.objectId == selection_.primary());
          entry.record.textLayout = layout;
          entry.command = render::ShapeReferenceCommand{*shape, layout,
              entry.record.objectId == selection_.primary()};
        }
      }
      // Canonical digest/encoding does not describe transient editor pixels.
      references.value().canonicalBytes.clear();
      references.value().digest.clear();
    }
  }

  const auto textState=interaction_->textSession()?interaction_->textSession()->read():runtime::TextEditorSnapshot{};
  foundation::ObjectId placeholderTarget = selection_.primary();
  if (textState.phase != runtime::TextPhase::kClosed && !textState.token.object.isZero())
    placeholderTarget = textState.token.object;
  if (!placeholderTarget.isZero() && textLayoutService_) {
    const auto selected = std::find_if(references.value().entries.begin(), references.value().entries.end(),
        [&](const auto& entry) { return entry.record.objectId == placeholderTarget; });
    if (selected != references.value().entries.end()) {
      const auto* shape = std::get_if<semantic::ShapeContent>(&selected->record.content);
      if (shape != nullptr) {
        const semantic::RichTextContent empty{};
        const auto& label = shape->label ? *shape->label : empty;
        const auto layout = textLayoutService_->resolveShapeLabel(
            selected->record.objectId, label, *shape, selected->record.transform, true);
        selected->record.textLayout = layout;
        selected->command = render::ShapeReferenceCommand{*shape, layout, true};
        references.value().canonicalBytes.clear();
        references.value().digest.clear();
      }
    }
  }
  if (interaction_->textSession() && textLayoutService_) {
    if (const auto object = textPresentation()) {
      const semantic::RichTextContent* content = std::get_if<semantic::RichTextContent>(&object->content);
      const semantic::ShapeContent* shape = std::get_if<semantic::ShapeContent>(&object->content);
      if (shape != nullptr && shape->label.has_value()) content = &*shape->label;
      if (content != nullptr) {
        const auto layout = shape != nullptr
            ? textLayoutService_->resolveShapeLabel(object->id, *content, *shape,
                                                     object->transform, true)
            : textLayoutService_->resolve(object->id, *content);
        if (layout->diagnostics.empty()) {
          render::ReferenceTraversalEntry entry{};
          entry.record.objectId = object->id;
          entry.record.kind = object->kind;
          entry.record.kindVersion = object->kind_version;
          entry.record.transform = object->transform;
          entry.record.properties = object->properties;
          entry.record.eraseMasks = object->erase_masks;
          entry.record.derivedGeometry = canvas::scene::deriveGeometry(*object);
          entry.record.content = object->content;
          entry.record.textLayout = layout;
          entry.record.placement = object->placement;
          const auto bounds = canvas::scene::computeBounds(
              *object, object->kind == semantic::ObjectKind::kRichText ? layout.get() : nullptr);
          entry.record.geometryBounds = bounds.geometry;
          entry.record.visualBounds = bounds.visual;
          entry.record.worldBounds = bounds.world;
          if (shape != nullptr && shape->label.has_value())
            entry.command = render::ShapeReferenceCommand{*shape, layout, true};
          else
            entry.command = render::RichTextReferenceCommand{*content, layout};
          auto& entries = references.value().entries;
          const auto existing = std::find_if(entries.begin(), entries.end(),
              [&](const auto& value) { return value.record.objectId == object->id; });
          if (existing != entries.end()) *existing = std::move(entry);
          else entries.push_back(std::move(entry));
          references.value().canonicalBytes.clear();
          references.value().digest.clear();
        }
      }
    }
  }
  if (imageCropPresentation_) {
    for (auto& entry : references.value().entries) {
      if (entry.record.objectId != imageCropPresentation_->id) continue;
      entry.record.content = imageCropPresentation_->content;
      const auto bounds = canvas::scene::computeBounds(*imageCropPresentation_);
      entry.record.geometryBounds = bounds.geometry;
      entry.record.visualBounds = bounds.visual;
      entry.record.worldBounds = bounds.world;
      entry.command = render::ImageReferenceCommand{
          std::get<semantic::ImageContent>(imageCropPresentation_->content), true};
      references.value().canonicalBytes.clear();
      references.value().digest.clear();
    }
  }
  const auto plan = render::FramePlanBuilder::build(frame, references.value());
  if (!plan) { AXIOM_ANDROID_DIAG("present plan failed"); return false; }
  if (selectionMode_) (void)renderSelectionOverlay();
  if (creationActive() && creationPreview().valid) {
    selectionView_ = std::make_unique<render::RenderViewRuntime>(frame);
    std::vector<render::SnapGuideGeometry> guides;
    for (const auto& guide : creationPreview().guides) {
      const auto screen = [&](float x, float y) {
        return render::ScreenPoint{x * zoom + viewport.translationX, y * zoom + viewport.translationY};
      };
      if (guide.axis == interaction::SnapGuideAxis::kVertical)
        guides.push_back({render::SnapGuideAxis::kVertical, screen(guide.position, guide.spanStart),
                         screen(guide.position, guide.spanEnd)});
      else guides.push_back({render::SnapGuideAxis::kHorizontal, screen(guide.spanStart, guide.position),
                            screen(guide.spanEnd, guide.position)});
    }
    selectionView_->editingOverlay().setCreationPreview(creationPreview().record, guides);
  } else if (!selectionMode_) selectionView_.reset();
  if(textState.phase!=runtime::TextPhase::kClosed){
    selectionView_=std::make_unique<render::RenderViewRuntime>(frame);
    const auto geometry=textGeometry({1,viewId_.value(),surface_.metricsGeneration,surface_.generation});
    if(geometry.available){selectionView_->editingOverlay().textCaret=geometry.caret;selectionView_->editingOverlay().textSelection=geometry.selection;}
    selectionView_->editingOverlay().textCaretVisible = textCaretVisible_;
  }
  const auto rendered = skiaRenderer_.renderFrame(*activeSurfaceProvider(), plan.value(),
                                                  selectionOverlay());
  if (rendered.code != render::BackendSubmissionCode::kAccepted) {
    canonicalPresentationRetryable_ = rendered.code == render::BackendSubmissionCode::kRetry;
    if (canonicalPresentationRetryable_) canonicalPresentationDirty_ = true;
    AXIOM_ANDROID_DIAG("present skia failed: %s framegen=%llu providergen=%llu",
                       rendered.message.c_str(),
                       static_cast<unsigned long long>(frame.surfaceGeneration.value()),
                       static_cast<unsigned long long>(activeSurfaceProvider()->generation()));
    return false;
  }
  // Publish the replacement identity only after the provider accepts it. A
  // busy backbuffer must not invalidate feedback for the last submitted
  // frame; successful newer submissions retain normal supersession rules.
  if (frameCoordinator_.request(canonicalCapture) != render::RuntimeFrameDisposition::kAccepted)
    return false;
  if (frameCoordinator_.submit(render::FrameRole::kCanonical, frame.frameId, false) !=
      render::RuntimeFrameDisposition::kAccepted) {
    AXIOM_ANDROID_DIAG("frame coordinator rejected canonical submit");
    return false;
  }
  const auto trackerSubmit = tracker_->submit(frame);
  if (trackerSubmit != render::PresentFeedbackDisposition::kSubmitted) { AXIOM_ANDROID_DIAG("present tracker submit failed code=%d framegen=%llu livegen=%llu metrics=%ux%u", static_cast<int>(trackerSubmit), static_cast<unsigned long long>(frame.surfaceGeneration.value()), static_cast<unsigned long long>(lifecycle_->current().surfaceGeneration.value()), frame.metrics.physicalWidth, frame.metrics.physicalHeight); return false; }
  // The renderer/provider submission is intentionally only a local submit
  // observation.  Physical presentation is reported later by the platform
  // through receivePlatformPresented(), with the exact frame identity and
  // independent PresentedFeedback evidence.  Keep the submitted frame bound
  // so stale or unrelated feedback cannot qualify it.
  lastSubmittedCanonicalFrame_ = canonicalCapture.identity;
  lastSubmittedCanonicalFrameState_ = std::make_unique<render::FrameState>(frame);
  recordPresentation("canonical-submitted", pendingCanonicalIdentities_.size(), frameMs);
  ++canonicalFrameCount_;
  canonicalPresentationDirty_ = false;
  // Creation/selection overlays have no ARC handoff identity. Once their
  // replacement canonical frame has been accepted locally, retire them
  // immediately; brush previews remain gated by platform-qualified feedback
  // through pendingCanonicalIdentities_.
  if (pendingCanonicalIdentities_.empty() && transientOverlayHandoffPending_ &&
      !creationActive() && !selectionTransformActive()) {
    (void)clearTransientOverlay();
  }
  // The deterministic local raster host has no separate platform feedback
  // channel. Preserve its established synchronous behavior while deferred
  // platform presenters remain gated by receivePlatformPresented().
  if (!platformPresentationDeferred_ && retirePreview &&
      !pendingCanonicalIdentities_.empty() && previewController_ != nullptr &&
      previewProvider_ != nullptr) {
    for (const auto& [pointer, handoff] : pendingCanonicalIdentities_) {
      static_cast<void>(pointer);
      (void)previewController_->cancelSession(handoff.session);
      (void)previewController_->retireSession(handoff.session, previewProvider_->generation());
    }
    pendingCanonicalIdentities_.clear();
    previewProvider_->setOverlayVisible(false);
  }
  // `retirePreview` remains source-compatible for existing callers, but it is
  // intentionally ignored here.  RuntimeFrameCoordinator can only qualify a
  // handoff after receivePlatformPresented() supplies matching platform facts.
  (void)retirePreview;
  return true;
}

std::optional<render::RuntimeFrameIdentity>
CanvasEditingRuntime::lastSubmittedCanonicalFrame() const noexcept {
  return lastSubmittedCanonicalFrame_;
}

bool CanvasEditingRuntime::receivePlatformPresented(
    const render::PresentedFeedback& feedback) noexcept {
  if (std::this_thread::get_id() != ownerThread_ || lifecycle_ == nullptr ||
      tracker_ == nullptr || frameCoordinator_.capture(render::FrameRole::kCanonical) == nullptr ||
      !lastSubmittedCanonicalFrame_.has_value()) {
    return false;
  }
  const auto& identity = *lastSubmittedCanonicalFrame_;
  if (feedback.viewId != identity.viewId || feedback.frameId != identity.frameId ||
      feedback.surfaceGeneration != identity.surfaceGeneration ||
      feedback.metricsGeneration != identity.metricsGeneration) {
    return false;
  }
  if (tracker_->receive(feedback) != render::PresentFeedbackDisposition::kPresented) {
    return false;
  }
  if (frameCoordinator_.receive({identity, render::FrameRole::kCanonical, true,
                                 render::RuntimePresentEvidence::kPlatformQualified}) !=
      render::RuntimeFrameDisposition::kAccepted) {
    return false;
  }
  const auto* capture = frameCoordinator_.capture(render::FrameRole::kCanonical);
  if (capture == nullptr) return false;
  const auto canonical = capture->identity;
  lastPresentedCanonicalFrame_ = canonical;
  lastPresentedCanonicalFrameState_ =
      std::make_unique<render::FrameState>(*lastSubmittedCanonicalFrameState_);
  recordPresentation("canonical-presented-platform-qualified", pendingCanonicalIdentities_.size(), 0.0);
  presentedSemanticGeneration_ = semantic::SemanticGeneration{canonical.sceneGeneration};
  presentedSurfaceGeneration_ = canonical.surfaceGeneration;
  presentedMetricsGeneration_ = canonical.metricsGeneration;
  bool brushPreviewQualified = false;
  {
    std::lock_guard providerLock(previewProviderMutex_);
    std::lock_guard previewLock(previewStateMutex_);
    if (previewController_ != nullptr && previewController_->active()) {
      const auto& previewState = previewController_->state();
      brushPreviewQualified = !previewState.dirty &&
          previewState.presentCount != 0U &&
          previewState.platformQualifiedRevision == previewState.contentRevision;
    }
  }
  const bool retire = frameCoordinator_.retirePreview(canonical) ||
      brushPreviewQualified;
  if (retire && semanticObjects_.size() != 0U) {
    std::lock_guard providerLock(previewProviderMutex_);
    std::lock_guard previewLock(previewStateMutex_);
    if (previewController_ != nullptr && previewController_->active()) {
      std::vector<std::uint64_t> retired;
      for (const auto& [pointer, handoffIdentity] : pendingCanonicalIdentities_) {
        if (canonicalVisibilitySink_ != nullptr) {
          const auto handoff = canonicalVisibilitySink_->canonicalVisible(
              handoffIdentity, *lastSubmittedCanonicalFrameState_);
          if (handoff == ink::HandoffResult::kRejected) return false;
          if (handoff != ink::HandoffResult::kAccepted) continue;
        }
        if (!previewController_->retireSession(
                handoffIdentity.session, previewProvider_->generation())) {
          return false;
        }
        retired.push_back(pointer);
      }
      for (const auto pointer : retired) {
        pendingCanonicalIdentities_.erase(pointer);
      }
    }
  }
  // Selection transforms commit through the semantic history lane and do not
  // create a brush/ARC handoff identity. Retire their last transient frame
  // only after this canonical frame has been accepted, so the compositor
  // never exposes the old canonical position between pointer-up and redraw.
  if (retire && !selectionTransformActive() && !creationActive()) {
    (void)clearTransientOverlay();
  }
  if (retire && transientOverlayHandoffPending_ && pendingCanonicalIdentities_.empty()) {
    (void)clearTransientOverlay();
  }
  lastSubmittedCanonicalFrame_.reset();
  lastSubmittedCanonicalFrameState_.reset();
  return true;
}

bool CanvasEditingRuntime::presentTransientOverlayFrame(std::uint64_t frameId) noexcept {
  std::lock_guard renderLock(previewRenderMutex_);
  std::lock_guard providerLock(previewProviderMutex_);
  transientOverlayDirty_ = true;
  if (!surface_.available || frameId == 0U || previewProvider_ == nullptr ||
      runtimeSceneHost_ == nullptr || sceneCoordinator_ == nullptr ||
      (!creationActive() && !selectionTransformActive() && selectionOverlay() == nullptr)) {
    return false;
  }
  if (frameIdHighWater_ == std::numeric_limits<std::uint64_t>::max()) return false;
  if (frameId <= frameIdHighWater_) return false;
  frameIdHighWater_ = frameId;
  if (!recoverHistoryScene()) return false;
  const auto viewport = viewportController_->state();
  const float zoom = viewport.scale > 0.0F && std::isfinite(viewport.scale)
      ? viewport.scale : 1.0F;
  const float centerX = surface_.logicalWidth * 0.5F;
  const float centerY = surface_.logicalHeight * 0.5F;
  const auto viewToWorld = [&](float viewX, float viewY) {
    return foundation::WorldPoint{
        (viewX - viewport.translationX) / zoom,
        (viewY - viewport.translationY) / zoom};
  };
  const std::array<foundation::WorldPoint, 4> worldCorners{
      viewToWorld(0.0F, 0.0F),
      viewToWorld(surface_.logicalWidth, 0.0F),
      viewToWorld(0.0F, surface_.logicalHeight),
      viewToWorld(surface_.logicalWidth, surface_.logicalHeight)};
  foundation::WorldRect worldViewport{worldCorners[0].x, worldCorners[0].y,
                                      worldCorners[0].x, worldCorners[0].y};
  for (std::size_t i = 1; i < worldCorners.size(); ++i) {
    worldViewport.left = std::min(worldViewport.left, worldCorners[i].x);
    worldViewport.top = std::min(worldViewport.top, worldCorners[i].y);
    worldViewport.right = std::max(worldViewport.right, worldCorners[i].x);
    worldViewport.bottom = std::max(worldViewport.bottom, worldCorners[i].y);
  }
  const render::FrameState frame{
      viewId_,
      render::CameraState{foundation::WorldPoint{
                              (centerX - viewport.translationX) / zoom,
                              (centerY - viewport.translationY) / zoom},
                          zoom, 0.0F,
                          render::CameraGeneration{cameraGeneration()}},
      worldViewport,
       render::SurfaceMetrics{surface_.logicalWidth, surface_.logicalHeight,
                              surface_.width, surface_.height,
                              surface_.devicePixelRatio,
                              surface_.displayScale},
      semanticGeneration_.current(), runtimeSceneHost_->revision(),
      render::SurfaceGeneration{surface_.generation},
       render::MetricsGeneration{surface_.metricsGeneration}, render::FrameId{frameId},
      foundation::WorldRect{0.0F, 0.0F, surface_.logicalWidth,
                            surface_.logicalHeight}};
  const render::RuntimeFrameCapture previewCapture{
      {frame.viewId, frame.frameId, frame.sceneReadToken.value(),
       frame.sceneGeneration.value(), frame.surfaceGeneration, frame.metricsGeneration},
      frame.metrics, render::FrameRole::kPreview};
  if (frameCoordinator_.request(previewCapture) != render::RuntimeFrameDisposition::kAccepted) return false;
  auto visibility = render::VisibilityResolver::resolve(frame, *runtimeSceneHost_);
  if (!visibility) return false;
  if (transformDrag_.active() &&
      std::find(visibility.value().backToFront.begin(), visibility.value().backToFront.end(),
                selection_.primary()) == visibility.value().backToFront.end()) {
    auto& ids = visibility.value().backToFront;
    ids.push_back(selection_.primary());
    std::sort(ids.begin(), ids.end(), [&](auto a, auto b) {
      return runtimeTraversalLess(*sceneCoordinator_->runtimeScene().find(a),
                                  *sceneCoordinator_->runtimeScene().find(b));
    });
    visibility.value().visibleRecords = ids.size();
  }
  auto references = render::DirectReferenceSource::build(
      frame, visibility.value(), sceneCoordinator_->runtimeScene());
  if (!references) return false;
  for (auto& entry : references.value().entries) {
    const auto* transform = transformOverrides_.find(entry.record.objectId);
    const bool selected = entry.record.objectId == selection_.primary();
    if (transform) entry.record.transform = *transform;
    if (transform || selected) {
      if (textLayoutService_) {
        if (const auto* shape = std::get_if<semantic::ShapeContent>(&entry.record.content)) {
          const auto layout = textLayoutService_->resolveShape(
              entry.record.objectId, *shape, entry.record.transform, selected);
          entry.record.textLayout = layout;
          entry.command = render::ShapeReferenceCommand{*shape, layout, selected};
        }
      }
      references.value().canonicalBytes.clear();
      references.value().digest.clear();
    }
  }
  const auto plan = render::FramePlanBuilder::build(frame, references.value());
  if (!plan) return false;
  if (selectionMode_) (void)renderSelectionOverlay();
  if (creationActive() && creationPreview().valid) {
    selectionView_ = std::make_unique<render::RenderViewRuntime>(frame);
    std::vector<render::SnapGuideGeometry> guides;
    for (const auto& guide : creationPreview().guides) {
      const auto screen = [&](float x, float y) {
        return render::ScreenPoint{x * zoom + viewport.translationX,
                                   y * zoom + viewport.translationY};
      };
      if (guide.axis == interaction::SnapGuideAxis::kVertical) {
        guides.push_back({render::SnapGuideAxis::kVertical,
                          screen(guide.position, guide.spanStart),
                          screen(guide.position, guide.spanEnd)});
      } else {
        guides.push_back({render::SnapGuideAxis::kHorizontal,
                          screen(guide.spanStart, guide.position),
                          screen(guide.spanEnd, guide.position)});
      }
    }
    selectionView_->editingOverlay().setCreationPreview(creationPreview().record, guides);
  }
  const auto rendered = previewRenderer_.renderTransientOverlay(
      *previewProvider_, plan.value(), selectionOverlay());
  if (rendered.code != render::BackendSubmissionCode::kAccepted) return false;
  if (frameCoordinator_.submit(render::FrameRole::kPreview, frame.frameId, false) !=
      render::RuntimeFrameDisposition::kAccepted) return false;
  if (frameCoordinator_.receive({previewCapture.identity, render::FrameRole::kPreview,
                                 true, render::RuntimePresentEvidence::kApproximate}) !=
      render::RuntimeFrameDisposition::kAccepted) return false;
  previewProvider_->setOverlayVisible(true);
  transientOverlayDirty_ = false;
  return true;
}

bool CanvasEditingRuntime::retryTransientOverlayFrame() noexcept {
  if (!transientOverlayDirty_) return true;
  if (!creationActive() && !selectionTransformActive()) return clearTransientOverlay();
  return presentTransientOverlayFrame(nextCanonicalFrameId());
}

bool CanvasEditingRuntime::clearTransientOverlay() noexcept {
  std::lock_guard renderLock(previewRenderMutex_);
  std::lock_guard providerLock(previewProviderMutex_);
  // Retire the logical overlay state before touching the platform surface.
  // A D3D12/DirectComposition acquire can be temporarily unavailable; that
  // must not keep a completed creation session in the retry loop.
  transientOverlayDirty_ = false;
  transientOverlayHandoffPending_ = false;
  if (previewProvider_ == nullptr) {
    selectionView_.reset();
    return false;
  }
  // DirectComposition may present the previous swap-chain buffer while the
  // transparent clear is queued. Hide first so the stale frame cannot flash
  // during a shape move or eraser handoff.
  previewProvider_->setOverlayVisible(false);
  if (!previewActive() && !selectionTransformActive() && !creationActive()) {
    selectionView_.reset();
  }
  const auto result = previewRenderer_.clearPreview(
      *previewProvider_, render::SurfaceGeneration{previewProvider_->generation()});
  if (result.code != render::BackendSubmissionCode::kAccepted) return false;
  // TransparentOverlaySkiaSurfaceProvider marks a successful present visible;
  // a clear frame is a retirement operation, so hide again after the present
  // rather than leaving the just-cleared swap-chain buffer exposed.
  previewProvider_->setOverlayVisible(false);
  return true;
}

std::vector<ink::StrokePoint> CanvasEditingRuntime::previewPoints() const {
  return preview_->snapshot().confirmed;
}

std::vector<ink::reference::StrokeOutlinePoint> CanvasEditingRuntime::brushPreviewOutline() const {
  if (brushPreviews_.empty()) return {};
  return brushPreviews_.begin()->second;
}

std::vector<std::vector<ink::reference::StrokeOutlinePoint>>
CanvasEditingRuntime::brushPreviewOutlines() const {
  std::vector<std::vector<ink::reference::StrokeOutlinePoint>> result;
  result.reserve(brushPreviews_.size());
  for (const auto& [pointer, outline] : brushPreviews_) {
    static_cast<void>(pointer);
    result.push_back(outline);
  }
  return result;
}

std::vector<ink::StrokePoint> CanvasEditingRuntime::transientPreviewPoints() const {
  return preview_->snapshot().confirmed;
}

std::vector<std::vector<ink::StrokePoint>> CanvasEditingRuntime::previewStrokes() const {
  auto strokes = committedStrokes_;
  const auto active = preview_->snapshot().confirmed;
  if (!active.empty()) strokes.push_back(active);
  return strokes;
}

}  // namespace canvas::interaction

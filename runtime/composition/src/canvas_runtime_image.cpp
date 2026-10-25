#include "canvas/runtime/canvas_runtime_facade.hpp"

#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/render/image_resource.hpp"
#include "canvas/render/image_crop_geometry.hpp"
#include "sha256_internal.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace canvas::runtime {
namespace {
ImageCommandReceipt rejected(ImageFailureReason reason) {
  ImageCommandReceipt out{};
  out.reason = reason;
  return out;
}

ImageCommandReceipt applied(const semantic::ObjectId id,
                            const interaction::CanvasEditingRuntime& runtime,
                            const semantic::ImageContent& content,
                            bool historyChanged) {
  ImageCommandReceipt out{};
  out.state = ImageCommandState::kApplied;
  out.reason = ImageFailureReason::kNone;
  out.objectId = id;
  out.content = content;
  out.semanticGeneration = runtime.semanticGeneration().value();
  out.historyChanged = historyChanged;
  return out;
}

bool validRect(const semantic::NormalizedRect& rect) noexcept {
  return std::isfinite(rect.x) && std::isfinite(rect.y) &&
      std::isfinite(rect.width) && std::isfinite(rect.height) &&
      rect.x >= 0.0 && rect.y >= 0.0 && rect.width > 0.0 && rect.height > 0.0 &&
      rect.x + rect.width <= 1.0 && rect.y + rect.height <= 1.0;
}

std::optional<semantic::NormalizedRect> canonicalRect(
    const semantic::NormalizedRect& rect) noexcept {
  constexpr double epsilon = 1.0e-12;
  if (std::abs(rect.x) <= epsilon && std::abs(rect.y) <= epsilon &&
      std::abs(rect.width - 1.0) <= epsilon && std::abs(rect.height - 1.0) <= epsilon)
    return std::nullopt;
  return rect;
}

bool validImage(const semantic::ObjectRecord* object) noexcept {
  return object != nullptr && object->kind == semantic::ObjectKind::kImage &&
      std::holds_alternative<semantic::ImageContent>(object->content);
}
}  // namespace

ImageCommandReceipt CanvasRuntimeFacade::registerImageBytes(
    const std::vector<std::uint8_t>& bytes, const std::string& mimeType, bool commit) {
  if (composition_ == nullptr || bytes.empty()) return rejected(ImageFailureReason::kInvalidRequest);
#if defined(CANVAS_RENDER_HAS_SKIA)
  // Validate the whole encoded file before changing the bound Manifest.
  const auto digest = semantic::internal::sha256(bytes);
  std::string hash = "sha256:";
  constexpr char hex[] = "0123456789abcdef";
  for (const auto b : digest) { hash += hex[b >> 4U]; hash += hex[b & 15U]; }
  foundation::ObjectId id{};
  std::copy_n(digest.begin(), id.bytes.size(), id.bytes.begin());
  if (id.isZero()) id.bytes.back() = 1;
  MemoryResourceProvider candidate;
  candidate.publish(id, bytes);
  render::ImageResourceResolver decoder(candidate);
  const auto decoded = decoder.resolve(semantic::ResourceId{id});
  if (decoded->state != render::ImageResourceState::kReady)
    return rejected(ImageFailureReason::kDecodeFailed);
  if (decoded->mimeType.empty()) return rejected(ImageFailureReason::kUnsupportedFormat);
  if (!mimeType.empty() && decoded->mimeType != mimeType)
    return rejected(ImageFailureReason::kMimeMismatch);
  if (const auto* existing = composition_->resources().find(id)) {
    if (existing->content_hash != hash || existing->resource_kind != semantic::ResourceKind::kImage)
      return rejected(ImageFailureReason::kResourceRejected);
  } else if (commit) {
    const auto& manifest = composition_->resources().manifest;
    if (manifest.manifest_revision == std::numeric_limits<std::uint64_t>::max())
      return rejected(ImageFailureReason::kResourceRejected);
    const auto varint = [](std::vector<std::uint8_t>& out, std::uint32_t value) {
      do { const auto b = static_cast<std::uint8_t>(value & 127U); value >>= 7U;
        out.push_back(static_cast<std::uint8_t>(b | (value ? 128U : 0U))); } while (value);
    };
    std::vector<std::uint8_t> metadata{8, 1, 16};
    varint(metadata, static_cast<std::uint32_t>(decoded->width));
    metadata.push_back(24); varint(metadata, static_cast<std::uint32_t>(decoded->height));
    metadata.insert(metadata.end(), {32, 1});
    semantic::UpdateResourceManifestOp update{};
    update.expected_manifest_revision = manifest.manifest_revision;
    update.upserts.push_back({semantic::ResourceId{id}, semantic::ResourceKind::kImage,
        1, hash, bytes.size(), 1, 0, std::move(metadata)});
    update.expectations.push_back({semantic::ResourceId{id}, semantic::ResourceExpectationState::kNeverUsed, 0});
    const semantic::Operation operation{runtime_.allocateOperationId(), runtime_.textDocument(), 1, 1, update};
    if (!runtime_.applyResourceManifestOperation(operation))
      return rejected(ImageFailureReason::kResourceRejected);
    (void)runtime_.recoverHistoryScene();
  }
  ImageCommandReceipt result{};
  result.state = ImageCommandState::kApplied; result.reason = ImageFailureReason::kNone;
  result.content.resource_id = semantic::ResourceId{id};
  result.content.intrinsic_width = decoded->width; result.content.intrinsic_height = decoded->height;
  return result;
#else
  (void)mimeType; (void)commit;
  return rejected(ImageFailureReason::kNotReady);
#endif
}

ImageCommandReceipt CanvasRuntimeFacade::importImage(const ImageImportRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (composition_ == nullptr || !composition_->canMutate() || runtime_.closed() || !runtime_.surface().available)
    return rejected(ImageFailureReason::kNotReady);
  if (imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (request.resourceId.value.isZero()) {
    if (readTextEditor().phase != TextPhase::kClosed || pendingWork_.load() != 0 ||
        !runtime_.textReady() || !std::isfinite(request.width) || !std::isfinite(request.height) ||
        !std::isfinite(request.transform.a) || !std::isfinite(request.transform.b) ||
        !std::isfinite(request.transform.c) || !std::isfinite(request.transform.d) ||
        !std::isfinite(request.transform.tx) || !std::isfinite(request.transform.ty) ||
        std::abs(request.transform.a * request.transform.d - request.transform.b * request.transform.c) < 1e-12 ||
        (request.sourceRect && !validRect(*request.sourceRect)) ||
        (!request.objectId.isZero() && runtime_.textRecord(request.objectId)))
      return rejected(ImageFailureReason::kInvalidRequest);
    const auto registered = registerImageBytes(request.bytes, request.mimeType, false);
    if (registered.state != ImageCommandState::kApplied) return registered;
    auto prepared = request;
    prepared.resourceId = registered.content.resource_id;
    prepared.intrinsicWidth = registered.content.intrinsic_width;
    prepared.intrinsicHeight = registered.content.intrinsic_height;
    if (prepared.placement.order_key.bytes().empty()) {
      std::vector<std::uint8_t> order;
      for (const auto& object : runtime_.semanticObjects_.children(std::nullopt))
        if (order.empty() || semantic::OrderKey(order) < object.placement.order_key)
          order.assign(object.placement.order_key.bytes().begin(), object.placement.order_key.bytes().end());
      if (order.size() < semantic::OrderKey::kMaxSize) order.push_back(128);
      prepared.placement.order_key = semantic::OrderKey(std::move(order));
    }
    if (prepared.width <= 0 && prepared.height <= 0) {
      const auto scale = std::min({1.0, 480.0 / prepared.intrinsicWidth, 360.0 / prepared.intrinsicHeight});
      prepared.width = std::max(32.0, prepared.intrinsicWidth * scale);
      prepared.height = prepared.width * prepared.intrinsicHeight / prepared.intrinsicWidth;
    }
    if (prepared.objectId.isZero()) prepared.objectId = runtime_.allocateTextIdentity();
    semantic::ObjectRecord candidate{};
    candidate.id = prepared.objectId; candidate.kind = semantic::ObjectKind::kImage; candidate.kind_version = 1;
    candidate.placement = prepared.placement; candidate.transform = prepared.transform;
    candidate.content = semantic::ImageContent{prepared.resourceId, prepared.intrinsicWidth, prepared.intrinsicHeight,
        prepared.sourceRect, prepared.contentMode, prepared.width, prepared.height};
    const semantic::Operation probe{runtime_.allocateOperationId(),runtime_.textDocument(),1,1,semantic::InsertObjectsOp{{candidate}}};
    if (composition_->notifications_.size() >= 256 ||
        runtime_.prepareTextOperation(probe).disposition != semantic::PrepareDisposition::kPrepared)
      return rejected(ImageFailureReason::kObjectRejected);
    const auto registration = registerImageBytes(request.bytes, request.mimeType);
    if (registration.state != ImageCommandState::kApplied) return registration;
    const auto result = importImage(prepared);
    if (result.state == ImageCommandState::kApplied) {
      (void)runtime_.setTextTool(false);
      (void)runtime_.setPanTool(false);
      (void)runtime_.setSelectionMode(true);
      (void)runtime_.selection_.click(result.objectId);
      runtime_.markCanonicalPresentationDirty();
    }
    return result;
  }
  if (request.resourceId.value.isZero() || request.bytes.empty() ||
      !std::isfinite(request.intrinsicWidth) || !std::isfinite(request.intrinsicHeight) ||
      request.intrinsicWidth <= 0.0 || request.intrinsicHeight <= 0.0)
    return rejected(ImageFailureReason::kInvalidRequest);
  const auto* entry = composition_->resources().find(request.resourceId.value);
  if (entry == nullptr) return rejected(ImageFailureReason::kUnknownResource);
  const auto candidate = EncodedResource{0U,
      std::make_shared<const std::vector<std::uint8_t>>(request.bytes)};
  if (!validateResourceBytes(*entry, candidate))
    return rejected(ImageFailureReason::kResourceRejected);

  semantic::ObjectId objectId = request.objectId;
  if (objectId.isZero()) objectId = runtime_.allocateTextIdentity();
  if (objectId.isZero() || runtime_.qualificationObjectRecord(objectId) != nullptr)
    return rejected(ImageFailureReason::kObjectRejected);
  semantic::ImageContent content{};
  content.resource_id = request.resourceId;
  content.intrinsic_width = request.intrinsicWidth;
  content.intrinsic_height = request.intrinsicHeight;
  content.source_rect = request.sourceRect ? canonicalRect(*request.sourceRect) : std::nullopt;
  content.content_mode = request.contentMode;
  content.width = request.width > 0.0 ? request.width : request.intrinsicWidth;
  content.height = request.height > 0.0 ? request.height : request.intrinsicHeight;
  if (!std::isfinite(content.width) || !std::isfinite(content.height) ||
      content.width <= 0.0 || content.height <= 0.0 ||
      (content.source_rect && !validRect(*content.source_rect)))
    return rejected(ImageFailureReason::kInvalidRequest);

  semantic::ObjectRecord object{};
  object.id = objectId;
  object.kind = semantic::ObjectKind::kImage;
  object.kind_version = 1U;
  object.placement = request.placement;
  object.transform = request.transform;
  object.content = content;
  const auto forwardId = runtime_.allocateOperationId();
  const auto inverseId = runtime_.allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return rejected(ImageFailureReason::kObjectRejected);
  const semantic::Operation forward{forwardId, runtime_.textDocument(), 1U, 1U,
      semantic::InsertObjectsOp{{object}}};
  if (runtime_.prepareTextOperation(forward).disposition != semantic::PrepareDisposition::kPrepared)
    return rejected(ImageFailureReason::kObjectRejected);
  const auto token = composition_->currentResourceToken(request.resourceId.value);
  if (!token || !composition_->publishResource(*token, request.bytes))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!composition_->ownerPump() || !composition_->imageResourceDecodeReady(request.resourceId.value))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!runtime_.historySceneReady() && !runtime_.recoverHistoryScene())
    return rejected(ImageFailureReason::kObjectRejected);
  if (!runtime_.applyLocalHistoryOperation(
          forward,
          {inverseId, runtime_.textDocument(), 1U, 1U,
           semantic::DeleteObjectsOp{{objectId}}}))
  {
    return rejected(ImageFailureReason::kObjectRejected);
  }
  (void)composition_->ownerPump();
  return applied(objectId, runtime_, content, true);
}

ImageCommandReceipt CanvasRuntimeFacade::replaceImage(const ImageReplaceRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (composition_ == nullptr || !composition_->canMutate()) return rejected(ImageFailureReason::kNotReady);
  if (imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (request.resourceId.value.isZero()) {
    if (!validImage(runtime_.textRecord(request.objectId)) || !runtime_.textWritable(request.objectId))
      return rejected(ImageFailureReason::kObjectRejected);
    if (readTextEditor().phase != TextPhase::kClosed || pendingWork_.load() != 0 ||
        !runtime_.textReady() || !std::isfinite(request.content.width) || !std::isfinite(request.content.height) ||
        (request.content.source_rect && !validRect(*request.content.source_rect)))
      return rejected(ImageFailureReason::kInvalidRequest);
    const auto registered = registerImageBytes(request.bytes, request.mimeType, false);
    if (registered.state != ImageCommandState::kApplied) return registered;
    auto prepared = request; prepared.resourceId = registered.content.resource_id;
    prepared.content.intrinsic_width = registered.content.intrinsic_width;
    prepared.content.intrinsic_height = registered.content.intrinsic_height;
    prepared.content.resource_id = prepared.resourceId;
    const auto* before = runtime_.textRecord(prepared.objectId);
    const auto prior = std::get<semantic::ImageContent>(before->content);
    if (prepared.content.width <= 0) prepared.content.width = prior.width;
    if (prepared.content.height <= 0) prepared.content.height = prior.height;
    const semantic::Operation probe{runtime_.allocateOperationId(),runtime_.textDocument(),1,1,
        semantic::SetImageContentOp{prepared.objectId,prepared.content}};
    if (composition_->notifications_.size() >= 256 ||
        runtime_.prepareTextOperation(probe).disposition != semantic::PrepareDisposition::kPrepared)
      return rejected(ImageFailureReason::kObjectRejected);
    const auto registration = registerImageBytes(request.bytes, request.mimeType);
    if (registration.state != ImageCommandState::kApplied) return registration;
    const auto result = replaceImage(prepared);
    return result;
  }
  const auto* before = runtime_.qualificationObjectRecord(request.objectId);
  if (!validImage(before) || request.resourceId.value.isZero() || request.bytes.empty())
    return rejected(ImageFailureReason::kInvalidRequest);
  const auto* entry = composition_ == nullptr ? nullptr : composition_->resources().find(request.resourceId.value);
  if (entry == nullptr) return rejected(ImageFailureReason::kUnknownResource);
  const auto candidate = EncodedResource{0U,
      std::make_shared<const std::vector<std::uint8_t>>(request.bytes)};
  if (!validateResourceBytes(*entry, candidate)) return rejected(ImageFailureReason::kResourceRejected);
  semantic::ImageContent next = request.content;
  next.resource_id = request.resourceId;
  if (next.width <= 0.0) next.width = std::get<semantic::ImageContent>(before->content).width;
  if (next.height <= 0.0) next.height = std::get<semantic::ImageContent>(before->content).height;
  if (next.intrinsic_width <= 0.0) next.intrinsic_width = std::get<semantic::ImageContent>(before->content).intrinsic_width;
  if (next.intrinsic_height <= 0.0) next.intrinsic_height = std::get<semantic::ImageContent>(before->content).intrinsic_height;
  if (!std::isfinite(next.width) || !std::isfinite(next.height) || next.width <= 0.0 || next.height <= 0.0 ||
      (next.source_rect && !validRect(*next.source_rect))) return rejected(ImageFailureReason::kInvalidRequest);
  const auto currentToken = composition_ == nullptr
      ? std::nullopt : composition_->currentResourceToken(request.resourceId.value);
  if (composition_ == nullptr || !currentToken ||
      !composition_->publishResource(*currentToken, request.bytes))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!composition_->ownerPump() || !composition_->imageResourceDecodeReady(request.resourceId.value))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!runtime_.historySceneReady() && !runtime_.recoverHistoryScene())
    return rejected(ImageFailureReason::kObjectRejected);
  const auto forwardId = runtime_.allocateOperationId(), inverseId = runtime_.allocateOperationId();
  if (!runtime_.applyLocalHistoryOperation(
          {forwardId, runtime_.textDocument(), 1U, 1U, semantic::SetImageContentOp{request.objectId, next}},
          {inverseId, runtime_.textDocument(), 1U, 1U,
           semantic::SetImageContentOp{request.objectId, std::get<semantic::ImageContent>(before->content)}}))
    return rejected(ImageFailureReason::kObjectRejected);
  return applied(request.objectId, runtime_, next, true);
}

ImageCommandReceipt CanvasRuntimeFacade::resizeImage(const ImageResizeRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!composition_ || !composition_->canMutate()) return rejected(ImageFailureReason::kNotReady);
  if (imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  const auto* before = runtime_.qualificationObjectRecord(request.objectId);
  if (!validImage(before) || !std::isfinite(request.width) || !std::isfinite(request.height) || request.width <= 0.0)
    return rejected(ImageFailureReason::kInvalidRequest);
  auto next = std::get<semantic::ImageContent>(before->content);
  if (request.policy == ImageResizePolicy::kPreserveAspect) {
    if (next.width <= 0.0 || next.height <= 0.0) return rejected(ImageFailureReason::kInvalidRequest);
    next.height = request.width * next.height / next.width;
  } else if (request.height <= 0.0) {
    return rejected(ImageFailureReason::kInvalidRequest);
  }
  next.width = request.width;
  if (request.policy == ImageResizePolicy::kFreeStretch) next.height = request.height;
  const auto f = runtime_.allocateOperationId(), i = runtime_.allocateOperationId();
  if (!runtime_.historySceneReady() && !runtime_.recoverHistoryScene())
    return rejected(ImageFailureReason::kObjectRejected);
  if (!runtime_.applyLocalHistoryOperation(
          {f, runtime_.textDocument(), 1U, 1U,
           semantic::SetObjectSizeOp{{{request.objectId, next.width, next.height}}}},
          {i, runtime_.textDocument(), 1U, 1U,
           semantic::SetObjectSizeOp{{{request.objectId,
               std::get<semantic::ImageContent>(before->content).width,
               std::get<semantic::ImageContent>(before->content).height}}}}))
    return rejected(ImageFailureReason::kObjectRejected);
  return applied(request.objectId, runtime_, next, true);
}

ImageCommandReceipt CanvasRuntimeFacade::beginImageCrop(const ImageCropBeginRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (composition_ == nullptr || !composition_->canMutate()) return rejected(ImageFailureReason::kNotReady);
  const auto* object = runtime_.qualificationObjectRecord(request.objectId);
  if (!validImage(object) || !runtime_.textWritable(request.objectId) || !runtime_.textReady())
    return rejected(ImageFailureReason::kObjectRejected);
  if (imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  const auto content = std::get<semantic::ImageContent>(object->content);
  imageCrop_ = ImageCropSession{nextImageCropToken_++, request.objectId, content, content};
  runtime_.imageCropPresentation_ = *object;
  runtime_.imageCropPointer_ = [this](const input::PointerSample& sample) { imageCropPointer(sample); };
  runtime_.markCanonicalPresentationDirty();
  ImageCommandReceipt out = applied(request.objectId, runtime_, content, false);
  out.cropToken = imageCrop_->token;
  return out;
}

ImageCommandReceipt CanvasRuntimeFacade::updateImageCrop(const ImageCropUpdateRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (request.cropToken != imageCrop_->token) return rejected(ImageFailureReason::kStaleCropSession);
  const auto* current = runtime_.textRecord(imageCrop_->objectId);
  if (!composition_ || !composition_->canMutate() || !validImage(current) ||
      !runtime_.textWritable(imageCrop_->objectId) ||
      std::get<semantic::ImageContent>(current->content) != imageCrop_->original) {
    discardImageCrop(); return rejected(ImageFailureReason::kStaleCropSession);
  }
  if (request.contentScale) {
    const double scale = *request.contentScale;
    if (!std::isfinite(scale) || scale < .25 || scale > 4)
      return rejected(ImageFailureReason::kInvalidRequest);
    const auto original = imageCrop_->original.source_rect.value_or(semantic::NormalizedRect{0,0,1,1});
    const auto previous = imageCrop_->current.source_rect.value_or(semantic::NormalizedRect{0,0,1,1});
    const double width = std::min(1.0,original.width/scale), height = std::min(1.0,original.height/scale);
    if (width <= 1.0/4096 || height <= 1.0/4096)
      return rejected(ImageFailureReason::kInvalidRequest);
    semantic::NormalizedRect rect{std::clamp(previous.x+previous.width/2-width/2,0.0,1-width),
        std::clamp(previous.y+previous.height/2-height/2,0.0,1-height),width,height};
    imageCrop_->current.source_rect = canonicalRect(rect);
    imageCrop_->contentScale = scale;
    if (runtime_.imageCropPresentation_) runtime_.imageCropPresentation_->content = imageCrop_->current;
    runtime_.markCanonicalPresentationDirty();
    auto out=applied(imageCrop_->objectId,runtime_,imageCrop_->current,false);
    out.cropToken=imageCrop_->token; return out;
  }
  if (!validRect(request.sourceRect) || request.sourceRect.width <= 1.0/4096 ||
      request.sourceRect.height <= 1.0/4096 || !std::isfinite(request.width) || !std::isfinite(request.height) ||
      request.width <= 1.0 || request.height <= 1.0) return rejected(ImageFailureReason::kInvalidRequest);
  imageCrop_->current.source_rect = canonicalRect(request.sourceRect);
  imageCrop_->current.width = request.width;
  imageCrop_->current.height = request.height;
  if (runtime_.imageCropPresentation_) runtime_.imageCropPresentation_->content = imageCrop_->current;
  runtime_.markCanonicalPresentationDirty();
  auto out = applied(imageCrop_->objectId, runtime_, imageCrop_->current, false);
  out.cropToken = imageCrop_->token;
  return out;
}

ImageCommandReceipt CanvasRuntimeFacade::commitImageCrop(std::uint64_t token) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (token != imageCrop_->token) return rejected(ImageFailureReason::kStaleCropSession);
  const auto session = *imageCrop_;
  const auto* current = runtime_.textRecord(session.objectId);
  if (!composition_ || !composition_->canMutate() || !validImage(current) ||
      !runtime_.textWritable(session.objectId) ||
      std::get<semantic::ImageContent>(current->content) != session.original) {
    discardImageCrop(); return rejected(ImageFailureReason::kStaleCropSession);
  }
  if (session.current == session.original) {
    discardImageCrop(); return applied(session.objectId, runtime_, session.original, false);
  }
  const auto f = runtime_.allocateOperationId(), i = runtime_.allocateOperationId();
  if (!runtime_.applyLocalHistoryOperation(
          {f, runtime_.textDocument(), 1U, 1U, semantic::SetImageContentOp{session.objectId, session.current}},
          {i, runtime_.textDocument(), 1U, 1U, semantic::SetImageContentOp{session.objectId, session.original}}))
  {
    return rejected(ImageFailureReason::kObjectRejected);
  }
  discardImageCrop();
  return applied(session.objectId, runtime_, session.current, true);
}

ImageCommandReceipt CanvasRuntimeFacade::cancelImageCrop(std::uint64_t token) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (token != imageCrop_->token) return rejected(ImageFailureReason::kStaleCropSession);
  const auto session = *imageCrop_;
  discardImageCrop();
  return applied(session.objectId, runtime_, session.original, false);
}

ImageCropSnapshot CanvasRuntimeFacade::readImageCrop() const {
  ImageCropSnapshot out{};
  if (!imageCrop_) return out;
  out.active = true;
  out.cropToken = imageCrop_->token;
  out.objectId = imageCrop_->objectId;
  out.content = imageCrop_->current;
  out.contentScale = imageCrop_->contentScale;
  return out;
}

void CanvasRuntimeFacade::discardImageCrop() noexcept {
  imageCrop_.reset();
  runtime_.imageCropPresentation_.reset();
  runtime_.imageCropPointer_ = {};
  runtime_.markCanonicalPresentationDirty();
}

void CanvasRuntimeFacade::imageCropPointer(const input::PointerSample& sample) {
  if (!imageCrop_ || !runtime_.imageCropPresentation_ || sample.predicted) return;
  if (sample.phase == input::PointerPhase::kCancel) {
    if (imageCrop_->pointer == sample.key) discardImageCrop();
    return;
  }
  const auto world = runtime_.viewToContent(sample.x, sample.y);
  const auto t = runtime_.imageCropPresentation_->transform;
  const double det = t.a*t.d-t.b*t.c;
  if (std::abs(det) < 1e-12) return;
  const double x = (t.d*(world.first-t.tx)-t.c*(world.second-t.ty))/det;
  const double y = (-t.b*(world.first-t.tx)+t.a*(world.second-t.ty))/det;
  if (sample.phase == input::PointerPhase::kDown) {
    if (imageCrop_->pointer.valid()) return;
    const auto w = imageCrop_->current.width, h = imageCrop_->current.height;
    const auto camera = readRuntimeState().camera;
    const double radius = 12.0 / std::max(.001, static_cast<double>(camera.scale) * std::hypot(t.a,t.b));
    const auto sourceBounds = render::imageCropSourceBounds(imageCrop_->current);
    const bool overSource = x >= sourceBounds.left && x <= sourceBounds.right &&
        y >= sourceBounds.top && y <= sourceBounds.bottom;
    const bool overFrame = x >= -radius && y >= -radius && x <= w+radius && y <= h+radius;
    if (!overSource && !overFrame) return;
    const bool left = overFrame && std::abs(x) <= radius, right = overFrame && std::abs(x-w) <= radius;
    const bool top = overFrame && std::abs(y) <= radius, bottom = overFrame && std::abs(y-h) <= radius;
    using H = render::HandleKind;
    imageCrop_->handle = left && top ? H::kTopLeft : right && top ? H::kTopRight
        : left && bottom ? H::kBottomLeft : right && bottom ? H::kBottomRight
        : left ? H::kLeft : right ? H::kRight : top ? H::kTop : bottom ? H::kBottom : H::kNone;
    imageCrop_->pointer = sample.key; imageCrop_->downX = x; imageCrop_->downY = y;
    imageCrop_->dragStart = imageCrop_->current;
    return;
  }
  if (imageCrop_->pointer != sample.key) return;
  if (sample.phase == input::PointerPhase::kMove || sample.phase == input::PointerPhase::kUp) {
    const auto start = imageCrop_->dragStart;
    auto r = start.source_rect.value_or(semantic::NormalizedRect{0,0,1,1});
    const auto initial = r;
    const double dx = x-imageCrop_->downX, dy = y-imageCrop_->downY;
    using H = render::HandleKind;
    const auto handle = imageCrop_->handle;
    if (handle == H::kNone) {
      const auto sourceBounds = render::imageCropSourceBounds(start);
      r.x = std::clamp(r.x-dx/(sourceBounds.right-sourceBounds.left), 0.0, 1-r.width);
      r.y = std::clamp(r.y-dy/(sourceBounds.bottom-sourceBounds.top), 0.0, 1-r.height);
    } else {
      const double minimum = std::nextafter(1.0/4096, 1.0);
      const bool left = handle==H::kLeft || handle==H::kTopLeft || handle==H::kBottomLeft;
      const bool right = handle==H::kRight || handle==H::kTopRight || handle==H::kBottomRight;
      const bool top = handle==H::kTop || handle==H::kTopLeft || handle==H::kTopRight;
      const bool bottom = handle==H::kBottom || handle==H::kBottomLeft || handle==H::kBottomRight;
      if (left) { const auto end=r.x+r.width; r.x=std::clamp(r.x+dx/start.width*r.width,0.0,end-minimum); r.width=end-r.x; }
      if (right) r.width=std::clamp(r.width+dx/start.width*r.width,minimum,1-r.x);
      if (top) { const auto end=r.y+r.height; r.y=std::clamp(r.y+dy/start.height*r.height,0.0,end-minimum); r.height=end-r.y; }
      if (bottom) r.height=std::clamp(r.height+dy/start.height*r.height,minimum,1-r.y);
    }
    const auto token = imageCrop_->token;
    (void)updateImageCrop({token,r,
        handle==H::kNone ? start.width : std::max(1.01,start.width*r.width/initial.width),
        handle==H::kNone ? start.height : std::max(1.01,start.height*r.height/initial.height),std::nullopt});
  }
  if (imageCrop_ && sample.phase == input::PointerPhase::kUp) imageCrop_->pointer = {};
}

SelectedImageSnapshot CanvasRuntimeFacade::readSelectedImage() const {
  SelectedImageSnapshot out{};
  if (!ownerThread()) return out;
  const auto object = runtime_.selectedObjectRecord();
  if (!validImage(object ? &*object : nullptr)) return out;
  out.available = true; out.objectId = object->id;
  out.content = std::get<semantic::ImageContent>(object->content);
  out.transform = object->transform;
  out.editable = composition_ && composition_->canMutate() && runtime_.textWritable(object->id);
  return out;
}

}  // namespace canvas::runtime

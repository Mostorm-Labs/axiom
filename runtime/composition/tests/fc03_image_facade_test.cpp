#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/runtime/runtime_facade.hpp"

#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <limits>

#ifndef FC03_FIXTURE_ROOT
#error FC03_FIXTURE_ROOT must be provided by the test target
#endif

namespace {
std::vector<std::uint8_t> readFixture(const char* name) {
  std::ifstream file(std::string(FC03_FIXTURE_ROOT) + "/" + name,
                     std::ios::binary);
  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
}

int main() {
  using namespace canvas;
  runtime::RuntimeComposition composition;
  assert(composition.open());
  assert(composition.bindSurface(256U, 256U));
  auto& facade = composition.controlFacade();

  const std::vector<std::uint8_t> png = {
      137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,
      8,4,0,0,0,181,28,12,2,0,0,0,11,73,68,65,84,120,218,99,100,248,15,
      0,1,5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
  const auto jpeg = readFixture("fc03-red.jpg");
  const auto webp = readFixture("fc03-sample.webp");
  assert(!jpeg.empty() && !webp.empty());
  // An ordinary consumer supplies file bytes, not a prebuilt Manifest.
  // Rejecting this request leaves the actual Minimal Host unable to import.
  {
    runtime::RuntimeComposition files;
    assert(files.open() && files.bindSurface(800, 600));
    auto& consumer = files.controlFacade();
    runtime::ImageImportRequest file;
    file.bytes = jpeg;
    const auto receipt = consumer.importImage(file);
    assert(receipt.state == runtime::ImageCommandState::kApplied);
    assert(receipt.content.intrinsic_width == 1.0);
    assert(receipt.content.intrinsic_height == 1.0);
    assert(files.resources().manifest.entries.size() == 1U);
    assert(files.semanticObjectCount() == 1U);
    const auto beforeInvalid = files.resources();
    const auto beforeInvalidGeneration = files.readback().semanticGeneration;
    auto invalid = file;
    invalid.bytes = webp;
    invalid.transform.a = std::numeric_limits<double>::quiet_NaN();
    assert(consumer.importImage(invalid).state == runtime::ImageCommandState::kRejected);
    assert(files.resources().manifest == beforeInvalid.manifest);
    assert(files.resources().lifetimeGeneration == beforeInvalid.lifetimeGeneration);
    assert(files.readback().semanticGeneration == beforeInvalidGeneration);
    invalid = file; invalid.bytes = webp;
    invalid.placement.parent_id = foundation::ObjectId::fromUint64(99123);
    assert(consumer.importImage(invalid).state == runtime::ImageCommandState::kRejected);
    assert(files.resources().manifest == beforeInvalid.manifest);
    assert(files.readback().semanticGeneration == beforeInvalidGeneration);
    invalid = file; invalid.mimeType = "image/png";
    assert(consumer.importImage(invalid).reason == runtime::ImageFailureReason::kMimeMismatch);
    assert(files.resources().manifest == beforeInvalid.manifest);
    assert(consumer.undo(501, consumer.readRuntimeState().identity.runtimeGeneration).state ==
           runtime::ProductControlState::kQueued);
    assert(files.ownerPump());
    assert(files.semanticObjectCount() == 0U);
    assert(consumer.redo(502, consumer.readRuntimeState().identity.runtimeGeneration).state ==
           runtime::ProductControlState::kQueued);
    assert(files.ownerPump());
    assert(files.semanticObjectCount() == 1U);
  }
  for (const auto& bytes : {png, jpeg, webp}) {
    runtime::RuntimeComposition ordinary;
    assert(ordinary.open() && ordinary.bindSurface(800,600));
    auto& consumer = ordinary.controlFacade();
    runtime::CanvasControlRequest tool{};
    tool.clientId=123; tool.requestId=1; tool.target=consumer.mountedCanvasTargets().front();
    tool.payload.kind=runtime::CanvasControlPayloadKind::kSelectTool;
    tool.payload.tool=runtime::CanvasToolKind::kText;
    assert(consumer.submitCanvasControl(tool).state == runtime::ProductControlState::kQueued);
    assert(ordinary.ownerPump());
    runtime::ImageImportRequest file{}; file.bytes=bytes;
    const auto result=consumer.importImage(file);
    assert(result.state==runtime::ImageCommandState::kApplied);
    assert(consumer.readCanvasControlSnapshot(consumer.mountedCanvasTargets().front()).tool==runtime::CanvasToolKind::kSelect);
    const auto before=ordinary.readback().semanticGeneration;
    const auto manifest=ordinary.resources().manifest;
    file.bytes={1,2,3};
    assert(consumer.importImage(file).reason==runtime::ImageFailureReason::kDecodeFailed);
    assert(ordinary.readback().semanticGeneration==before && ordinary.resources().manifest==manifest);
  }
  semantic::ResourceManifest manifest{};
  manifest.manifest_revision = 1U;
  manifest.entries.push_back({semantic::ResourceId{foundation::ObjectId::fromUint64(77U)},
      semantic::ResourceKind::kImage, 1U,
      "sha256:431ced6916a2a21a156e38701afe55bbd7f88969fbbfc56d7fe099d47f265460",
      png.size(), 1U, 0U, {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01}});
  manifest.entries.push_back({semantic::ResourceId{foundation::ObjectId::fromUint64(78U)},
      semantic::ResourceKind::kImage, 1U,
      "sha256:1df3a19acd973f85c4470dec6266fbc668cee8221e30cde2bae66d04decc0a5c",
      jpeg.size(), 1U, 0U, {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01}});
  manifest.entries.push_back({semantic::ResourceId{foundation::ObjectId::fromUint64(79U)},
      semantic::ResourceKind::kImage, 1U,
      "sha256:4a5afeaff8483923da964bc7896f02d0283e8bff99b5b8f82a31ae3214dab1d0",
      webp.size(), 1U, 0U, {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01}});
  assert(composition.bindManifest(manifest, 42U));
  runtime::ImageImportRequest request{};
  request.resourceId = semantic::ResourceId{foundation::ObjectId::fromUint64(77U)};
  request.bytes = png;
  request.intrinsicWidth = 30.0;
  request.intrinsicHeight = 20.0;
  request.objectId = foundation::ObjectId::fromUint64(9001U);
  request.placement.order_key = semantic::OrderKey{{0x40U}};
  const auto imported = facade.importImage(request);
  assert(imported.state == runtime::ImageCommandState::kApplied);
  assert(imported.objectId != foundation::ObjectId{});

  runtime::ImageImportRequest jpegRequest = request;
  jpegRequest.resourceId = semantic::ResourceId{foundation::ObjectId::fromUint64(78U)};
  jpegRequest.bytes = jpeg;
  jpegRequest.objectId = foundation::ObjectId::fromUint64(9002U);
  jpegRequest.placement.order_key = semantic::OrderKey{{0x50U}};
  const auto jpegImported = facade.importImage(jpegRequest);
  assert(jpegImported.state == runtime::ImageCommandState::kApplied);

  runtime::ImageImportRequest webpRequest = request;
  webpRequest.resourceId = semantic::ResourceId{foundation::ObjectId::fromUint64(79U)};
  webpRequest.bytes = webp;
  webpRequest.objectId = foundation::ObjectId::fromUint64(9003U);
  webpRequest.placement.order_key = semantic::OrderKey{{0x60U}};
  const auto webpImported = facade.importImage(webpRequest);
  assert(webpImported.state == runtime::ImageCommandState::kApplied);

  runtime::ImageCropBeginRequest begin{};
  begin.objectId = imported.objectId;
  const auto started = facade.beginImageCrop(begin);
  assert(started.state == runtime::ImageCommandState::kApplied);
  assert(started.cropToken != 0U);
  const auto initialCrop = facade.readImageCrop().content;
  assert(facade.updateImageCrop({started.cropToken, {0,0,1.0/4096,1}, 30,20}).state ==
      runtime::ImageCommandState::kRejected);
  assert(facade.updateImageCrop({started.cropToken, {0,0,1,1}, 1,20}).state ==
      runtime::ImageCommandState::kRejected);
  assert(facade.readImageCrop().content == initialCrop);
  runtime::ImageCropUpdateRequest scaleRequest{started.cropToken, {}, 0, 0, 0.1};
  assert(facade.updateImageCrop(scaleRequest).state == runtime::ImageCommandState::kRejected);
  scaleRequest.contentScale = 2;
  assert(facade.updateImageCrop(scaleRequest).state == runtime::ImageCommandState::kApplied);
  assert(facade.readImageCrop().content.source_rect == semantic::NormalizedRect(.25,.25,.5,.5));
  assert(facade.readImageCrop().contentScale == 2);
  runtime::ImageCropUpdateRequest update{};
  update.cropToken = started.cropToken;
  update.sourceRect = semantic::NormalizedRect{0.1, 0.1, 0.8, 0.8};
  update.width = 120.0;
  update.height = 80.0;
  assert(facade.updateImageCrop(update).state == runtime::ImageCommandState::kApplied);
  const auto committed = facade.commitImageCrop(started.cropToken);
  assert(committed.state == runtime::ImageCommandState::kApplied);
  assert(committed.content.source_rect.has_value());
  assert(committed.content.width == 120.0);

  const auto beforeBadImportObjects = composition.semanticObjectCount();
  const auto beforeBadImportGeneration = composition.readback().semanticGeneration;
  auto bad = request;
  bad.objectId = foundation::ObjectId::fromUint64(9902U);
  bad.bytes = {1U, 2U, 3U};
  const auto rejected = facade.importImage(bad);
  assert(rejected.state == runtime::ImageCommandState::kRejected);
  assert(rejected.reason == runtime::ImageFailureReason::kResourceRejected);
  assert(composition.semanticObjectCount() == beforeBadImportObjects);
  assert(composition.readback().semanticGeneration == beforeBadImportGeneration);

  const auto resized = facade.resizeImage({imported.objectId, 240.0, 1.0,
      runtime::ImageResizePolicy::kPreserveAspect});
  assert(resized.state == runtime::ImageCommandState::kApplied);
  assert(resized.content.width == 240.0);
  assert(resized.content.height == 160.0);
  const auto stretched = facade.resizeImage({imported.objectId, 240.0, 90.0,
      runtime::ImageResizePolicy::kFreeStretch});
  assert(stretched.state == runtime::ImageCommandState::kApplied);
  assert(stretched.content.height == 90.0);

  assert(composition.qualificationRuntime().undo());
  const auto afterUndo = composition.qualificationRuntime().qualificationObjectRecord(imported.objectId);
  assert(afterUndo != nullptr);
  assert(std::get<semantic::ImageContent>(afterUndo->content).height == 160.0);
  assert(composition.qualificationRuntime().redo());
  const auto afterRedo = composition.qualificationRuntime().qualificationObjectRecord(imported.objectId);
  assert(afterRedo != nullptr);
  assert(std::get<semantic::ImageContent>(afterRedo->content).height == 90.0);

  semantic::UpdateResourceManifestOp remove{};
  remove.expected_manifest_revision = manifest.manifest_revision;
  remove.removes.push_back(request.resourceId);
  const semantic::Operation removeOperation{
      semantic::OperationId{foundation::ObjectId::fromUint64(7001U)},
      semantic::DocumentId{foundation::ObjectId::fromUint64(1U)}, 1U, 1U, remove};
  const auto beforeRemoveObjects = composition.semanticObjectCount();
  const auto beforeRemoveGeneration = composition.readback().semanticGeneration;
  const auto removeResult = composition.applyExternalOperation(
      removeOperation, semantic::ApplySource::kRemoteSync);
  assert(removeResult.disposition == semantic::ApplyDisposition::kRejected);
  assert(composition.semanticObjectCount() == beforeRemoveObjects);
  assert(composition.readback().semanticGeneration == beforeRemoveGeneration);

  const auto beforeCancel = composition.readback().semanticGeneration;
  const auto second = facade.beginImageCrop({imported.objectId});
  assert(second.state == runtime::ImageCommandState::kApplied);
  assert(facade.cancelImageCrop(second.cropToken).state == runtime::ImageCommandState::kApplied);
  assert(composition.readback().semanticGeneration == beforeCancel);

  const auto lostSession = facade.beginImageCrop({imported.objectId});
  assert(lostSession.state == runtime::ImageCommandState::kApplied);
  assert(composition.loseSurface());
  assert(!facade.readImageCrop().active);
  assert(composition.rebindSurface());
  const auto closeSession = facade.beginImageCrop({imported.objectId});
  assert(closeSession.state == runtime::ImageCommandState::kApplied);
  assert(composition.close());
  assert(!facade.readImageCrop().active);
  return 0;
}

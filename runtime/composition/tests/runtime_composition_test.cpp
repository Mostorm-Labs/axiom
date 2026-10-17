#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <string>

namespace {
canvas::semantic::ResourceManifestEntry entry(std::uint64_t id) {
  canvas::semantic::ResourceManifestEntry value;
  value.resource_id = canvas::semantic::ResourceId{
      canvas::foundation::ObjectId::fromUint64(id)};
  value.resource_revision = 1U;
  value.content_hash =
      "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a";
  value.byte_length = 1U;
  value.decode_version = 1U;
  value.metadata = {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01};
  return value;
}

canvas::semantic::ResourceManifest makeManifest(std::uint64_t count) {
  canvas::semantic::ResourceManifest value;
  value.manifest_revision = 1U;
  value.entries.reserve(static_cast<std::size_t>(count));
  for (std::uint64_t id = 1U; id <= count; ++id) value.entries.push_back(entry(id));
  return value;
}
}

int main() {
  canvas::runtime::RuntimeComposition composition;
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kCreated);
  assert(composition.open());
  assert(composition.bindSurface(256U, 256U));
  assert(composition.registerSource(9U));
  canvas::semantic::ResourceManifest manifest;
  canvas::semantic::ResourceManifestEntry entry;
  entry.resource_id = canvas::semantic::ResourceId{canvas::foundation::ObjectId::fromUint64(7U)};
  entry.resource_revision = 1U;
  entry.content_hash = "sha256:4bf5122f344554c53bde2ebb8cd2b7e3d1600ad631c385a5d7cce23c7785459a";
  entry.byte_length = 1U;
  entry.decode_version = 1U;
  entry.metadata = {0x08, 0x01, 0x10, 0x01, 0x18, 0x01, 0x20, 0x01};
  manifest.entries.push_back(entry);
  manifest.manifest_revision = 1U;
  assert(composition.bindManifest(manifest, 1U));
  assert(composition.publishResource(canvas::foundation::ObjectId::fromUint64(7U), {0x01U}));
  assert(composition.ownerPump());
  assert(composition.readback().pendingNotifications == 0U);
  assert(composition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(7U)).has_value());
  const auto firstToken = composition.resources().token(canvas::foundation::ObjectId::fromUint64(7U), 0U, 99U);
  assert(firstToken.has_value());
  const auto validated = composition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(7U));
  assert(validated->state == canvas::runtime::ResourceBindingState::kBytesValidated);
  assert(!composition.notifyResource(canvas::foundation::ObjectId::fromUint64(99U)));
  assert(composition.lastResourceError() == canvas::runtime::ResourcePortError::kUnknownResource);
  canvas::semantic::ResourceManifest rebound = manifest;
  rebound.entries.front().resource_id = canvas::semantic::ResourceId{canvas::foundation::ObjectId::fromUint64(8U)};
  rebound.manifest_revision = 2U;
  assert(composition.bindManifest(rebound, 2U));
  assert(composition.readback().lifetimeGeneration == 2U);
  assert(!composition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(7U)).has_value());
  assert(!composition.publishResource(*firstToken, {0x01U}));
  assert(composition.lastResourceError() == canvas::runtime::ResourcePortError::kStaleBinding);

  canvas::runtime::RuntimeComposition queueComposition;
  assert(queueComposition.open());
  assert(queueComposition.bindSurface(256U, 256U));
  assert(queueComposition.bindManifest(makeManifest(257U), 7U));
  assert(queueComposition.publishResource(canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  assert(queueComposition.ownerPump());
  const auto beforeQueueFull = queueComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U));
  assert(beforeQueueFull.has_value());
  for (std::uint64_t id = 2U; id <= 257U; ++id) {
    assert(queueComposition.notifyResource(canvas::foundation::ObjectId::fromUint64(id)));
  }
  assert(!queueComposition.publishResource(canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  assert(queueComposition.lastResourceError() == canvas::runtime::ResourcePortError::kQueueFull);
  assert(queueComposition.ownerPump());
  const auto afterQueueFull = queueComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U));
  assert(afterQueueFull.has_value());
  assert(afterQueueFull->state == canvas::runtime::ResourceBindingState::kBytesValidated);
  assert(afterQueueFull->providerGeneration == beforeQueueFull->providerGeneration);

  canvas::runtime::RuntimeComposition rebindComposition;
  assert(rebindComposition.open());
  assert(rebindComposition.bindSurface(256U, 256U));
  auto sameResource = makeManifest(1U);
  assert(rebindComposition.bindManifest(sameResource, 11U));
  assert(rebindComposition.publishResource(canvas::foundation::ObjectId::fromUint64(1U), {0x01U}));
  assert(rebindComposition.ownerPump());
  assert(rebindComposition.bindManifest(sameResource, 12U));
  assert(rebindComposition.notifyResource(canvas::foundation::ObjectId::fromUint64(1U)));
  assert(rebindComposition.ownerPump());
  const auto reboundReceipt = rebindComposition.resourceReceipt(
      canvas::foundation::ObjectId::fromUint64(1U));
  assert(reboundReceipt.has_value());
  assert(reboundReceipt->token.documentBinding == 12U);
  assert(reboundReceipt->changed);
  sameResource.entries.clear();
  sameResource.manifest_revision = 2U;
  assert(rebindComposition.bindManifest(sameResource, 13U));
  assert(!rebindComposition.resourceReceipt(canvas::foundation::ObjectId::fromUint64(1U)).has_value());

  assert(composition.loseSurface());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kLost);
  assert(!composition.notifyResource(canvas::foundation::ObjectId::fromUint64(7U)));
  assert(composition.rebindSurface());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kReady);
  assert(composition.stop());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kQuiescent);
  assert(composition.detach());
  assert(composition.close());
  assert(composition.state() == canvas::runtime::RuntimeCompositionState::kClosed);
  assert(!composition.runtime().selectCreationPreset(canvas::interaction::CreationPreset::kRectangle));
  return 0;
}

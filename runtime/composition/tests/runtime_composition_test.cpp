#include "canvas/runtime/runtime_composition.hpp"
#include <cassert>
#include <string>
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
